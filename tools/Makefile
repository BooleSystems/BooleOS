# nullos/tools/Makefile - Fase 3

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

.PHONY: all clean run dirs

all: dirs $(KERNEL_ISO)
	@echo ""
	@echo "  Build OK: $(KERNEL_ISO)"
	@echo "  'make run' para testar"
	@echo ""

dirs:
	@mkdir -p $(BUILD)/boot
	@mkdir -p $(BUILD)/kernel/drivers
	@mkdir -p $(BUILD)/kernel/memory

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

$(KERNEL_ISO): $(KERNEL_ELF)
	@echo "  ISO  $@"
	@mkdir -p $(ISO_DIR)/boot/grub
	@cp $(KERNEL_ELF) $(ISO_DIR)/boot/nullos.elf
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
