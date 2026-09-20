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
            $(ROOT)/kernel/hal.c \
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
              -no-shutdown \
              -display sdl

# `run` keeps -no-reboot on purpose (post-mortem state on a triple fault:
# QEMU exits instead of silently rebooting). That flag turns a guest reset
# into a shutdown too, so `reboot` looks like `shutdown` there. Use
# `run-reboot-test` to test a real reboot.
QEMU_FLAGS_RUN = $(QEMU_FLAGS) -no-reboot

.PHONY: all clean run run-reboot-test debug dirs user disk inject

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

# make inject FILE=path/to/algo.elf [NAME=algo.elf]
# Copies an arbitrary file into the root of build/disk.img with mcopy
# (mtools, the same host tool make_disk.sh uses), without rebuilding the
# ISO. HOST-SIDE INFRASTRUCTURE ONLY: it does NOT mean the kernel can
# exec() programs from FAT16 (that arrives with Phase 19; today exec()
# only loads from the ramfs). Its use for now is faster disk-related
# testing, e.g. dropping a test file on the disk without touch/edit inside
# QEMU. Notes: FILE is resolved from tools/ (where make runs) unless
# absolute; the kernel only understands 8.3 names, so keep NAME short
# (8 chars + 3-char extension); do not inject while QEMU has the image open.
inject: $(DISK_IMG)
	@test -n "$(FILE)" || { echo "usage: make inject FILE=path/to/file [NAME=name.ext]"; exit 1; }
	@test -f "$(FILE)" || { echo "inject: no such file: $(FILE)"; exit 1; }
	mcopy -o -i $(DISK_IMG) "$(FILE)" "::/$(if $(NAME),$(NAME),$(notdir $(FILE)))"
	@echo "  INJECT $(FILE) -> $(DISK_IMG)::/$(if $(NAME),$(NAME),$(notdir $(FILE)))"

run: $(KERNEL_ISO) $(DISK_IMG)
	qemu-system-x86_64 $(QEMU_FLAGS_RUN)

run-reboot-test: $(KERNEL_ISO) $(DISK_IMG)
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
	qemu-system-i386 $(QEMU_FLAGS_RUN) -cpu qemu32 -s -S

clean:
	@rm -rf $(BUILD)
