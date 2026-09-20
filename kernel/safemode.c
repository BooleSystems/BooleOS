// nullos/kernel/safemode.c — Safe Mode (see safemode.h).
//
// PASS 2 STUB: a banner explaining why we are here and one action, "R -
// reboot normally". The complete Safe Mode (numbered menu, tiers 1/2,
// submenus, restricted shell) is built in the following passes of Phase 18-B.
//
// Runs with nothing but the HAL and bootcfg: NO kmalloc, PMM, VMM, scheduler
// or anything that depends on them — it is entered before they exist.

#include "safemode.h"
#include "hal.h"
#include "bootcfg.h"
#include "messages.h"

void safemode_enter(safemode_reason_t reason, uint32_t fail_count) {
    console_clear();
    console_set_color(CONSOLE_WHITE, CONSOLE_RED);
    console_puts(msg(MSG_SAFE_TITLE));
    console_set_color(CONSOLE_LIGHT_GREY, CONSOLE_BLACK);

    if (reason == SAFEMODE_REASON_FAIL_COUNT) {
        console_puts(msg(MSG_SAFE_REASON_COUNT_1));
        console_put_dec(fail_count);
        console_puts(msg(MSG_SAFE_REASON_COUNT_2));
        console_put_dec(BOOTCFG_FAIL_THRESHOLD);
        console_puts(msg(MSG_SAFE_REASON_COUNT_3));
    } else {
        console_puts(msg(MSG_SAFE_REASON_REQUESTED));
        console_put_dec(fail_count);
        console_puts("\n");
    }

    console_puts(msg(MSG_SAFE_STUB_NOTE));
    console_puts(msg(MSG_SAFE_MENU_REBOOT));

    for (;;) {
        int c = input_poll_key();
        if (c == -1) {
            __asm__ volatile ("hlt");   // interrupts are on: the keyboard IRQ wakes us
            continue;
        }
        if (c != 'r' && c != 'R') continue;   // the only action so far; ignore the rest

        bootcfg_set_u32(BOOTCFG_KEY_FAIL_COUNT, 0);
        if (bootcfg_write() < 0)
            console_puts(msg(MSG_SAFE_RESET_FAILED));
        console_puts(msg(MSG_SAFE_REBOOTING));
        power_reboot();   // does not return on success; if it fails it printed why — keep waiting
    }
}
