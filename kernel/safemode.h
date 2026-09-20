// nullos/kernel/safemode.h — Safe Mode (Phase 18-B).
//
// A recovery environment inside the same kernel binary, entered very early in
// kmain(), before the PMM, VMM, heap, scheduler, syscall layer and exec are
// initialized: it must survive bugs in exactly those. It uses only the HAL
// (console, input, block I/O, power) and the boot config sector.
//
// PASS 2 STUB: this is only a minimal screen with one action. The full
// numbered menu (tier 1 / tier 2, submenus, restricted shell) comes in the
// next passes — see docs/safemode.md.

#ifndef SAFEMODE_H
#define SAFEMODE_H

#include <stdint.h>

typedef enum {
    SAFEMODE_REASON_FAIL_COUNT,   // boot_fail_count reached the threshold
    SAFEMODE_REASON_REQUESTED,    // "safemode" on the boot command line
} safemode_reason_t;

// Runs Safe Mode. Never returns. `fail_count` is the current boot_fail_count,
// shown to the user.
void safemode_enter(safemode_reason_t reason, uint32_t fail_count) __attribute__((noreturn));

#endif // SAFEMODE_H
