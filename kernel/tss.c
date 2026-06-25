// nullos/kernel/tss.c
#include "tss.h"
#include <stdint.h>

static tss_entry_t tss_entry;

void tss_init(uint32_t ss0, uint32_t esp0) {
    // Zero out the TSS
    for (uint32_t i = 0; i < sizeof(tss_entry); i++) {
        ((uint8_t*)&tss_entry)[i] = 0;
    }

    tss_entry.ss0 = ss0;
    tss_entry.esp0 = esp0;

    // We must set the stack segment for the user-mode.
    // It's the kernel data segment but with the bottom two bits set (RPL 3).
    tss_entry.cs = 0x08 | 0x3;
    tss_entry.ss = 0x10 | 0x3;
    tss_entry.ds = 0x10 | 0x3;
    tss_entry.es = 0x10 | 0x3;
    tss_entry.fs = 0x10 | 0x3;
    tss_entry.gs = 0x10 | 0x3;

    tss_entry.iomap_base = sizeof(tss_entry);
}

void tss_set_stack(uint32_t ss0, uint32_t esp0) {
    tss_entry.ss0 = ss0;
    tss_entry.esp0 = esp0;
}

uint32_t tss_get_addr(void) {
    return (uint32_t)&tss_entry;
}
