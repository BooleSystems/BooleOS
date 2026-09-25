// booleos/kernel/version.h
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
#ifndef BOOLEOS_VERSION_H
#define BOOLEOS_VERSION_H

#define BOOLEOS_VERSION      "0.20.0"
#define BOOLEOS_PHASE        "20"
#define BOOLEOS_PHASE_DESC   "Crash handler leads into Safe Mode"

// Composed strings so callers don't have to concatenate these by hand.
#define BOOLEOS_BANNER       "BooleOS v" BOOLEOS_VERSION " - Phase " BOOLEOS_PHASE ": " BOOLEOS_PHASE_DESC
#define BOOLEOS_SHORT_BANNER "BooleOS v" BOOLEOS_VERSION

#endif // BOOLEOS_VERSION_H
