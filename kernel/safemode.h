// nullos/kernel/safemode.h — Safe Mode (Phase 18-B).
//
// A recovery environment inside the same kernel binary, entered very early in
// kmain(), before the PMM, VMM, heap, scheduler, syscall layer and exec are
// initialized: it must survive bugs in exactly those. It uses only the HAL
// (console, input, block I/O, power) and the boot config sector, and only
// static buffers — no kmalloc.
//
// Pass 3: the tier-1 text UI (numbered menu, reboot submenu, disk info, sector
// hexdump). The restricted shell with file access (tier 2) is pass 4 — see
// docs/safemode.md. Safe Mode never resumes the normal boot in place: you
// leave it only by rebooting.

#ifndef SAFEMODE_H
#define SAFEMODE_H

#include <stdint.h>

typedef enum {
    SAFEMODE_REASON_FAIL_COUNT,   // boot_fail_count reached the threshold
    SAFEMODE_REASON_REQUESTED,    // "safemode" on the boot command line
} safemode_reason_t;

// Runs Safe Mode. Never returns. `fail_count` is the boot_fail_count at entry,
// shown in the header.
void safemode_enter(safemode_reason_t reason, uint32_t fail_count) __attribute__((noreturn));

#endif // SAFEMODE_H
