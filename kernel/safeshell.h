// nullos/kernel/safeshell.h — Safe Mode's restricted shell (Phase 18-B, tier 2).
//
// Built-in commands only (help, ls, cat, pwd, cd, back), each one calling the
// FAT16 functions directly in ring 0: there are no processes here, never
// process_spawn_user/exec, and it does not run ramfs programs even if one has
// the same name. Read/navigation only — no writes, no delete.
//
// Precondition: the PMM, VMM, heap and FAT16 are initialized (safemode.c does
// that on demand before calling in).

#ifndef SAFESHELL_H
#define SAFESHELL_H

// Runs the prompt loop until the user types "back". The current directory is
// kept between calls (it is not reset when you leave and re-enter).
void safeshell_run(void);

#endif // SAFESHELL_H
