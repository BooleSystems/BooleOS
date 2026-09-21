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
            $(ROOT)/kernel/bootcfg.c \
            $(ROOT)/kernel/safemode.c \
            $(ROOT)/kernel/crashdump.c \
            $(ROOT)/kernel/safeshell.c \
            $(ROOT)/kernel/messages.c \
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

# Previous-release kernel kept in the repo for the permanent "previous
# release" GRUB entry (see docs/safemode.md). It is the kernel AND the ramfs
# of that release: the old kernel needs its own userland (syscall ABI).
PREV_DIR          = $(ROOT)/tools/prev
PREV_ELF          = $(PREV_DIR)/nullos.elf
PREV_RAMFS        = $(PREV_DIR)/ramfs.img
PREV_VERSION_FILE = $(PREV_DIR)/VERSION
VERSION_STR       = $(shell sed -n 's/^\#define NULLOS_VERSION *"\(.*\)"/\1/p' $(VERSION_H))
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

.PHONY: all clean run run-reboot-test debug dirs user disk inject snapshot test-elf

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

# The user programs are built by the `user` target (a recursive make in user/).
# Without a rule for the .elf files, a target that needs them but is not
# preceded by `user` in the command line (`make clean && make run`) fails with
# "No rule to make target ../build/user/init.elf". `user` is an ORDER-ONLY
# prerequisite (after the |) on purpose: it always runs first, but being phony it
# does not make the .elf files look "changed" — what triggers a ramfs/ISO rebuild
# is still an .elf that user/ actually rewrote.
USER_ELFS = $(INIT_ELF) $(SPINTEST_ELF) $(SHELL_ELF) $(EDIT_ELF) $(FORKTEST_ELF) $(SELFTEST_ELF) $(CAT_ELF)
$(USER_ELFS): | user
	@true

$(RAMFS_IMG): $(INIT_ELF) $(SPINTEST_ELF) $(SHELL_ELF) $(EDIT_ELF) $(FORKTEST_ELF) $(SELFTEST_ELF) $(CAT_ELF)
	@echo "  RAMFS $@"
	python3 $(ROOT)/tools/make_ramfs.py $@ init=$(INIT_ELF) spintest=$(SPINTEST_ELF) shell=$(SHELL_ELF) edit=$(EDIT_ELF) forktest=$(FORKTEST_ELF) selftest=$(SELFTEST_ELF) cat=$(CAT_ELF)

# Generates the GRUB menu config from the template, substituting the
# version straight out of kernel/version.h — the single source of
# truth for the version string (see CLAUDE.md). Never hand-edit the
# generated file; edit grub.cfg.in (wording) or version.h (version).
#
# The "previous release" entry sits between the PREV-BEGIN/PREV-END marker
# lines of the template. It is kept (with @PREV_VERSION@ filled from
# tools/prev/VERSION) only when tools/prev/ holds a complete snapshot, and
# deleted from the generated file otherwise.
$(GRUB_CFG): $(GRUB_CFG_IN) $(VERSION_H) $(wildcard $(PREV_DIR)/*)
	@echo "  GEN  $@"
	@mkdir -p $(BUILD)
	@V="$(VERSION_STR)"; \
	if [ -f "$(PREV_ELF)" ] && [ -f "$(PREV_RAMFS)" ] && [ -f "$(PREV_VERSION_FILE)" ]; then \
	    PV=$$(head -n 1 "$(PREV_VERSION_FILE)" | tr -d '\r\n'); \
	    sed -e "s/@NULLOS_VERSION@/$$V/" -e "s/@PREV_VERSION@/$$PV/" -e '/^## PREV-\(BEGIN\|END\)/d' $(GRUB_CFG_IN) > $@; \
	else \
	    sed -e "s/@NULLOS_VERSION@/$$V/" -e '/^## PREV-BEGIN/,/^## PREV-END/d' $(GRUB_CFG_IN) > $@; \
	fi

$(KERNEL_ISO): $(KERNEL_ELF) $(RAMFS_IMG) $(GRUB_CFG) $(wildcard $(PREV_DIR)/*)
	@echo "  ISO  $@"
	@mkdir -p $(ISO_DIR)/boot/grub
	@cp $(KERNEL_ELF) $(ISO_DIR)/boot/nullos.elf
	@cp $(RAMFS_IMG)  $(ISO_DIR)/boot/ramfs.img
	@cp $(GRUB_CFG)   $(ISO_DIR)/boot/grub/grub.cfg
	@rm -f $(ISO_DIR)/boot/prev-nullos.elf $(ISO_DIR)/boot/prev-ramfs.img
	@if [ -f "$(PREV_ELF)" ] && [ -f "$(PREV_RAMFS)" ]; then \
	    cp "$(PREV_ELF)"   $(ISO_DIR)/boot/prev-nullos.elf; \
	    cp "$(PREV_RAMFS)" $(ISO_DIR)/boot/prev-ramfs.img; \
	fi
	grub2-mkrescue -o $@ $(ISO_DIR)
	@echo "  OK"

$(DISK_IMG):
	@echo "  DISK $@"
	bash $(ROOT)/tools/make_disk.sh $@

disk: $(DISK_IMG)

# make test-elf
# Host-side test of the kernel's ELF loader: builds the real kernel/elf.c
# against stand-in page allocator/mapper functions (tools/test_elf_load.c) and
# runs it on every built user program, plus truncation, header fuzzing and
# crafted hostile headers. Needs the host gcc (Linux). Run it after changing
# elf.c, the linker script or the size of a user program.
test-elf: user
	gcc -std=gnu99 -O1 -Wall -Wextra -Wno-int-to-pointer-cast -I$(ROOT)/kernel \
	    -o $(BUILD)/test_elf_load $(ROOT)/tools/test_elf_load.c $(ROOT)/kernel/elf.c
	$(BUILD)/test_elf_load $(wildcard $(BUILD)/user/*.elf)

# make snapshot
# Records the CURRENT build as the "previous release" for the GRUB menu:
# copies build/nullos.elf and build/ramfs.img to tools/prev/ and writes the
# version to tools/prev/VERSION. The three files are tracked in git on
# purpose. Run it by hand, once, right after a release is tagged (on the
# tagged tree, after `make clean && make`), and commit tools/prev/ — the NEXT
# release's ISO then offers this release as its "previous release" entry. It
# is never run by `all`, `run` or any other target. (The kernel and the ramfs
# are saved together: the old kernel needs its own userland, the syscall ABI
# must match.)
snapshot: $(KERNEL_ELF) $(RAMFS_IMG)
	@mkdir -p $(PREV_DIR)
	@cp $(KERNEL_ELF) $(PREV_ELF)
	@cp $(RAMFS_IMG)  $(PREV_RAMFS)
	@echo "$(VERSION_STR)" > $(PREV_VERSION_FILE)
	@echo "  SNAPSHOT $(VERSION_STR) -> $(PREV_DIR)"

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
