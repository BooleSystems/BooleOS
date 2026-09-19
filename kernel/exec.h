#ifndef EXEC_H
#define EXEC_H

#include "process.h"

/*
 * exec - load a program from the ramfs and spawn it as a user process.
 *
 * name        : filename to look up in the ramfs (e.g. "init")
 * cwd_cluster : starting current directory for the new process (0 =
 *               root). SYS_EXEC (syscall.c) passes the calling
 *               process's own cwd_cluster, so "run"/"edit" launch
 *               into the same directory the caller was in — the same
 *               inheritance fork() already gives a child. Boot-time
 *               callers with no "launcher" process (kmain spawning
 *               the initial shell) pass 0.
 *
 * Returns a pointer to the new process on success, NULL on failure.
 * ramfs_init() must have been called before exec().
 */
process_t *exec(const char *name, uint32_t cwd_cluster);

#endif
