// nullos/kernel/safemode.h — Safe Mode (Phase 18-B).
//
// A recovery environment inside the same kernel binary, entered very early in
// kmain(), before the PMM, VMM, heap, scheduler, syscall layer and exec are
// initialized: it must survive bugs in exactly those.
//
// Tier 1 (menu, reboot submenu, disk info, sector hexdump) uses only the HAL
// (console, input, block I/O, power), the boot config sector and static
// buffers — no kmalloc. Tier 2 (menu item 5, the restricted shell in
// safeshell.c) initializes the PMM, VMM, heap and FAT16 on demand, only when
// the user chooses it; if that crashes, the failure counter is still at or
// above the limit and the next boot lands in Safe Mode again. See
// docs/safemode.md. Safe Mode never resumes the normal boot in place: you
// leave it only by rebooting.

#ifndef SAFEMODE_H
#define SAFEMODE_H

#include <stdint.h>

typedef enum {
    SAFEMODE_REASON_FAIL_COUNT,   // boot_fail_count reached the threshold
    SAFEMODE_REASON_REQUESTED,    // "safemode" on the boot command line
    SAFEMODE_REASON_CRASH,        // the previous run crashed (crashdump.h)
} safemode_reason_t;

// Runs Safe Mode. Never returns. `fail_count` is the boot_fail_count at entry,
// shown in the header.
void safemode_enter(safemode_reason_t reason, uint32_t fail_count) __attribute__((noreturn));

#endif // SAFEMODE_H
