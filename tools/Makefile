# nullos/tools/Makefile - Fase 5

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
            $(ROOT)/kernel/memory/pmm.c \
            $(ROOT)/kernel/memory/vmm.c \
            $(ROOT)/kernel/memory/heap.c \
            $(ROOT)/kernel/drivers/vga.c

ASM_OBJS = $(patsubst $(ROOT)/%.asm, $(BUILD)/%.o, $(ASM_SRCS))
C_OBJS   = $(patsubst $(ROOT)/%.c,   $(BUILD)/%.o, $(C_SRCS))
ALL_OBJS = $(ASM_OBJS) $(C_OBJS)

KERNEL_ELF = $(BUILD)/nullos.elf
KERNEL_ISO = $(BUILD)/nullos.iso
INIT_ELF     = $(BUILD)/user/init.elf
SPINTEST_ELF = $(BUILD)/user/spintest.elf
SHELL_ELF    = $(BUILD)/user/shell.elf
EDIT_ELF     = $(BUILD)/user/edit.elf
README_TXT   = $(ROOT)/tools/readme.txt
RAMFS_IMG    = $(BUILD)/ramfs.img

.PHONY: all clean run dirs user

all: dirs user $(KERNEL_ISO)
	@echo ""
	@echo "  Build OK: $(KERNEL_ISO)"
	@echo "  'make run' para testar"
	@echo ""

dirs:
	@mkdir -p $(BUILD)/boot
	@mkdir -p $(BUILD)/kernel/drivers
	@mkdir -p $(BUILD)/kernel/memory
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

$(RAMFS_IMG): $(INIT_ELF) $(SPINTEST_ELF) $(SHELL_ELF) $(EDIT_ELF) $(README_TXT)
	@echo "  RAMFS $@"
	python3 $(ROOT)/tools/make_ramfs.py $@ init=$(INIT_ELF) spintest=$(SPINTEST_ELF) shell=$(SHELL_ELF) edit=$(EDIT_ELF) readme.txt=$(README_TXT)

$(KERNEL_ISO): $(KERNEL_ELF) $(RAMFS_IMG)
	@echo "  ISO  $@"
	@mkdir -p $(ISO_DIR)/boot/grub
	@cp $(KERNEL_ELF) $(ISO_DIR)/boot/nullos.elf
	@cp $(RAMFS_IMG)  $(ISO_DIR)/boot/ramfs.img
	@cp $(ROOT)/tools/grub.cfg $(ISO_DIR)/boot/grub/grub.cfg
	grub2-mkrescue -o $@ $(ISO_DIR)
	@echo "  OK"

run: $(KERNEL_ISO)
	qemu-system-x86_64 \
		-cdrom $(KERNEL_ISO) \
		-m 256M \
		-serial stdio \
		-no-reboot \
		-no-shutdown \
		-display sdl

clean:
	@rm -rf $(BUILD)
