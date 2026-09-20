# nullos/tools/Makefile - Phase 5

CC  = i686-elf-gcc
LD  = i686-elf-ld
ASM = nasm

ROOT    = ..
BUILD   = $(ROOT)/build
ISO_DIR = $(BUILD)/iso

CFLAGS  = -m32 \
           -std=c99 \
           -ffreestanding \
           -fno-stack-protector \
           -fno-builtin \
           -nostdlib \
           -O2 \
           -Wall \
           -Wextra \
           -I$(ROOT)/kernel \
           -I$(ROOT)

ASMFLAGS = -f elf32

LDFLAGS  = -m elf_i386 \
            -T $(ROOT)/boot/linker.ld \
            --no-undefined

ASM_SRCS = $(ROOT)/boot/boot.asm \
            $(ROOT)/kernel/gdt_flush.asm \
            $(ROOT)/kernel/isr.asm \
            $(ROOT)/kernel/context_switch.asm \
            $(ROOT)/kernel/usermode.asm

C_SRCS   = $(ROOT)/kernel/main.c \
            $(ROOT)/kernel/serial.c \
            $(ROOT)/kernel/syscall.c \
            $(ROOT)/kernel/ramfs.c \
            $(ROOT)/kernel/elf.c \
            $(ROOT)/kernel/exec.c \
            $(ROOT)/kernel/gdt.c \
            $(ROOT)/kernel/idt.c \
            $(ROOT)/kernel/pic.c \
            $(ROOT)/kernel/timer.c \
            $(ROOT)/kernel/keyboard.c \
            $(ROOT)/kernel/tss.c \
            $(ROOT)/kernel/process.c \
            $(ROOT)/kernel/scheduler.c \
            $(ROOT)/kernel/pipe.c \
            $(ROOT)/kernel/power.c \
            $(ROOT)/kernel/memory/pmm.c \
            $(ROOT)/kernel/memory/vmm.c \
            $(ROOT)/kernel/memory/heap.c \
            $(ROOT)/kernel/drivers/vga.c \
            $(ROOT)/kernel/drivers/ata.c \
            $(ROOT)/kernel/drivers/pci.c \
            $(ROOT)/kernel/fs/fat16.c \
            $(ROOT)/kernel/fs/vfs.c

ASM_OBJS = $(patsubst $(ROOT)/%.asm, $(BUILD)/%.o, $(ASM_SRCS))
C_OBJS   = $(patsubst $(ROOT)/%.c,   $(BUILD)/%.o, $(C_SRCS))
ALL_OBJS = $(ASM_OBJS) $(C_OBJS)

KERNEL_ELF = $(BUILD)/nullos.elf
KERNEL_ISO = $(BUILD)/nullos.iso
INIT_ELF     = $(BUILD)/user/init.elf
SPINTEST_ELF = $(BUILD)/user/spintest.elf
SHELL_ELF    = $(BUILD)/user/shell.elf
EDIT_ELF     = $(BUILD)/user/edit.elf
FORKTEST_ELF = $(BUILD)/user/forktest.elf
SELFTEST_ELF = $(BUILD)/user/selftest.elf
CAT_ELF      = $(BUILD)/user/cat.elf
RAMFS_IMG    = $(BUILD)/ramfs.img
DISK_IMG     = $(BUILD)/disk.img
GRUB_CFG_IN  = $(ROOT)/tools/grub.cfg.in
GRUB_CFG     = $(BUILD)/grub.cfg
VERSION_H    = $(ROOT)/kernel/version.h

QEMU_FLAGS = -boot d \
              -cdrom $(KERNEL_ISO) \
              -drive file=$(DISK_IMG),format=raw,if=ide \
              -m 256M \
              -serial stdio \
              -no-reboot \
              -no-shutdown \
              -display sdl

.PHONY: all clean run debug dirs user disk

all: dirs user $(KERNEL_ISO) $(DISK_IMG)
	@echo ""
	@echo "  Build OK: $(KERNEL_ISO)"
	@echo "  'make run' to test it"
	@echo ""

dirs:
	@mkdir -p $(BUILD)/boot
	@mkdir -p $(BUILD)/kernel/drivers
	@mkdir -p $(BUILD)/kernel/memory
	@mkdir -p $(BUILD)/kernel/fs
	@mkdir -p $(BUILD)/user

user:
	@echo "  USER user/init.c"
	$(MAKE) -C $(ROOT)/user

$(BUILD)/%.o: $(ROOT)/%.asm
	@echo "  ASM  $<"
	@mkdir -p $(dir $@)
	$(ASM) $(ASMFLAGS) $< -o $@

$(BUILD)/%.o: $(ROOT)/%.c
	@echo "  CC   $<"
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL_ELF): $(ALL_OBJS)
	@echo "  LD   $@"
	$(LD) $(LDFLAGS) -o $@ $(ALL_OBJS)

$(RAMFS_IMG): $(INIT_ELF) $(SPINTEST_ELF) $(SHELL_ELF) $(EDIT_ELF) $(FORKTEST_ELF) $(SELFTEST_ELF) $(CAT_ELF)
	@echo "  RAMFS $@"
	python3 $(ROOT)/tools/make_ramfs.py $@ init=$(INIT_ELF) spintest=$(SPINTEST_ELF) shell=$(SHELL_ELF) edit=$(EDIT_ELF) forktest=$(FORKTEST_ELF) selftest=$(SELFTEST_ELF) cat=$(CAT_ELF)

# Generates the GRUB menu config from the template, substituting the
# version straight out of kernel/version.h — the single source of
# truth for the version string (see CLAUDE.md). Never hand-edit the
# generated file; edit grub.cfg.in (wording) or version.h (version).
$(GRUB_CFG): $(GRUB_CFG_IN) $(VERSION_H)
	@echo "  GEN  $@"
	@mkdir -p $(BUILD)
	@V=$$(sed -n 's/^#define NULLOS_VERSION *"\(.*\)"/\1/p' $(VERSION_H)); \
	sed "s/@NULLOS_VERSION@/$$V/" $(GRUB_CFG_IN) > $@

$(KERNEL_ISO): $(KERNEL_ELF) $(RAMFS_IMG) $(GRUB_CFG)
	@echo "  ISO  $@"
	@mkdir -p $(ISO_DIR)/boot/grub
	@cp $(KERNEL_ELF) $(ISO_DIR)/boot/nullos.elf
	@cp $(RAMFS_IMG)  $(ISO_DIR)/boot/ramfs.img
	@cp $(GRUB_CFG)   $(ISO_DIR)/boot/grub/grub.cfg
	grub2-mkrescue -o $@ $(ISO_DIR)
	@echo "  OK"

$(DISK_IMG):
	@echo "  DISK $@"
	bash $(ROOT)/tools/make_disk.sh $@

disk: $(DISK_IMG)

run: $(KERNEL_ISO) $(DISK_IMG)
	qemu-system-x86_64 $(QEMU_FLAGS)

# GDB debugging, two terminals:
#   terminal 1: make debug        (QEMU starts paused, GDB stub on :1234)
#   terminal 2: gdb $(KERNEL_ELF)
#               (gdb) target remote :1234
#               (gdb) continue / step / break kmain / etc.
# Uses qemu-system-i386, not qemu-system-x86_64, as the root fix: the
# x86_64 binary's gdbstub always reports the 64-bit register set over
# the wire regardless of -cpu, which GDB rejects with "g packet reply
# is too long" — that's a property of the binary, not the emulated
# CPU, so -cpu alone (previously -cpu qemu32) can't fix it. The i386
# binary reports plain i386 as expected. -cpu qemu32 is kept anyway
# for a plain 32-bit CPU with no long-mode extensions. Debug-only:
# `run` keeps using qemu-system-x86_64 as always.
debug: $(KERNEL_ISO) $(DISK_IMG)
	qemu-system-i386 $(QEMU_FLAGS) -cpu qemu32 -s -S

clean:
	@rm -rf $(BUILD)
