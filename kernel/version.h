// nullos/kernel/version.h
//
// Single source of truth for the OS version. Every place that shows a
// version string — the kernel's boot banner (kernel/main.c), the
// userland shell's "fetch"/"uname" commands (user/shell.c), and the
// GRUB menu entry label (tools/grub.cfg, generated at build time from
// this file — see tools/Makefile) — reads it from here.
//
// This header is plain string/text macros only: no kernel types, no
// function declarations. That's deliberate, so user/*.c (a separate,
// unprivileged compilation unit — see CLAUDE.md) can include it too
// without pulling in anything kernel-only.
//
// When a phase completes, THIS is the only file that needs editing for
// the version to update everywhere. Do not hardcode the version string
// anywhere else.
#ifndef NULLOS_VERSION_H
#define NULLOS_VERSION_H

#define NULLOS_VERSION      "0.17.0"
#define NULLOS_PHASE        "17"
#define NULLOS_PHASE_DESC   "Cleanup A"

// Composed strings so callers don't have to concatenate these by hand.
#define NULLOS_BANNER       "NullOS v" NULLOS_VERSION " - Phase " NULLOS_PHASE ": " NULLOS_PHASE_DESC
#define NULLOS_SHORT_BANNER "NullOS v" NULLOS_VERSION

#endif // NULLOS_VERSION_H
