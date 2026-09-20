// nullos/kernel/messages.c — the message table (see messages.h).

#include "messages.h"

#define MSG_FALLBACK "(?)"

static const char *const g_msgs[] = {
    // kmain boot log
    [MSG_BOOT_OK] = "OK\n",
    [MSG_BOOT_LOGO_1] = "  _   _       _ _  ___  ____  \n",
    [MSG_BOOT_LOGO_2] = " | \\ | |_   _| | |/ _ \\/ ___| \n",
    [MSG_BOOT_LOGO_3] = " |  \\| | | | | | | | | \\___ \\ \n",
    [MSG_BOOT_LOGO_4] = " | |\\  | |_| | | | |_| |___) |\n",
    [MSG_BOOT_LOGO_5] = " |_| \\_|\\__,_|_|_|\\___/|____/ \n\n",
    [MSG_BOOT_INVALID_MULTIBOOT2_MAGIC] = "Invalid Multiboot2 magic!\n",
    [MSG_BOOT_MULTIBOOT2_OK_PREFIX] = "Multiboot2: ",
    [MSG_TAG_BOOT] = "[BOOT] ",
    [MSG_BOOT_RAMFS_MODULE_0X] = "ramfs module: 0x",
    [MSG_BOOT_RAMFS_RANGE_SEP] = " - 0x",
    [MSG_BOOT_RAMFS_SIZE_OPEN] = " (",
    [MSG_BOOT_RAMFS_SIZE_CLOSE] = " bytes)\n",
    [MSG_BOOT_NO_RAMFS_MODULE] = "no module — running without ramfs\n",
    [MSG_TAG_GDT] = "[GDT]  ",
    [MSG_BOOT_GDT_INITIALIZING] = "Initializing... ",
    [MSG_TAG_PIC] = "[PIC]  ",
    [MSG_BOOT_REMAPPING_IRQS] = "Remapping IRQs... ",
    [MSG_TAG_IDT] = "[IDT]  ",
    [MSG_BOOT_INSTALLING_VECTORS] = "Installing vectors...\n",
    [MSG_TAG_TIMER] = "[TIMER]",
    [MSG_BOOT_TIMER_INIT] = "PIT @ 100Hz... ",
    [MSG_TAG_KB] = "[KB]   ",
    [MSG_BOOT_KEYBOARD_INIT] = "PS/2 keyboard... ",
    [MSG_TAG_PMM] = "[PMM]  ",
    [MSG_BOOT_PMM_INITIALIZING] = "Initializing...\n",
    [MSG_TAG_VMM] = "[VMM]  ",
    [MSG_BOOT_ENABLING_PAGING] = "Enabling paging...\n",
    [MSG_TAG_HEAP] = "[HEAP] ",
    [MSG_BOOT_INITIALIZING_KMALLOC] = "Initializing kmalloc...\n",
    [MSG_TAG_SCHED] = "[SCHED]",
    [MSG_BOOT_SCHED_INITIALIZING] = "Initializing scheduler... ",
    [MSG_TAG_ATA] = "[ATA]  ",
    [MSG_BOOT_DETECTING_DISK] = "Detecting disk... ",
    [MSG_BOOT_NO_DISK] = "no disk\n",
    [MSG_TAG_FAT16] = "[FAT16]",
    [MSG_BOOT_FAT16_INITIALIZING] = " Initializing... ",
    [MSG_BOOT_NO_FAT16_DISK] = "no FAT16 disk\n",
    [MSG_TAG_PCI] = "[PCI]  ",
    [MSG_BOOT_SCANNING_BUS] = "Scanning bus... ",
    [MSG_TAG_RAMFS] = "[RAMFS]",
    [MSG_BOOT_MOUNTING_IMAGE] = "Mounting image...\n",
    [MSG_TAG_EXEC] = "[EXEC] ",
    [MSG_BOOT_LOADING_SHELL] = "Loading shell...\n",
    [MSG_BOOT_ERROR_LOADING_SHELL] = "ERROR loading shell\n",
    [MSG_BOOT_SHELL_STARTED] = "\n Shell started!\n",

    // IDT setup and exception handler
    [MSG_IDT_KERNEL_EXCEPTION] = "\n\n*** KERNEL EXCEPTION ***\n",
    [MSG_IDT_EXCEPTION] = "  Exception : ",
    [MSG_IDT_UNKNOWN] = "Unknown",
    [MSG_IDT_EIP] = "  EIP       : ",
    [MSG_IDT_ERR_CODE] = "  Err Code  : ",
    [MSG_IDT_CR2_ADDR] = "  CR2 (addr): ",
    [MSG_IDT_PF_FLAGS] = "  PF flags  : ",
    [MSG_IDT_NOT_PRESENT] = "not-present ",
    [MSG_IDT_PROTECTION] = "protection ",
    [MSG_IDT_READ] = "read ",
    [MSG_IDT_WRITE] = "write ",
    [MSG_IDT_KERNEL] = "kernel",
    [MSG_IDT_USER] = "user",
    [MSG_IDT_1_ZEROING_IDT_AT_0X200000] = " [1] zeroing IDT at 0x200000\n",
    [MSG_IDT_2_EXCEPTION_GATES_0_31] = " [2] exception gates (0-31)\n",
    [MSG_IDT_3_IRQ_GATES_32_33] = " [3] IRQ gates (32-33, 46-47)\n",
    [MSG_IDT_4_SYSCALL_GATE_0X80_DPL] = " [4] syscall gate (0x80, DPL=3)\n",
    [MSG_IDT_5_FLUSH] = " [5] flush\n",
    [MSG_IDT_6_OK] = " [6] ok!\n",

    // CPU exception names (idt.c)
    [MSG_EXC_DE] = "#DE Divide Error",
    [MSG_EXC_DB] = "#DB Debug",
    [MSG_EXC_NMI] = "NMI",
    [MSG_EXC_BP] = "#BP Breakpoint",
    [MSG_EXC_OF] = "#OF Overflow",
    [MSG_EXC_BR] = "#BR Bound Range",
    [MSG_EXC_UD] = "#UD Invalid Opcode",
    [MSG_EXC_NM] = "#NM No FPU",
    [MSG_EXC_DF] = "#DF Double Fault",
    [MSG_EXC_COPROC_OVERRUN] = "Coprocessor Overrun",
    [MSG_EXC_TS] = "#TS Invalid TSS",
    [MSG_EXC_NP] = "#NP Segment Not Present",
    [MSG_EXC_SS] = "#SS Stack Fault",
    [MSG_EXC_GP] = "#GP General Protection",
    [MSG_EXC_PF] = "#PF Page Fault",
    [MSG_EXC_RESERVED] = "Reserved",
    [MSG_EXC_MF] = "#MF x87 Float",
    [MSG_EXC_AC] = "#AC Alignment Check",
    [MSG_EXC_MC] = "#MC Machine Check",
    [MSG_EXC_XM] = "#XM SIMD Float",
    [MSG_EXC_VE] = "#VE Virtualization",

    // kernel heap
    [MSG_HEAP_ERROR_NO_ROOM_TO] = "heap: ERROR no room to expand!\n",
    [MSG_HEAP_ERROR_OUT_OF_PHYSICAL] = "heap: ERROR out of physical pages!\n",
    [MSG_HEAP_ERROR_OUT_OF_PAGE] = "heap: ERROR out of page tables!\n",
    [MSG_HEAP_INITIALIZING_AT] = "   heap: initializing at ",
    [MSG_HEAP_ERROR_INITIALIZING] = "   heap: ERROR initializing!\n",
    [MSG_HEAP_INITIAL_BLOCK_SIZE] = "   heap: initial block size=",
    [MSG_HEAP_BYTES_NL] = " bytes\n",
    [MSG_HEAP_CORRUPTION_DETECTED] = "heap: CORRUPTION DETECTED!\n",
    [MSG_HEAP_KFREE_INVALID_POINTER] = "kfree: ERROR invalid pointer or double free!\n",
    [MSG_HEAP_BLOCKS] = "Blocks: ",
    [MSG_HEAP_FREE_SEP] = " | Free: ",
    [MSG_HEAP_B_USED_SEP] = "B | Used: ",
    [MSG_HEAP_B_NL] = "B\n",

    // physical memory manager
    [MSG_PMM_1_BITMAP_AT] = "   pmm: [1] bitmap at ",
    [MSG_PMM_2_FREEING_HIGH_MEM] = "   pmm: [2] freeing high mem\n",
    [MSG_PMM_WARNING_NO_MEMORY_ABOVE] = "   pmm: WARNING no memory above 1MB to free\n",
    [MSG_PMM_3_MARKING_USED_REGIONS] = "   pmm: [3] marking used regions\n",
    [MSG_PMM_4_FREE] = "   pmm: [4] free=",
    [MSG_PMM_PAGES_NL] = " pages\n",
    [MSG_PMM_DUMP_TAG] = "[PMM] ",
    [MSG_PMM_TOTAL] = "Total: ",
    [MSG_PMM_KB_FREE] = "KB Free: ",
    [MSG_PMM_KB_NL] = "KB\n",

    // virtual memory manager
    [MSG_VMM_1_ZEROING_PD_AND] = "   vmm: [1] zeroing PD and PTs\n",
    [MSG_VMM_2_MAPPING_0_4MB] = "   vmm: [2] mapping 0-4MB\n",
    [MSG_VMM_3_MAPPING_4MB_8MB] = "   vmm: [3] mapping 4MB-8MB\n",
    [MSG_VMM_4_CR3_CR0_PG] = "   vmm: [4] CR3 + CR0.PG\n",
    [MSG_VMM_5_OK] = "   vmm: [5] ok!\n",
    [MSG_VMM_DUMP_TAG] = "[VMM] ",
    [MSG_VMM_PTS_USED] = "PTs used: ",

    // exec()
    [MSG_EXEC_NOT_FOUND] = "[EXEC] not found: ",
    [MSG_EXEC_FAILED_TO_CREATE_PAGE] = "[EXEC] failed to create page directory\n",
    [MSG_EXEC_ELF_LOAD_FAILED] = "[EXEC] elf_load failed: ",
    [MSG_EXEC_OUT_OF_MEMORY_FOR] = "[EXEC] out of memory for user stack\n",
    [MSG_EXEC_FAILED_TO_MAP_USER] = "[EXEC] failed to map user stack\n",
    [MSG_EXEC_SCHEDULER_SPAWN_USER_FAILED] = "[EXEC] scheduler_spawn_user failed\n",
    [MSG_EXEC_SPAWNED] = "[EXEC] spawned: ",
    [MSG_EXEC_ENTRY_0X] = " entry=0x",
    [MSG_EXEC_ESP_0X] = " esp=0x",

    // scheduler
    [MSG_SCHED_TAG] = "[SCHED] ",
    [MSG_SCHED_ROUND_ROBIN] = "round-robin cooperative context switching\n",

    // process table
    [MSG_PROC_TAG] = "[PROC] ",
    [MSG_PROC_TABLE_HEADER] = "PID  STATE     RUNS  ESP       NAME\n",
    [MSG_PROC_STATE_UNUSED] = "unused",
    [MSG_PROC_STATE_READY] = "ready",
    [MSG_PROC_STATE_RUNNING] = "running",
    [MSG_PROC_STATE_SLEEP] = "sleep",
    [MSG_PROC_STATE_BLOCKED] = "blocked",
    [MSG_PROC_STATE_ZOMBIE] = "zombie",
    [MSG_PROC_STATE_UNKNOWN] = "?",

    // syscalls
    [MSG_SYS_CTRL_C] = "^C\n",
    [MSG_SYS_LS_NO_SUCH_DIRECTORY] = "ls: no such directory: ",
    [MSG_SYS_RAMFS] = "ramfs:\n",
    [MSG_SYS_FAT16] = "fat16:\n",
    [MSG_SYS_DIR_ENTRY] = "<DIR>\n",
    [MSG_SYS_BYTES_NL] = " B\n",
    [MSG_SYS_NO_FILES] = "(no files)\n",
    [MSG_SYS_UNKNOWN_SYSCALL] = "[SYSCALL] unknown number: ",

    // PCI
    [MSG_PCI_NO_PCI_DEVICES_FOUND] = "  (no PCI devices found)\n",
    [MSG_PCI_VENDOR] = "  vendor=",
    [MSG_PCI_DEVICE] = " device=",
    [MSG_PCI_CLASS] = " class=",
    [MSG_PCI_PROGIF] = " progif=",
    [MSG_PCI_HTYPE] = " htype=",
    [MSG_PCI_BARS] = "        bars:",
    [MSG_PCI_BAR] = " bar",

    // FAT16
    [MSG_FAT16_INVALID_BPB] = "invalid BPB (sectors_per_cluster=0), ",

    // power
    [MSG_POWER_REBOOT_FAILED] = "reboot failed: keyboard-controller reset had no effect\n",
    [MSG_POWER_SHUTDOWN_UNSUPPORTED] = "shutdown not supported on this hardware\n",
    [MSG_POWER_SHUTDOWN_NO_PM_BASE] = "shutdown failed: PM I/O base address is not set\n",
    [MSG_POWER_SHUTDOWN_FAILED_ACPI] = "shutdown failed: ACPI power-off had no effect\n",

};

// Compile-time check: the table must reach exactly MSG_COUNT entries (its
// size is set by the highest designated initializer). Fails to compile
// (negative array size) if an ID was added to the enum without a text at the
// end of the table. C99 has no _Static_assert.
typedef char msg_table_size_check[
    (sizeof(g_msgs) / sizeof(g_msgs[0]) == MSG_COUNT) ? 1 : -1];

const char *msg(msg_id_t id) {
    if ((unsigned)id >= (unsigned)MSG_COUNT) return MSG_FALLBACK;
    const char *s = g_msgs[id];
    return s ? s : MSG_FALLBACK;
}
