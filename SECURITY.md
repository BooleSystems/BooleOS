# Security policy

BooleOS is a hobby operating system, pre-1.0, written by one maintainer with AI assistance. It runs in an emulator and has no production users, but memory-safety bugs in it are real bugs and reports are welcome.

## Supported versions

Only the latest released tag gets security fixes. Today that is `v0.21.0`. This line is updated whenever a new version is released.

`main` and `nightly` are development branches and come with no guarantee. There is no long-term-support table: an older release is not patched, upgrade to the latest tag.

## Reporting a vulnerability

Send the report by email to `theshannondev@gmail.com`. Please do not open a public issue for a vulnerability.

Useful things to include: the version or commit, what you did, what you expected and what happened, and a way to reproduce it (a program, a shell session, a QEMU command line).

This is a hobby project. There is no formal response time and no bug bounty, and replies are best effort. The maintainer does commit to confirming that the report arrived and to not ignoring it.

## Technical notes

How userland pointers are validated before the kernel touches them, the memory-safety bugs fixed in Phase 14, and the rule that the ELF loader does not trust the file (Phase 19) are described in [docs/security.md](docs/security.md). They are not repeated here.

## Phase 21: Copy-on-write fork() hardening

Since Phase 21, `fork()` shares the parent's user pages with the child instead of copying them (see [docs/memory.md](docs/memory.md)). Sharing memory between processes puts the isolation guarantee at risk in three ways, and each has its own mechanism.

### A frame is freed only when no process maps it

If a shared frame went back to the free pool while another process still mapped it, the next allocation could hand the same frame to a third process, and two processes would read and write each other's memory. The PMM (`kernel/memory/pmm.c`) therefore keeps a reference count per physical page, `pmm_refcount[]`.

- Every allocation starts at 1, `pmm_page_ref()` adds one, and `pmm_free_page()` drops one and returns the frame to the pool only when the count reaches 0. Exit, a failed `fork()` and the copy-on-write copy all release pages through it.
- The count is a `uint16_t`. It is bounded by `PROCESS_MAX` (16), since a process maps a page at most once, and a compile-time check fails the build if `PROCESS_MAX` ever reaches 65535. `pmm_page_ref()` also refuses to go past the limit, and `fork()` then fails instead of wrapping the count.
- Allocation, reference and free run with interrupts off, so a timer tick cannot interleave two updates of the same count. The `fork()` walk, `vmm_cow_break()` and the release in `process_exit()` are interrupt-off sections too.
- `process_exit()` releases the exiting process's pages and clears `cr3` in the same interrupt-off section, so the scheduler cannot switch to it with a half-released address space and two exits of one process cannot release the pages twice.

### The kernel breaks copy-on-write before it writes user memory

The kernel writes user memory through the physical address, not through the process's page tables. A copy-on-write page is read-only in the page table, but that does not stop such a write: it would land in the frame the other process still maps and change that process's memory. Two paths write into user buffers, and both go through `user_kptr_write()` (`kernel/syscall.c`), which calls `vmm_cow_break()` first:

- `copy_to_user()`, used by `SYS_READ`, `SYS_MEMINFO`, `SYS_PIPE`, `SYS_GETCWD` and the other syscalls that return data through a pointer;
- `SYS_GETARG`, which copies the argument string byte by byte.

`vmm_cow_break()` gives the caller a private copy, or the original frame back when it is the last owner, before a single byte is written. Reads (`copy_from_user()`, `user_kptr()`) need no break. The rule for new code: any syscall that writes into a user buffer must use `copy_to_user()` or `user_kptr_write()`, never `user_kptr()`.

### CR0.WP

`vmm_init()` sets `CR0.WP` together with `CR0.PG`. With WP clear, ring 0 ignores the read-only bit, so a kernel write to a user virtual address of a copy-on-write page would succeed silently and write into the shared frame. With WP set the same write faults, and the page-fault handler in `kernel/idt.c` resolves it as an ordinary copy-on-write fault. This covers writes through a virtual address. It does not cover a write through a physical address, which never goes through the page tables; that case is what the previous section handles.

### Known limits

- A copy-on-write fault with no free frame for the copy is not turned into an error for that process: there is no per-process fault isolation yet, so it takes the fatal path, which saves a crash record and resets the machine. A process that exhausts memory can therefore reset the system. Per-process fault isolation is planned for Phase 29.
- The page directory and the page tables of an exited process are not freed yet (Phase 23-B). That leaks a few frames per process but does not share anything.
- `SYS_PAGEREF` returns the reference count of a page in the caller's own address space, for tests. It returns no physical address, and only says whether the caller's own page is shared with another process.

### Relevant files

```
kernel/
  memory/pmm.c/h      per-page reference count: pmm_page_ref(), pmm_page_refcount(), pmm_free_page()
  memory/vmm.c/h      VMM_COW, vmm_cow_break(), vmm_get_user_pte(), CR0.WP in vmm_init()
  process.c           process_fork() (sharing pass), release_user_pages(), process_exit()
  idt.c               exception_handler(): copy-on-write fault check before the fatal path
  syscall.c           user_kptr_write(), copy_to_user(), sys_getarg(), sys_pageref()
  irq.h               irq_save()/irq_restore()
user/
  selftest.c          tests 22 to 24: private writes, three generations, preemption
```
