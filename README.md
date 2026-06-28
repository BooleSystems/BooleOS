# NullOS

> A bare-metal x86 (32-bit) operating system written from scratch in C99 and x86 Assembly.

```
  _   _       _ _  ___  ____  
 | \ | |_   _| | |/ _ \/ ___| 
 |  \| | | | | | | | | \___ \ 
 | |\  | |_| | | | |_| |___) |
 |_| \_|\__,_|_|_|\___/|____/ 

 NullOS v0.4.0 - Fase 4: Usermode + Syscalls
```

## Overview

NullOS is an experimental x86 OS written from scratch in C99 and NASM assembly. It boots via GRUB (Multiboot2), runs kernel and user processes with memory isolation, and handles syscalls from ring 3 via `int 0x80`.

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
| **5** | ramfs / initrd para carregar programas de usuário | ⏳ Planned |
| **6** | Preempção (scheduler preemptivo via IRQ0) | ⏳ Planned |
| **7** | Mais syscalls (open, read, write, mmap…) + shell | ⏳ Planned |

## O que está implementado

### Kernel base
- Boot via GRUB2 com header Multiboot2
- VGA text mode 80×25 com cores
- GDT com segmentos ring 0 e ring 3 (código + dados)
- IDT com handlers para exceções CPU (0–31), IRQs (32–33) e syscall gate (int 0x80, DPL=3)
- PIC 8259 remapeado (IRQs 0–15 → vetores 32–47)
- PIT configurado a 100 Hz
- Driver de teclado PS/2

### Memória
- PMM: bitmap de páginas físicas (64 MB)
- VMM: paginação 32-bit com identity map 0–8 MB, diretórios por processo
- Heap do kernel: `kmalloc`/`kfree` com first-fit

### Multitarefa
- Tabela de processos com até 16 entradas
- Scheduler cooperativo round-robin
- Context switch em assembly (salva/restaura callee-saved registers via ESP)
- TSS configurado para stack do kernel por processo (SS0:ESP0)
- Processos kernel com bootstrap e stacks isoladas

### Usermode e syscalls
- `jump_to_usermode` via `iret` com segmentos ring 3 (CS=0x1B, SS=0x23)
- Isolamento via CR3 por processo (page directory próprio com kernel mapeado)
- Syscall gate: `int 0x80`, convenção `eax=num, ebx=arg1, ecx=arg2, edx=arg3`
- Syscalls implementadas:

| num | nome | assinatura |
|-----|------|-----------|
| 1 | `SYS_WRITE` | `write(fd, buf, len) → bytes` |
| 2 | `SYS_EXIT` | `exit(code) → não retorna` |
| 3 | `SYS_YIELD` | `yield() → 0` |
| 4 | `SYS_GETPID` | `getpid() → pid` |

## Estrutura

```
boot/
  boot.asm       Multiboot2 header + _start
  linker.ld      Layout de memória (kernel @ 0x100000)
kernel/
  main.c         kmain: inicialização e loop do scheduler
  gdt.c/asm      Global Descriptor Table
  idt.c          Interrupt Descriptor Table + exception handler
  isr.asm        Stubs de exceção e syscall gate (isr128)
  pic.c          8259 PIC
  timer.c        PIT 100 Hz
  keyboard.c     PS/2 keyboard
  tss.c          Task State Segment
  process.c/h    Tabela de processos
  scheduler.c/h  Round-robin cooperativo
  context_switch.asm  Troca de contexto ESP
  usermode.asm   jump_to_usermode
  syscall.c/h    Dispatcher de syscalls
  memory/
    pmm.c        Physical Memory Manager
    vmm.c        Virtual Memory Manager
    heap.c       kmalloc/kfree
  drivers/
    vga.c        VGA text driver
tools/
  Makefile       Build system (i686-elf-gcc + NASM + grub2-mkrescue)
  grub.cfg       Configuração do GRUB
build/           Artefatos (git-ignored)
```

## Build

```bash
cd tools
make          # gera build/nullos.iso
make run      # lança no QEMU
make clean    # limpa build/
```

**Dependências:** `i686-elf-gcc`, `i686-elf-ld`, `nasm`, `grub2-mkrescue`, `qemu-system-x86_64`

## Specs técnicas

- Arquitetura: x86 32-bit (i686)
- Linguagem: C99 + NASM
- Boot: Multiboot2 via GRUB2
- Toolchain: i686-elf-gcc, i686-elf-ld, NASM

## License

MIT
