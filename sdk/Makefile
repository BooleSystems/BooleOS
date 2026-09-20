# nullos/sdk/Makefile — builds NullOS programs from outside the kernel tree.
#
#   make                  build every *.c in this directory to build/<name>.elf
#   make hello.elf        build just one
#   make inject PROG=hello   copy build/hello.elf into the NullOS disk image
#                         (build/disk.img at the repo root) without rebuilding
#                         the ISO; then in NullOS:  run hello.elf
#   make clean
#
# It reuses the SAME compiler flags, linker script (user/link.ld) and library
# sources (user/lib/) as the programs shipped with the system, so a program built
# here has exactly the format the kernel's loader expects: a 32-bit little-endian
# ELF32 executable linked at 0x01000000 with entry point `_start`. Nothing here
# is duplicated on purpose — if the format ever changes, only user/ changes.
#
# (-I$(ROOT)/kernel is only for kernel/syscall.h: libnos takes the syscall
# numbers from there.) Needs the i686-elf-gcc cross-compiler (docs/setup.md).

ROOT  ?= ..
USER   = $(ROOT)/user
BUILD  = build

CC     = i686-elf-gcc
CFLAGS = -m32 -std=c99 -ffreestanding -fno-stack-protector -fno-builtin \
         -nostdlib -O2 -Wall -Wextra -I$(USER)/lib -I$(ROOT)/kernel

LINK   = $(USER)/link.ld
# libnos: the syscall wrappers + string helpers (always linked); the printf
# family (linked here because the samples use it — drop it if you don't).
LIBNOS   = $(BUILD)/lib/nullos.o
LIBSTDIO = $(BUILD)/lib/nosstdio.o

SRCS  = $(wildcard *.c)
ELFS  = $(patsubst %.c,$(BUILD)/%.elf,$(SRCS))

.PHONY: all clean inject
all: $(ELFS)

$(LIBNOS): $(USER)/lib/nullos.c $(USER)/lib/nullos.h
	@mkdir -p $(BUILD)/lib
	$(CC) $(CFLAGS) -c -o $@ $<

$(LIBSTDIO): $(USER)/lib/nosstdio.c $(USER)/lib/nullos.h
	@mkdir -p $(BUILD)/lib
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/%.elf: %.c $(LINK) $(LIBNOS) $(LIBSTDIO)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -T $(LINK) -o $@ $< $(LIBNOS) $(LIBSTDIO)

# `make hello.elf` as a shortcut for `make build/hello.elf`
%.elf: $(BUILD)/%.elf
	@true

# Copies build/$(PROG).elf into the root of the NullOS disk image with mcopy
# (the tools/ `inject` target). Host-side only; do not run while QEMU has the
# image open. The kernel only understands 8.3 names: keep PROG at 8 characters
# or fewer (hello -> hello.elf).
inject: $(if $(PROG),$(BUILD)/$(PROG).elf)
	@test -n "$(PROG)" || { echo "usage: make inject PROG=<name>   (builds build/<name>.elf first)"; exit 1; }
	$(MAKE) -C $(ROOT)/tools inject FILE=$(CURDIR)/$(BUILD)/$(PROG).elf NAME=$(PROG).elf

clean:
	rm -rf $(BUILD)
