# Processes, scheduler, and fork()

## Multitasking
- Process table with up to 16 entries
- Round-robin scheduler with preemption via IRQ0 (10-tick slice = 100ms at 100Hz)
- Context switch in assembly (saves/restores callee-saved registers via ESP)
- TSS configured for a per-process kernel stack (SS0:ESP0)
- Processes that never call yield are preempted by the timer automatically

## Program execution and foreground control
- `SYS_EXEC (10)`: receives a user virtual pointer to the program name; `sys_exec` copies the string byte by byte from user space via `vmm_get_phys_from_dir(cur->cr3, vaddr)` (identity-map), calls the kernel's `exec()`, and returns the new process's PID or -1
- The shell stores the returned PID in `foreground_pid`; typing another command resets `foreground_pid`
- **Ctrl+C**: IRQ1 detects scancode `0x1D` (Ctrl press/release) and `0x2E` (C); injects `0x03` into the ringbuffer; `SYS_READ` returns immediately with `buf[0]=0x03` and echoes `^C\n`; the shell calls `sys_kill(foreground_pid)` and resets the PID

## `fork()`

- Full duplication (not copy-on-write): a fresh page directory, a fresh physical page for every page the parent has mapped (code, data, stack — whatever's actually present, not a fixed list of regions), and a duplicated fd table entry for every open file
- `process_fork()` (`process.c`) does the heavy lifting:
  1. Atomically claims a free process-table slot under `cli`/`sti` (assigns the pid and sets `PROCESS_BLOCKED` — reserved but not runnable — in one uninterruptible step), so two `fork()`s interleaved by the preemptive timer can't pick the same slot
  2. Walks the parent's page directory beyond the two shared kernel PDEs, allocating a new physical page and calling `vmm_map_user_page()` (the same helper `exec()` uses) for every present mapping
  3. Fabricates the child's kernel stack: the 13-word frame captured from the parent's syscall entry (8 `pusha` registers + the CPU's ring3→ring0 trap frame), with `eax` forced to 0, preceded by the same 4-dummy-words-plus-return-address prologue `build_initial_stack()` uses for brand-new processes — except the return address is `isr128_resume` (a new label in `isr.asm`, right before the existing `popa`/`iret`) instead of a bootstrap function
  4. Only then flips the child to `PROCESS_READY`
- The first time the scheduler runs the child, `context_switch()`'s `ret` lands on `isr128_resume`, which does `popa; iret` off that fabricated frame — resuming in ring 3 at the exact instruction after the parent's `fork()` call, with `eax=0`
- `g_syscall_frame` (`syscall.c`): set by `isr128` right after `pusha` to a pointer at the 13-word frame above; `sys_fork()` copies it into a **local** buffer as its very first action, before anything that could be preempted, and nothing else ever reads the global again for that call — otherwise a different process's syscall entry could overwrite it mid-copy and corrupt the child being built
- `SYS_FORK (25)`: after `process_fork()` succeeds, duplicates `fd_table[parent_slot]` into `fd_table[child_slot]` (`vfs_fd_t` is plain data with no owned/shared resource, so a shallow copy is safe — each fd then tracks its own read/write position independently, not POSIX's shared-offset semantics)
- Slot exhaustion or an out-of-memory mid-copy both return -1 to the parent with no process left behind; a partial address-space copy is unwound by walking the child's own (partially built) directory and freeing its data pages, since `process_fork()` runs on the caller's small fixed-size kernel stack and can't afford a separate allocation-tracking array
- Known, explicitly out-of-scope limitations (documented in code comments, not fixed here): `process_spawn()`/`process_spawn_user()` still claim a free slot without `cli`/`sti` (same class of race `process_fork()` closes for itself); `process_exit()` never frees a process's `cr3` or its mapped pages, forked children included; the page-copy assumes physical pages fall in the identity-mapped first 8MB, the same assumption `elf_load()` already makes
- `exec()`, `elf.c`, and `scheduler_yield()`/`scheduler_block_current()` are unchanged — `fork()` reuses the existing scheduler rather than adding a parallel path
- `user/forktest.c`: calls `fork()` and prints "I'm the parent, child=PID" or "I'm the child, pid=PID" depending on the return value

## Relevant files

```
kernel/
  process.c/h         Process table + process_fork()
  scheduler.c/h       Cooperative round-robin
  context_switch.asm  ESP context switch
  usermode.asm        jump_to_usermode
  tss.c               Task State Segment
user/
  forktest.c          calls fork(), prints the parent/child paths and PIDs
```
