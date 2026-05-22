# nullos/tools/Makefile
# Build system do NullOS — Fase 0
#
# Dependências necessárias no seu sistema:
#   - x86_64-elf-gcc    (cross-compiler — veja docs/setup.md)
#   - x86_64-elf-ld
#   - nasm
#   - grub-mkrescue
#   - xorriso
#   - qemu-system-x86_64

# ============================================================
# Configuração do cross-compiler
# ============================================================
CC      = x86_64-elf-gcc
LD      = x86_64-elf-ld
ASM     = nasm
OBJCOPY = x86_64-elf-objcopy

# Raiz do projeto (Makefile fica em tools/, então sobe um nível)
ROOT    = ..
BUILD   = $(ROOT)/build
ISO_DIR = $(BUILD)/iso

# ============================================================
# Flags de compilação
# ============================================================

# -m32             : gera código 32-bit (Multiboot2 entra em protected mode 32-bit)
# -ffreestanding   : sem libc, sem runtime padrão
# -fno-stack-protector : sem canary (não temos libc pra suportar)
# -O2              : otimização razoável
# -Wall -Wextra    : warnings
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

# ============================================================
# Fontes
# ============================================================
ASM_SRCS = $(ROOT)/boot/boot.asm

C_SRCS   = $(ROOT)/kernel/main.c \
            $(ROOT)/kernel/drivers/vga.c

# ============================================================
# Objetos
# ============================================================
ASM_OBJS = $(patsubst $(ROOT)/%.asm, $(BUILD)/%.o, $(ASM_SRCS))
C_OBJS   = $(patsubst $(ROOT)/%.c,   $(BUILD)/%.o, $(C_SRCS))
ALL_OBJS = $(ASM_OBJS) $(C_OBJS)

KERNEL_ELF = $(BUILD)/nullos.elf
KERNEL_ISO = $(BUILD)/nullos.iso

# ============================================================
# Targets
# ============================================================
.PHONY: all clean run debug dirs

all: dirs $(KERNEL_ISO)
	@echo ""
	@echo "  Build concluido: $(KERNEL_ISO)"
	@echo "  Rode 'make run' para testar no QEMU"
	@echo ""

dirs:
	@mkdir -p $(BUILD)/boot
	@mkdir -p $(BUILD)/kernel/drivers
	@mkdir -p $(ISO_DIR)/boot/grub

# Compilar Assembly
$(BUILD)/%.o: $(ROOT)/%.asm
	@echo "  ASM  $<"
	@mkdir -p $(dir $@)
	$(ASM) $(ASMFLAGS) $< -o $@

# Compilar C
$(BUILD)/%.o: $(ROOT)/%.c
	@echo "  CC   $<"
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Linkar tudo num ELF
$(KERNEL_ELF): $(ALL_OBJS)
	@echo "  LD   $@"
	$(LD) $(LDFLAGS) -o $@ $(ALL_OBJS)

# Gerar ISO bootável com GRUB
$(KERNEL_ISO): $(KERNEL_ELF)
	@echo "  ISO  $@"
	@cp $(KERNEL_ELF) $(ISO_DIR)/boot/nullos.elf
	@cp $(ROOT)/tools/grub.cfg $(ISO_DIR)/boot/grub/grub.cfg
	grub-mkrescue -o $@ $(ISO_DIR) 2>/dev/null
	@echo "  OK   ISO gerada com sucesso"

# Rodar no QEMU
run: $(KERNEL_ISO)
	@echo "  QEMU nullos.iso"
	qemu-system-x86_64 \
		-cdrom $(KERNEL_ISO) \
		-m 256M \
		-serial stdio \
		-no-reboot \
		-no-shutdown

# Rodar com GDB
debug: $(KERNEL_ISO)
	qemu-system-x86_64 \
		-cdrom $(KERNEL_ISO) \
		-m 256M \
		-serial stdio \
		-no-reboot \
		-no-shutdown \
		-s -S &
	gdb $(KERNEL_ELF) \
		-ex "target remote :1234" \
		-ex "break kmain" \
		-ex "continue"

# Limpar build
clean:
	@echo "  CLEAN"
	@rm -rf $(BUILD)
