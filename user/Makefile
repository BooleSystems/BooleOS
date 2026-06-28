# nullos/user/Makefile

CC     = i686-elf-gcc
LD     = i686-elf-ld
BUILD  = ../build/user

CFLAGS = -m32 \
          -std=c99 \
          -ffreestanding \
          -fno-stack-protector \
          -fno-builtin \
          -nostdlib \
          -O2 \
          -Wall \
          -Wextra

.PHONY: all clean

all: $(BUILD)/init.elf $(BUILD)/spintest.elf

$(BUILD)/init.elf: init.c link.ld
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -T link.ld -o $@ init.c

$(BUILD)/spintest.elf: spintest.c link.ld
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -T link.ld -o $@ spintest.c

clean:
	@rm -rf $(BUILD)
