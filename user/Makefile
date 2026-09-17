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
          -Wextra \
          -I../kernel

.PHONY: all clean

all: $(BUILD)/init.elf $(BUILD)/spintest.elf $(BUILD)/shell.elf $(BUILD)/edit.elf $(BUILD)/forktest.elf $(BUILD)/selftest.elf

$(BUILD)/init.elf: init.c link.ld
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -T link.ld -o $@ init.c

$(BUILD)/spintest.elf: spintest.c link.ld
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -T link.ld -o $@ spintest.c

$(BUILD)/shell.elf: shell.c link.ld ../kernel/version.h
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -T link.ld -o $@ shell.c

$(BUILD)/edit.elf: edit.c link.ld
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -T link.ld -o $@ edit.c

$(BUILD)/forktest.elf: forktest.c link.ld
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -T link.ld -o $@ forktest.c

$(BUILD)/selftest.elf: selftest.c link.ld
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -T link.ld -o $@ selftest.c

clean:
	@rm -rf $(BUILD)
