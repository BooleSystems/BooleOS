// nullos/kernel/crashdump.h — the crash path (Phase 20).
//
// An unhandled CPU exception used to end in a red screen and a `hlt` loop, so
// the machine needed a manual reset and the context of the error was lost.
// Now the exception handler (idt.c) saves a small record into the boot config
// sector (the same LBA 1 as boot_fail_count) and resets the machine; the next
// boot lands in Safe Mode showing the crash.
//
// The save runs inside an exception handler, so it must not depend on anything
// the crash may have corrupted: it uses no heap and no lock, does its disk I/O
// with the POLLING-ONLY sector functions (interrupts are off and the scheduler
// cannot run there), builds the sector in its own static buffer (not the
// stack), and is guarded against re-entry: a second exception while handling
// the first skips the dump and goes straight to the reset.
//
// Keys added to the config sector (see docs/safemode.md):
//   crash_pending  1 = crashed, Safe Mode not acknowledged yet
//                  2 = acknowledged ("Reboot normally" in Safe Mode); cleared
//                      after the next complete normal boot
//   crash_type     the exception vector (14 = page fault)
//   crash_eip      instruction pointer at the fault      (hex)
//   crash_err      the CPU's error code                   (hex)
//   crash_cr2      faulting address, meaningful for #PF   (hex)
//   crash_ticks    timer ticks since boot (100 per second)

#ifndef CRASHDUMP_H
#define CRASHDUMP_H

#include <stdint.h>

typedef struct {
    uint32_t int_no;
    uint32_t eip;
    uint32_t err_code;
    uint32_t cr2;
    uint32_t ticks;
} crash_info_t;

// ── exception-handler side ───────────────────────────────────────────

// Call first thing in the exception handler (interrupts already off). Returns
// 1 the first time; 0 if a crash is ALREADY being handled (a second exception
// while saving or printing) — the caller must then call crash_reset() at once.
int crash_begin(void);

// Saves `info` into the config sector. Returns 1 if it was written, 0 if it
// could not be (no usable config sector, or the disk did not respond).
int crash_save(const crash_info_t *info);

// Resets the machine, never returns: pulses the 8042 reset line
// (power_reboot_request()), waits ~0.5 s counted on the raw PIT, and if the
// machine is still running forces a triple fault (an empty IDT and an
// interrupt).
void crash_reset(void) __attribute__((noreturn));

// Busy-waits about `ms` milliseconds with interrupts off (raw PIT polling), so
// the crash screen stays readable before the reset.
void crash_pause_ms(uint32_t ms);

// ── boot / Safe Mode side ────────────────────────────────────────────

typedef struct {
    uint32_t state;            // 0 none, 1 pending, 2 acknowledged
    crash_info_t info;
} crash_record_t;

// Reads the record from the in-memory config (bootcfg_read() first). Returns
// the state; fills *out when state != 0.
uint32_t crash_record_load(crash_record_t *out);

// "Reboot normally" in Safe Mode: pending (1) becomes acknowledged (2), in
// memory (the caller writes the config). The record is kept so it can still be
// viewed until a complete normal boot follows.
void crash_record_acknowledge(void);

// The first keyboard read of a boot means the system came up: an acknowledged
// record is removed (in memory; the caller writes the config). A pending one is
// left alone. Returns 1 if something was removed.
int crash_record_clear_if_acknowledged(void);

#endif // CRASHDUMP_H
