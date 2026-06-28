	// nullos/kernel/main.c
// kmain() - Fase 3: Processes + Scheduler

#include <stdint.h>
#include "drivers/vga.h"
#include "serial.h"
#include "usermode.h"
#include "gdt.h"
#include "idt.h"
#include "pic.h"
#include "timer.h"
#include "keyboard.h"
#include "memory/pmm.h"
#include "memory/vmm.h"
#include "memory/heap.h"
#include "scheduler.h"
#include "multiboot2.h"

#define MULTIBOOT2_MAGIC 0x36d76289

/* -------------------------------------------------------
 * Processo de teste em ring 3 — código x86 embutido
 *
 * Layout de memória virtual do processo:
 *   código : USER_CODE_VIRT  = 0x01000000
 *   stack  : USER_STACK_VIRT = 0x02000000  (top = +PAGE_SIZE)
 *
 * Convenção de syscall: eax=num, ebx=arg1, ecx=arg2, edx=arg3
 *
 * O blob executa, em ordem:
 *   1. SYS_WRITE(1, msg, 18)   — imprime "Hello from ring3!\n"
 *   2. SYS_GETPID()            — resultado ignorado
 *   3. loop: SYS_YIELD()
 *
 * A string "Hello from ring3!\n" (18 bytes) está no offset 38 do blob.
 * Endereço virtual da string = 0x01000000 + 0x26 = 0x01000026.
 * ------------------------------------------------------- */
#define USER_CODE_VIRT  0x01000000U
#define USER_STACK_VIRT 0x02000000U

static const uint8_t user_blob[] = {
    /* offset  0 */ 0xB8, 0x01, 0x00, 0x00, 0x00,  /* mov eax, 1          (SYS_WRITE)  */
    /* offset  5 */ 0xBB, 0x01, 0x00, 0x00, 0x00,  /* mov ebx, 1          (fd=stdout)  */
    /* offset 10 */ 0xB9, 0x26, 0x00, 0x00, 0x01,  /* mov ecx, 0x01000026 (msg addr)   */
    /* offset 15 */ 0xBA, 0x12, 0x00, 0x00, 0x00,  /* mov edx, 18         (msg len)    */
    /* offset 20 */ 0xCD, 0x80,                     /* int 0x80                         */
    /* offset 22 */ 0xB8, 0x04, 0x00, 0x00, 0x00,  /* mov eax, 4          (SYS_GETPID) */
    /* offset 27 */ 0xCD, 0x80,                     /* int 0x80                         */
    /* offset 29 */ 0xB8, 0x03, 0x00, 0x00, 0x00,  /* mov eax, 3  [loop:] (SYS_YIELD)  */
    /* offset 34 */ 0xCD, 0x80,                     /* int 0x80                         */
    /* offset 36 */ 0xEB, 0xF7,                     /* jmp loop  (rel=-9 → offset 29)   */
    /* offset 38 */ 'H','e','l','l','o',' ','f','r','o','m',' ','r','i','n','g','3','!','\n'
};

static process_t *spawn_user_test(void) {
    /* Cria page directory isolado para o processo */
    uint32_t cr3 = vmm_create_directory();
    if (!cr3) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_puts("[USER] falha ao criar directory\n");
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
        return 0;
    }

    /* Aloca página física para código e copia o blob */
    uint32_t code_phys = pmm_alloc_page();
    if (!code_phys) return 0;
    uint8_t *code_page = (uint8_t *)code_phys;  /* identity-mapped (<8MB) */
    for (uint32_t i = 0; i < sizeof(user_blob); i++)
        code_page[i] = user_blob[i];
    vmm_map_user_page(cr3, USER_CODE_VIRT, code_phys);

    /* Aloca página física para stack de usuário */
    uint32_t stack_phys = pmm_alloc_page();
    if (!stack_phys) return 0;
    vmm_map_user_page(cr3, USER_STACK_VIRT, stack_phys);

    uint32_t user_esp = USER_STACK_VIRT + PAGE_SIZE;  /* topo da stack */
    return scheduler_spawn_user("ring3-test", USER_CODE_VIRT, user_esp, cr3);
}

static void print_ok(void) {
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_puts("OK\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
}

static void print_tag(const char *tag) {
    vga_set_color(VGA_WHITE, VGA_BLACK);
    vga_puts(tag);
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
}

static void print_separator(void) {
    vga_set_color(VGA_DARK_GREY, VGA_BLACK);
    for (int i = 0; i < 60; i++) vga_putchar('-');
    vga_puts("\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
}

static void heartbeat_task(void *arg) {
    (void)arg;
    uint32_t beats = 0;

    for (;;) {
        vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
        vga_puts("[task:heartbeat] beat ");
        vga_putdec(++beats);
        vga_puts("\n");
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);

        scheduler_sleep_current(100);
    }
}

static void heap_watch_task(void *arg) {
    (void)arg;
    uint32_t samples = 0;

    for (;;) {
        vga_set_color(VGA_YELLOW, VGA_BLACK);
        vga_puts("[task:heap-watch] sample ");
        vga_putdec(++samples);
        vga_puts("\n");
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
        heap_dump();

        scheduler_sleep_current(250);
    }
}

// Teste basico do kmalloc/kfree
static void __attribute__((unused)) test_heap(void) {
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_puts("\n[TESTE HEAP]\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);

    // Aloca alguns blocos
    void *a = kmalloc(64);
    vga_puts(" kmalloc(64)  = "); vga_puthex((uint32_t)a); vga_puts("\n");

    void *b = kmalloc(128);
    vga_puts(" kmalloc(128) = "); vga_puthex((uint32_t)b); vga_puts("\n");

    void *c = kmalloc(256);
    vga_puts(" kmalloc(256) = "); vga_puthex((uint32_t)c); vga_puts("\n");

    heap_dump();

    // Libera e realoca
    vga_puts(" kfree(b)\n");
    kfree(b);

    void *d = kmalloc(64);
    vga_puts(" kmalloc(64)  = "); vga_puthex((uint32_t)d); vga_puts("\n");

    heap_dump();

    // Libera tudo
    kfree(a);
    kfree(c);
    kfree(d);

    vga_puts(" kfree(a, c, d)\n");
    heap_dump();

    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    vga_puts(" Heap OK!\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
}

void kmain(uint32_t multiboot_magic, uint32_t multiboot_info_addr) {
    (void)multiboot_info_addr;
    serial_init();
    vga_init();

    // Banner
    vga_set_color(VGA_CYAN, VGA_BLACK);
    vga_puts("  _   _       _ _  ___  ____  \n");
    vga_puts(" | \\ | |_   _| | |/ _ \\/ ___| \n");
    vga_puts(" |  \\| | | | | | | | | \\___ \\ \n");
    vga_puts(" | |\\  | |_| | | | |_| |___) |\n");
    vga_puts(" |_| \\_|\\__,_|_|_|\\___/|____/ \n\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_puts(" NullOS v0.3.0 - Fase 3: Processes + Scheduler\n\n");

    print_separator();

    // Multiboot
    print_tag("[BOOT] ");
    if (multiboot_magic != MULTIBOOT2_MAGIC) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_puts("Multiboot2 magic invalido!\n");
        goto hang;
    }
    vga_puts("Multiboot2: "); print_ok();

    // Módulos Multiboot2
    {
        uint32_t mod_start = 0, mod_end = 0;
        print_tag("[BOOT] ");
        if (multiboot2_find_module((void *)multiboot_info_addr,
                                   &mod_start, &mod_end)) {
            vga_puts("modulo: 0x");
            vga_puthex(mod_start);
            vga_puts(" - 0x");
            vga_puthex(mod_end);
            vga_puts(" (");
            vga_putdec(mod_end - mod_start);
            vga_puts(" bytes)\n");
        } else {
            vga_puts("nenhum modulo carregado\n");
        }
    }

    // GDT
    print_tag("[GDT]  ");
    vga_puts("Inicializando... ");
    gdt_init();
    print_ok();

    // PIC
    print_tag("[PIC]  ");
    vga_puts("Remapeando IRQs... ");
    pic_init();
    for (int i = 0; i < 16; i++) pic_mask_irq((uint8_t)i);
    print_ok();

    // IDT
    print_tag("[IDT]  ");
    vga_puts("Instalando vetores...\n");
    idt_init();
    print_tag("       ");
    print_ok();

    // Timer
    print_tag("[TIMER]");
    vga_puts("PIT @ 100Hz... ");
    timer_init(100);
    print_ok();

    // Teclado
    print_tag("[KB]   ");
    vga_puts("PS/2 keyboard... ");
    keyboard_init();
    print_ok();

    // Habilita interrupções
    __asm__ volatile ("sti");

    print_separator();

    // PMM
    print_tag("[PMM]  ");
    vga_puts("Inicializando...\n");
    // multiboot_info_addr+8 tem mem_upper em KB (offset padrão Multiboot1-compat)
    // Usamos 64MB como padrão seguro para QEMU
    pmm_init(64 * 1024);  // 64MB
    print_tag("       ");
    print_ok();
    pmm_dump();

    // VMM
    print_tag("[VMM]  ");
    vga_puts("Ativando paginacao...\n");
    vmm_init();
    print_tag("       ");
    print_ok();
    vmm_dump();

    // Heap
    print_tag("[HEAP] ");
    vga_puts("Inicializando kmalloc...\n");
    heap_init();
    print_tag("       ");
    print_ok();

    print_separator();

    print_tag("[SCHED]");
    vga_puts("Inicializando scheduler... ");
    scheduler_init();
    process_t *heartbeat = scheduler_spawn("heartbeat", heartbeat_task, 0);
    process_t *heap_watch = scheduler_spawn("heap-watch", heap_watch_task, 0);
    if (!heartbeat || !heap_watch) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_puts("ERRO ao criar tasks\n");
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
        goto hang;
    }
    process_t *user_proc = spawn_user_test();
    if (!user_proc) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_puts("ERRO ao criar processo de usuario\n");
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
        goto hang;
    }
    print_ok();
    scheduler_dump();

    print_separator();

    // Testa a heap
    //test_heap();

    print_separator();

    // Status final
    vga_set_color(VGA_YELLOW, VGA_BLACK);
    vga_puts("\n Fase 3b iniciada!\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_puts(" PMM + VMM + kmalloc funcionando.\n");
    vga_puts(" Scheduler cooperativo com stacks de kernel por tarefa.\n");
    vga_puts(" Isolamento de processos via CR3 switching e TSS configurado.\n");
    vga_puts(" Proximos passos: ring 3 (user mode) e syscalls.\n\n");

    vga_puts(" Digite algo:\n > ");

    for (;;) {
        scheduler_run_once();
        __asm__ volatile ("sti; hlt");
    }

hang:
    __asm__ volatile ("cli");
    for (;;) __asm__ volatile ("hlt");
}
