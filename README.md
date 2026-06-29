# NullOS

> A bare-metal x86 (32-bit) operating system written from scratch in C99 and x86 Assembly.

```
  _   _       _ _  ___  ____  
 | \ | |_   _| | |/ _ \/ ___| 
 |  \| | | | | | | | | \___ \ 
 | |\  | |_| | | | |_| |___) |
 |_| \_|\__,_|_|_|\___/|____/ 

 NullOS v0.8.0 - Fase 8: exec, Ctrl+C, foreground
```

## Overview

NullOS is an experimental x86 OS written from scratch in C99 and NASM assembly. It boots via GRUB (Multiboot2), runs kernel and user processes with memory isolation, handles syscalls from ring 3 via `int 0x80`, and loads user programs from a flat ramfs image passed as a GRUB module.

## Roadmap

| Fase | Descrição | Status |
|------|-----------|--------|
| **0** | Bootloader (Multiboot2) + VGA text output | ✅ Done |
| **1** | GDT, IDT, PIC, PIT (100 Hz), teclado PS/2 | ✅ Done |
| **2** | PMM (Physical Memory Manager) | ✅ Done |
| **2b** | VMM com paginação + heap (`kmalloc`/`kfree`) | ✅ Done |
| **3a** | Tabela de processos + scheduler cooperativo round-robin | ✅ Done |
| **3b** | Context switch por processo, CR3 por processo, exception handlers | ✅ Done |
| **4** | TSS, ring 3 usermode, syscalls via `int 0x80` | ✅ Done |
| **5** | Multiboot2 module parser, ramfs flat, ELF32 loader, `exec()`, `user/init` | ✅ Done |
| **6** | Retorno de syscalls em `eax`, preempção via IRQ0 (fatia de 10 ticks) | ✅ Done |
| **7** | `SYS_READ`, ringbuffer de teclado, shell interativo em userland | ✅ Done |
| **8** | `SYS_EXEC`, Ctrl+C, foreground PID, copy-from-user | ✅ Done |
| **9** | Mais syscalls (open, mmap…), múltiplos processos do shell, filesystem | ⏳ Planned |

## O que está implementado

### Kernel base
- Boot via GRUB2 com header Multiboot2
- VGA text mode 80×25 com cores
- GDT com segmentos ring 0 e ring 3 (código + dados)
- IDT com handlers para exceções CPU (0–31), IRQs (32–33) e syscall gate (`int 0x80`, DPL=3)
- PIC 8259 remapeado (IRQs 0–15 → vetores 32–47)
- PIT configurado a 100 Hz
- Driver de teclado PS/2

### Memória
- PMM: bitmap de páginas físicas (64 MB)
- VMM: paginação 32-bit com identity map 0–8 MB, diretórios por processo
- Heap do kernel: `kmalloc`/`kfree` com first-fit

### Multitarefa
- Tabela de processos com até 16 entradas
- Scheduler round-robin com preempção via IRQ0 (fatia de 10 ticks = 100ms a 100Hz)
- Context switch em assembly (salva/restaura callee-saved registers via ESP)
- TSS configurado para stack do kernel por processo (SS0:ESP0)
- Processos que nunca chamam yield são expulsos pelo timer automaticamente

### Usermode e syscalls
- `jump_to_usermode` via `iret` com segmentos ring 3 (CS=0x1B, SS=0x23)
- Isolamento via CR3 por processo
- Syscall gate: `int 0x80`, convenção `eax=num, ebx=arg1, ecx=arg2, edx=arg3`

| num | nome | assinatura |
|-----|------|------------|
| 1 | `SYS_WRITE` | `write(fd, buf, len) → bytes` |
| 2 | `SYS_EXIT` | `exit(code) → não retorna` |
| 3 | `SYS_YIELD` | `yield() → 0` |
| 4 | `SYS_GETPID` | `getpid() → pid` |
| 5 | `SYS_READ` | `read(fd, buf, len) → bytes lidos` |
| 6 | `SYS_UPTIME` | `uptime() → ticks (100 Hz)` |
| 7 | `SYS_MEMINFO` | `meminfo(*pmm_pages, *heap_bytes, *nprocs) → 0` |
| 8 | `SYS_PS` | `ps() → 0` (imprime tabela de processos via VGA) |
| 9 | `SYS_KILL` | `kill(pid) → 0 ou -1` |
| 10 | `SYS_EXEC` | `exec(name) → pid ou -1` |

> **Retorno de syscalls:** `isr128` escreve o retorno do `syscall_handler` no slot EAX do frame do `pusha` antes do `popa`, entregando o valor correto em `eax` para o userland após o `iret`. Inline asm do userland deve usar constraints `"=a"`/`"0"` para que o compilador não assuma eax inalterado após o `int $0x80`.

### Carregamento de programas
- Parser de tags Multiboot2 (`multiboot2_find_module`)
- ramfs flat: `[uint32_t n] [entry×n: name[32]+offset+size] [dados...]`
- ELF32 loader: valida magic, itera `PT_LOAD`, aloca páginas físicas, mapeia no CR3 do processo, copia segmentos
- `exec(name)`: busca na ramfs → cria CR3 → `elf_load` → aloca user stack → `scheduler_spawn_user`
- `kmain` chama `exec("shell")` se o GRUB passar um módulo; sobe sem processo de usuário caso contrário

### Shell interativo
- Ringbuffer de teclado (256 chars) no IRQ1; echo removido do handler
- `SYS_READ (fd=0)`: lê da ringbuffer com polling + `scheduler_sleep_current(1)` para não starvar; faz echo e trata backspace
- `user/shell`: loop `> ` → `sys_read` → `run_command`
- Comandos: `help`, `uname`, `fetch`, `ps`, `mem`, `echo <texto>`, `kill <pid>`, `run <prog>`, `clear`, `exit`
- `fetch`: banner ASCII com OS, Arch, Uptime, PMM livre, Heap livre, Procs rodando
- Sem tasks de debug em background — VGA exclusivo do shell

### Execução de programas e controle de foreground
- `SYS_EXEC (10)`: recebe ponteiro virtual do usuário para o nome do programa; `sys_exec` copia a string byte a byte do espaço do usuário via `vmm_get_phys_from_dir(cur->cr3, vaddr)` (identity-map), chama `exec()` do kernel e retorna o PID do novo processo ou -1
- Shell guarda o PID retornado em `foreground_pid`; ao digitar outro comando, `foreground_pid` é resetado
- **Ctrl+C**: IRQ1 detecta scancode `0x1D` (Ctrl press/release) e `0x2E` (C); injeta `0x03` no ringbuffer; `SYS_READ` retorna imediatamente com `buf[0]=0x03` e ecoa `^C\n`; shell chama `sys_kill(foreground_pid)` e reseta o PID

## Estrutura

```
boot/
  boot.asm            Multiboot2 header + _start
  linker.ld           Layout de memória (kernel @ 0x100000)
kernel/
  main.c              kmain: inicialização e loop do scheduler
  gdt.c/asm           Global Descriptor Table
  idt.c               Interrupt Descriptor Table + exception handler
  isr.asm             Stubs de exceção e syscall gate (isr128)
  pic.c               8259 PIC
  timer.c             PIT 100 Hz
  keyboard.c          PS/2 keyboard
  tss.c               Task State Segment
  process.c/h         Tabela de processos
  scheduler.c/h       Round-robin cooperativo
  context_switch.asm  Troca de contexto ESP
  usermode.asm        jump_to_usermode
  syscall.c/h         Dispatcher de syscalls
  multiboot2.h        Parser de tags Multiboot2
  ramfs.c/h           ramfs flat (find por nome)
  elf.c/h             ELF32 loader
  exec.c/h            exec(): ramfs → ELF → spawn
user/
  init.c              processo de usuário simples: SYS_WRITE + SYS_EXIT
  spintest.c          processo sem yield: valida preempção via IRQ0
  shell.c             shell interativo: help/uname/fetch/ps/mem/echo/kill/run/clear/exit
  link.ld             linker script de usuário (entry @ 0x01000000)
  Makefile            compila init.elf, spintest.elf e shell.elf
  memory/
    pmm.c             Physical Memory Manager
    vmm.c             Virtual Memory Manager
    heap.c            kmalloc/kfree
  drivers/
    vga.c             VGA text driver
tools/
  Makefile            Build system (i686-elf-gcc + NASM + grub2-mkrescue)
  grub.cfg            Configuração do GRUB
build/                Artefatos (git-ignored)
```

## Build

```bash
cd tools
make          # gera build/nullos.iso
make run      # lança no QEMU
make clean    # limpa build/
```

**Dependências:** `i686-elf-gcc`, `i686-elf-ld`, `nasm`, `grub2-mkrescue`, `qemu-system-x86_64`

## Usando a ramfs

Para carregar um programa `init`:

1. Compile o programa como ELF32 estático:
   ```bash
   i686-elf-gcc -m32 -nostdlib -static -o init init.c
   ```

2. Crie a imagem ramfs (ferramenta a implementar em `tools/mkramfs`):
   ```
   [uint32_t n_entries=1]
   [name="init\0..." offset=X size=Y]
   [bytes do ELF]
   ```

3. Adicione ao `grub.cfg`:
   ```
   module2 /boot/ramfs.img
   ```

## Specs técnicas

- Arquitetura: x86 32-bit (i686)
- Linguagem: C99 + NASM
- Boot: Multiboot2 via GRUB2
- Toolchain: i686-elf-gcc, i686-elf-ld, NASM

## License

MIT
