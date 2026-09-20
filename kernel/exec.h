#ifndef EXEC_H
#define EXEC_H

#include "process.h"

/*
 * exec - load a program (ELF) and spawn it as a user process.
 *
 * name        : program to run. Looked up like any file open (vfs_open()):
 *               first the ramfs (flat names such as "shell"), then FAT16,
 *               where a relative path resolves against cwd_cluster below.
 *               A FAT16 program is read from disk whole (size from its
 *               directory entry, at most EXEC_MAX_FILE_SIZE) into a
 *               temporary heap buffer, loaded, and the buffer freed.
 * cwd_cluster : starting current directory for the new process (0 =
 *               root). SYS_EXEC (syscall.c) passes the calling
 *               process's own cwd_cluster, so "run"/"edit" launch
 *               into the same directory the caller was in — the same
 *               inheritance fork() already gives a child. Boot-time
 *               callers with no "launcher" process (kmain spawning
 *               the initial shell) pass 0.
 * start_blocked : passed straight through to process_spawn_user() —
 *               0 for the plain path (process is immediately
 *               runnable, unchanged from before pipes existed); 1 for
 *               SYS_EXEC_PIPE, which still needs to seed the new
 *               process's fd_table entries (in syscall.c) before it's
 *               safe to schedule — see process_spawn_user()'s own doc
 *               comment in process.h for why. Callers that pass 1
 *               MUST call process_make_ready() on the returned
 *               process once that seeding is done.
 *
 * Returns a pointer to the new process on success, NULL on failure.
 * ramfs_init() must have been called before exec(); FAT16 programs also
 * need the heap and fat16_init().
 */
process_t *exec(const char *name, uint32_t cwd_cluster, int start_blocked);

#endif
