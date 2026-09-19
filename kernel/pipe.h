/* nullos/kernel/pipe.h — in-kernel pipe buffers for inter-process
   communication (Phase 16). See docs/pipes.md for the full design:
   the fixed static pool (no kmalloc, nothing to leak), the
   single-waiter-per-direction scope limit, and the symmetric
   EOF/broken-pipe protocol. */
#ifndef PIPE_H
#define PIPE_H

#include <stdint.h>

#define PIPE_MAX      8
#define PIPE_BUF_SIZE 512

/* Allocates a free pipe slot: a fresh circular buffer, read_refs=1,
   write_refs=1 (the two vfs_fd_t entries SYS_PIPE is about to create
   for the caller). Fills *out_pipe_idx. Returns 0 on success, -1 if
   the pool is exhausted (PIPE_MAX pipes already in use system-wide) —
   this is a fixed, hobby-OS-sized pool, not a general resource
   allocator. */
int pipe_create(uint32_t *out_pipe_idx);

/* Reads up to len bytes into buf. Blocks (PROCESS_BLOCKED) while the
   pipe is empty and at least one write end is still open (write_refs
   > 0); returns 0 (EOF) immediately, without blocking, once the pipe
   is empty AND no write end remains. Returns as soon as at least one
   byte is available — like a real pipe, this never waits to fill the
   whole requested len. Returns -1 only on a bad pipe_idx. */
int pipe_read(uint32_t pipe_idx, char *buf, uint32_t len);

/* Writes exactly len bytes (never a short write), blocking as needed
   while the buffer is full and at least one read end remains open
   (read_refs > 0). Returns 0 on success, -1 (broken pipe) if the last
   read end closes while this call is blocked waiting for space, or on
   a bad pipe_idx. */
int pipe_write(uint32_t pipe_idx, const char *buf, uint32_t len);

/* Adds one reference to the read/write end respectively — MUST be
   called whenever a vfs_fd_t naming a pipe end is duplicated into a
   second fd_table slot by anything other than SYS_PIPE's own initial
   creation (fork()'s fd_table row copy, or SYS_EXEC_PIPE seeding the
   new process's fd_table). See vfs_dup() in kernel/fs/vfs.h, which is
   the intended single call site for these from outside this file. */
void pipe_add_read_ref(uint32_t pipe_idx);
void pipe_add_write_ref(uint32_t pipe_idx);

/* Releases one reference to the read/write end respectively — called
   by vfs_close() when a pipe-backed fd is closed (including via
   process exit; see sys_exit()'s fix in syscall.c). Wakes the
   opposite side if this drops the corresponding refcount to 0 (so a
   blocked reader sees EOF, or a blocked writer sees broken-pipe,
   instead of hanging forever). Frees the pipe slot once BOTH
   refcounts reach 0. */
void pipe_release_read(uint32_t pipe_idx);
void pipe_release_write(uint32_t pipe_idx);

#endif
