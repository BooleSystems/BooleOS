// nullos/kernel/power.h — platform power control (reboot / shutdown).
//
// Arch-neutral signatures on purpose: everything x86-specific (the 8042
// keyboard controller, the PIIX4 power-management registers) stays inside
// power.c, so Phase 18-A can move these behind the HAL without touching
// callers.

#ifndef POWER_H
#define POWER_H

// Resets the machine. Does not return on success; returns -1 (after
// printing why) if the reset had no effect.
int power_reboot(void);

// Powers the machine off. Does not return on success; returns -1 (after
// printing why) if the hardware isn't supported or the request had no
// effect.
int power_shutdown(void);

#endif
