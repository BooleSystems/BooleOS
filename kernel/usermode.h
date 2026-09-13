#ifndef USERMODE_H
#define USERMODE_H

#include <stdint.h>

// Jumps to ring 3. Does not return.
// Before calling: tss_set_stack() must be updated with the current
// process's kernel esp0 so that int 0x80 can return to the kernel.
void jump_to_usermode(uint32_t entry, uint32_t user_esp);

#endif
