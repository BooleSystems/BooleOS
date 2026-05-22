# NullOS — Setup e Build (Fase 0)

## O que você vai precisar

### Dependências (Fedora)

```bash
sudo dnf install -y nasm xorriso qemu-system-x86 gdb make
```

### Cross-compiler x86_64-elf-gcc

Esse é o passo chato. O GCC padrão do seu sistema gera binários para Linux (com libc, syscalls do Linux, etc.). O kernel precisa de um compilador que gere ELF puro sem nenhuma dependência.

**Opção mais rápida: Docker**

```bash
# Roda um container com tudo pronto
docker run -it --rm -v $(pwd):/nullos ghcr.io/osdev/osdev-env
cd /nullos/tools && make
```

**Opção manual (crosstool-ng no Fedora):**

```bash
sudo dnf install -y gcc gcc-c++ gperf bison flex texinfo help2man \
                    make diffutils patch ncurses-devel autoconf \
                    automake libtool git wget

# Instala crosstool-ng
git clone https://github.com/crosstool-ng/crosstool-ng
cd crosstool-ng
./bootstrap
./configure --enable-local
make

# Configura para x86_64-elf
./ct-ng x86_64-unknown-elf
./ct-ng build
# (isso leva ~20-30 minutos)

# Adiciona ao PATH
export PATH="$HOME/x-tools/x86_64-unknown-elf/bin:$PATH"
```

**Alternativa mais simples (i686 em vez de x86_64):**

Se quiser começar mais rápido, o OSDev Wiki tem binários prontos de i686-elf-gcc para Linux. A Fase 0 funciona em 32-bit também — é só ajustar o Makefile.

---

## Estrutura de build

```
nullos/
├── boot/
│   ├── boot.asm       <- Entry point Assembly (Multiboot2)
│   └── linker.ld      <- Linker script
├── kernel/
│   ├── main.c         <- kmain()
│   └── drivers/
│       ├── vga.h
│       └── vga.c      <- Driver VGA texto 80x25
└── tools/
    ├── Makefile        <- Sistema de build
    ├── grub.cfg        <- Config do bootloader
    └── setup_env.sh    <- Script de setup
```

## Compilar e rodar

```bash
cd nullos/tools

# Build completo (gera nullos.iso)
make

# Rodar no QEMU
make run

# Rodar com GDB para debug
make debug

# Limpar build
make clean
```

## O que você deve ver no QEMU

```
  _   _       _ _  ___  ____
 | \ | |_   _| | |/ _ \/ ___|
 |  \| | | | | | | | | \___ \
 | |\  | |_| | | | |_| |___) |
 |_| \_|\__,_|_|_|\___/|____/

 NullOS v0.0.1 - Experimental AI-generated OS
 Fase 0: Boot

------------------------------------------------------------
[BOOT] Multiboot2: OK
[MEM]  Kernel em 0x100000, multiboot_info em 0x...
[VGA]  Modo texto 80x25 ativo
------------------------------------------------------------

 Fase 0 concluida. Kernel respondendo.
 Proximos passos: GDT, IDT, interrupts (Fase 1)

 > _
```

## Debug via serial

O QEMU com `-serial stdio` vai imprimir output serial no terminal. Na Fase 1 vamos adicionar um driver serial para debug sem precisar de VGA.

## Próxima fase (Fase 1)

- GDT (Global Descriptor Table) — segmentação de memória
- IDT (Interrupt Descriptor Table) — tabela de interrupções
- PIC (Programmable Interrupt Controller) — remapear IRQs
- Timer IRQ — base para o scheduler no futuro
- Keyboard IRQ — ler teclado de verdade
