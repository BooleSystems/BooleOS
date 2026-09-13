# NullOS

> A bare-metal x86 (32-bit) operating system written from scratch in C99 and x86 Assembly.

```
  _   _       _ _  ___  ____  
 | \ | |_   _| | |/ _ \/ ___| 
 |  \| | | | | | | | | \___ \ 
 | |\  | |_| | | | |_| |___) |
 |_| \_|\__,_|_|_|\___/|____/ 

 NullOS v0.10.0 - Fase 10: disco persistente (ATA PIO + FAT16)
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
| **9** | `SYS_OPEN`, `SYS_CLOSE`, `SYS_READ` para arquivos da ramfs, tabela de fds por processo | ✅ Done |
| **10** | Disco persistente: driver ATA PIO, FAT16 leitura/escrita, `SYS_CREATE`/`SYS_WRITE_FILE`, `touch`, editor salva de verdade | ✅ Done |

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
| 11 | `SYS_OPEN` | `open(name) → fd (≥3) ou -1` |
| 12 | `SYS_CLOSE` | `close(fd) → 0 ou -1` |
| 13 | `SYS_READ_RAW` | `read_raw() → scancode\|(ctrl<<8)` (bloqueante, sem eco) |
| 14 | `SYS_GOTOXY` | `gotoxy(col, row) → 0` |
| 15 | `SYS_CLEAR` | `clear() → 0` |
| 16 | `SYS_GETARG` | `getarg(buf, len) → bytes ou -1` (argumento passado por `SYS_EXEC`) |
| 17 | `SYS_KBD_FLUSH` | `kbd_flush() → 0` (esvazia buffers de teclado) |
| 18 | `SYS_SETCOLOR` | `set_color(fg, bg) → 0` |
| 19 | `SYS_SET_RAW_MODE` | `set_raw_mode(1/0) → 0` (desliga eco do `SYS_READ`) |
| 20 | `SYS_WAIT` | `wait(pid) → 0` (bloqueia até o processo terminar) |
| 21 | `SYS_READDIR` | `readdir() → 0` (lista ramfs + FAT16 via VGA) |
| 22 | `SYS_WRITE_FILE` | `write_file(fd, buf, len) → 0 ou -1` (grava no FAT16) |
| 23 | `SYS_CREATE` | `create(name) → fd (≥3) ou -1` (abre se existir, senão cria vazio no FAT16) |

> `SYS_READ` é polimórfico: fd=0 lê do teclado (bloqueante, com eco e backspace); fd≥3 lê de arquivo aberto via `SYS_OPEN`/`SYS_CREATE`, avança a posição e retorna 0 no EOF.

> `SYS_WRITE` (fd=1/2) escreve na VGA; arquivos usam `SYS_WRITE_FILE` dedicado, que grava no FAT16 via `vfs_write`/`fat16_write_file` — a ramfs continua somente-leitura.

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

### Arquivos: open/read/close na ramfs
- `SYS_OPEN (11)`: copia o nome do espaço do usuário via `user_kptr`, busca na ramfs com `ramfs_find`, aloca o primeiro slot livre na `fd_table[slot_proc][0..7]` e retorna `fd = 3 + idx`; retorna -1 se arquivo não encontrado ou sem slots livres
- `SYS_READ (5)` com fd≥3: lê até `len` bytes de `ramfs_base + offset + pos`, avança `pos`, retorna 0 no EOF — sem bloqueio
- `SYS_CLOSE (12)`: marca o slot como livre
- `fd_table[PROCESS_MAX][8]` — tabela global indexada pelo slot do processo; `sys_exit` zera todos os fds do processo ao encerrar, evitando vazamento de slots
- fds 0/1/2 reservados (stdin/stdout/stderr); arquivos começam em fd=3

### Disco persistente: ATA PIO + FAT16

- `kernel/drivers/ata.c`: driver ATA PIO puro, sem IRQ/DMA — sonda os 4 slots possíveis (primary/secondary × master/slave) enviando `IDENTIFY` (0xEC) e descartando dispositivos ATAPI (assinatura `LBA_MID=0x14`/`LBA_HI=0xEB`); `ata_read_sector`/`ata_write_sector` fazem LBA28 com `READ SECTORS` (0x20) / `WRITE SECTORS` (0x30) + `CACHE FLUSH` (0xE7), esperando `BSY` limpar e `DRQ` setar via polling, no mesmo estilo de espera usado pelo teclado e timer
- `kernel/fs/fat16.c`: lê o BPB do setor 0, cacheia a FAT inteira na heap (`kmalloc`); `fat16_find`/`fat16_readdir` varrem o root directory (nomes 8.3); `fat16_read_at` segue a cadeia de clusters a partir de um offset arbitrário; `fat16_write_file` libera a cadeia antiga, aloca uma nova e reescreve a FAT no disco; `fat16_create` procura uma entrada livre/deletada no root dir e grava um dirent vazio (idempotente — não falha se o arquivo já existe)
- `kernel/fs/vfs.c`: despachante único usado pelas syscalls — `vfs_open` tenta a ramfs (somente leitura) e depois o FAT16; `vfs_create` chama `vfs_open` primeiro e só cria no FAT16 se não encontrar; `vfs_write` recusa gravação em arquivos da ramfs
- `SYS_CREATE (23)`: open-or-create — copia o nome do userland, tenta abrir e, se não existir, cria a entrada no FAT16 e retorna o fd já pronto para escrita
- Boot: `kmain` chama `ata_init()` e `fat16_init()` logo após o scheduler; se não houver disco (ou não for FAT16 válido), o boot segue normalmente e as operações de arquivo em disco retornam -1 sem crashar
- `tools/make_disk.sh` + alvo `make disk`: gera `build/disk.img` (32 MB, FAT16 via `mkfs.vfat`) só se ainda não existir, preservando dados entre builds; `tools/run_qemu`/`make run` anexa o disco como `-drive file=build/disk.img,format=raw,if=ide`
- Shell: `touch <nome>` cria um arquivo vazio (`SYS_CREATE` + `SYS_CLOSE`); `ls` lista ramfs e FAT16 separadamente
- Editor: `load_file` agora cria o arquivo (`SYS_CREATE`) quando ele não existe, mantendo o fd aberto; Ctrl+S grava o buffer inteiro via `SYS_WRITE_FILE` e mostra "salvo" ou "salvo (sem disco)" no rodapé; Ctrl+Q fecha o fd antes de sair

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
  drivers/
    vga.c             VGA text driver
    ata.c/h           driver ATA PIO (polling, LBA28)
  fs/
    fat16.c/h         FAT16 leitura/escrita sobre ATA
    vfs.c/h           despachante ramfs + FAT16
  memory/
    pmm.c             Physical Memory Manager
    vmm.c             Virtual Memory Manager
    heap.c            kmalloc/kfree
user/
  init.c              processo de usuário simples: SYS_WRITE + SYS_EXIT
  spintest.c          processo sem yield: valida preempção via IRQ0
  shell.c             shell interativo: help/uname/fetch/ps/mem/ls/touch/echo/kill/run/clear/exit
  edit.c              editor de texto: abre/cria/salva arquivos no FAT16
  link.ld             linker script de usuário (entry @ 0x01000000)
  Makefile            compila init.elf, spintest.elf, shell.elf e edit.elf
tools/
  Makefile            Build system (i686-elf-gcc + NASM + grub2-mkrescue), alvo `disk`
  grub.cfg            Configuração do GRUB
  make_disk.sh        gera build/disk.img (FAT16, 32 MB) se ainda não existir
build/                Artefatos (git-ignored) — inclui disk.img (persiste entre builds)
```

## Build

```bash
cd tools
make          # gera build/nullos.iso e build/disk.img (só cria o disco se não existir)
make disk     # força a criação de build/disk.img isoladamente
make run      # lança no QEMU com o disco anexado (-drive ...,if=ide)
make clean    # limpa build/ (⚠ apaga também o disk.img — dados persistidos se perdem)
```

**Dependências:** `i686-elf-gcc`, `i686-elf-ld`, `nasm`, `grub2-mkrescue`, `qemu-system-x86_64`, `mkfs.vfat`/`mcopy` (pacotes `dosfstools`/`mtools`, usados por `tools/make_disk.sh`)

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
