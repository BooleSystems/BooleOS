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

LIBNOS = $(BUILD)/lib/nullos.o
# msg() table: linked only into the programs that print user-facing text
# through it (shell, edit, cat) — every program that links it carries the whole table.
LIBMSG = $(BUILD)/lib/messages.o

.PHONY: all clean

all: $(BUILD)/init.elf $(BUILD)/spintest.elf $(BUILD)/shell.elf $(BUILD)/edit.elf $(BUILD)/forktest.elf $(BUILD)/selftest.elf $(BUILD)/cat.elf

$(LIBNOS): lib/nullos.c lib/nullos.h
	@mkdir -p $(BUILD)/lib
	$(CC) $(CFLAGS) -c -o $@ lib/nullos.c

$(LIBMSG): lib/messages.c lib/messages.h
	@mkdir -p $(BUILD)/lib
	$(CC) $(CFLAGS) -c -o $@ lib/messages.c

$(BUILD)/init.elf: init.c link.ld $(LIBNOS)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -T link.ld -o $@ init.c $(LIBNOS)

$(BUILD)/spintest.elf: spintest.c link.ld $(LIBNOS)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -T link.ld -o $@ spintest.c $(LIBNOS)

$(BUILD)/shell.elf: shell.c link.ld lib/messages.h ../kernel/version.h $(LIBNOS) $(LIBMSG)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -T link.ld -o $@ shell.c $(LIBNOS) $(LIBMSG)

$(BUILD)/edit.elf: edit.c link.ld lib/messages.h $(LIBNOS) $(LIBMSG)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -T link.ld -o $@ edit.c $(LIBNOS) $(LIBMSG)

$(BUILD)/forktest.elf: forktest.c link.ld $(LIBNOS)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -T link.ld -o $@ forktest.c $(LIBNOS)

$(BUILD)/selftest.elf: selftest.c link.ld $(LIBNOS)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -T link.ld -o $@ selftest.c $(LIBNOS)

$(BUILD)/cat.elf: cat.c link.ld lib/messages.h $(LIBNOS) $(LIBMSG)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -T link.ld -o $@ cat.c $(LIBNOS) $(LIBMSG)

clean:
	@rm -rf $(BUILD)
