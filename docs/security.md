# Userland pointer validation

- Every syscall that reads or writes through a userland-supplied address (`sys_write`, `sys_read`, `sys_write_file`, `sys_meminfo`) goes through `user_ptr_valid()`/`copy_from_user()`/`copy_to_user()` (`kernel/syscall.c`), which confirm the whole `[addr, addr+len)` range is mapped **and** `VMM_USER` (via `vmm_get_user_phys_from_dir()`) before touching a single byte — a process can no longer point a syscall at the kernel's own identity-mapped memory (heap, page tables, ...) to read or corrupt it
- The same `VMM_USER` check is enforced for filename/argument strings too: `user_kptr()` — the shared byte-resolution helper `copy_user_str()` is built on, used by `sys_open`, `sys_create`, `sys_exec`, and `sys_getarg` — resolves through `vmm_get_user_phys_from_dir()` as well, so those four syscalls got the same fix with no changes of their own

## Phase 14: bugs fixed

Kernel memory-safety hardening closed 4 confirmed ring 3 → ring 0 arbitrary
memory read/write bugs (`sys_write`, `sys_read`, `sys_write_file`,
`sys_meminfo`), a `kmalloc()` integer-overflow bug, and (via the same fix in
`user_kptr()`) the same gap in `sys_open`/`sys_create`/`sys_exec`/
`sys_getarg`. Leftover debug output was also removed from `sys_open`.

The key mechanism is `vmm_get_user_phys_from_dir()` (`kernel/memory/vmm.c`),
deliberately stricter than the pre-existing `vmm_get_phys_from_dir()`: it
also requires `VMM_USER` on both the PDE and PTE, not just "present". This
distinction is the whole fix — every process's page directory clones the
kernel's own PDE0/PDE1 (the identity-mapped first 8MB: kernel heap, page
tables, ...), so that region is always "present" in every process, just
never `VMM_USER`. A validator that only checked "present" (like the one
first tried during this fix) would still treat that shared kernel region as
a legitimate buffer.

See `PROGRESS.md` → "Architecture decisions" for the full narrative of how
`user_kptr()` was found to have the same gap as `user_ptr_valid()`/
`copy_from_user()`/`copy_to_user()`, and CHANGELOG.md `[0.14.0]` for the
release note.

## The ELF loader does not trust the file (Phase 19)

Once `exec()` could load programs from the disk, the image `elf_load()` sees became something a user can write. `elf_load(cr3, data, size, &entry)` therefore gets the file's real size and treats every number in the file as hostile:

- the ELF header must fit, and be a 32-bit little-endian i386 `ET_EXEC`;
- the program header table (`e_phoff` + `e_phnum` × `e_phentsize`, at most 64 entries, entry size at least that of a program header) must lie inside the file;
- for each loadable segment: `p_filesz <= p_memsz`; the file data (`p_offset` + `p_filesz`) must lie inside the file — but only when there *is* file data, since a pure `.bss` segment has `p_filesz == 0` and an offset that the linker places at or past the end of the file; and the virtual range must lie in `[0x00800000, 0x02000000)` (below is the kernel's shared identity map, above is where `exec()` puts the user stack);
- every sum and product is done in 64-bit arithmetic, so a 32-bit field cannot wrap around a check;
- **all** headers are validated before the first page is mapped, so a bad header found half way cannot leave earlier segments mapped.

Nothing is read outside `[data, data + size)`. This closes the old gap "`elf_load` never receives the file's real `file_size`" and the `page_end` overflow near `UINT32_MAX`. It is tested on the host against the real `elf.c` with truncation, header fuzzing and crafted hostile headers — see `make test-elf` in `docs/testing.md`.

## Copy-on-write fork() and memory safety (Phase 21)

Since Phase 21, `fork()` maps the parent's user pages into the child at the same physical frames, read-only, and the first write copies the page (mechanism in [memory.md](memory.md)). Sharing frames between processes could break isolation in three ways. A short version of this section, with the reporting policy, is in the root `SECURITY.md`.

**A frame is freed only when no process maps it.** `kernel/memory/pmm.c` keeps `pmm_refcount[]`, one `uint16_t` per managed page. Every allocation starts at 1, `pmm_page_ref()` adds one, and `pmm_free_page()` drops one and returns the frame to the pool only at 0. If exit, a failed `fork()` or a copy-on-write copy freed a frame that another process still mapped, the next `pmm_alloc_page()` could hand it to a third process, and two processes would read and write each other's memory. Details of the count:

- It is bounded by `PROCESS_MAX` (16), because a process maps a given page at most once. `uint16_t` is far more than that needs, a compile-time check in `pmm.c` fails the build if `PROCESS_MAX` reaches 65535, and `pmm_page_ref()` returns -1 instead of wrapping (`process_fork()` then fails and unwinds).
- A page that is marked used but has a count of 0 was reserved with `pmm_mark_used()` and never allocated. `pmm_free_page()` frees it directly, and `pmm_page_ref()` counts it as having one owner first, so sharing it cannot make it look free.
- Allocation, reference and free run with interrupts off (`irq_save()`/`irq_restore()`, `kernel/irq.h`), so a timer tick cannot interleave two read-modify-writes of the same count. The `fork()` sharing pass, `vmm_cow_break()` and `release_user_pages()` are interrupt-off sections as well, for the same reason.
- `process_exit()` releases the process's pages, clears `cr3` and marks the slot unused in one interrupt-off section. A tick in between could otherwise switch to the process with a released address space, or let a second exit of the same process release the pages again. A slot claimed by `fork()` or a spawn starts with `cr3 = 0`, so killing a half-built process releases nothing.

**The kernel breaks copy-on-write before writing user memory.** The kernel writes user memory through the physical address (the first 8 MB are identity-mapped), not through the process's page tables. A read-only PTE does not stop such a write, so a `copy_to_user()` into a buffer on a shared page would change the frame the other process still maps. `user_kptr_write()` (`kernel/syscall.c`) calls `vmm_cow_break()` for the target address first, then resolves the pointer as `user_kptr()` does. It is used by:

- `copy_to_user()`, which serves `SYS_READ`, `SYS_MEMINFO`, `SYS_PIPE`, `SYS_GETCWD` and the other syscalls that return data through a pointer;
- `SYS_GETARG` (`sys_getarg()`), which writes the argument string one byte at a time.

`vmm_cow_break()` gives the process a private copy, or takes the original frame back writable when it is the last owner. Reading needs no break, so `copy_from_user()`, `copy_user_str()` and `user_kptr()` are unchanged. The rule for new syscalls: a write into a user buffer goes through `copy_to_user()` or `user_kptr_write()`, never through `user_kptr()`.

**`CR0.WP` is set.** `vmm_init()` sets bit 16 of CR0 together with paging. With WP clear, ring 0 ignores the read-only bit, so a kernel write through a user virtual address of a copy-on-write page would succeed silently and land in the shared frame. With WP set, the write faults and `exception_handler()` (`kernel/idt.c`) resolves it as a copy-on-write fault: it acts only on exception 14 with error-code bits 0 (protection) and 1 (write) set, and only for a page marked `VMM_COW`. A write to a read-only page without `VMM_COW` is still fatal. WP covers writes through virtual addresses only. A write through a physical address never touches the page tables, which is why the previous paragraph needs `user_kptr_write()`.

Known limits:

- A copy-on-write fault with no free frame for the copy takes the fatal path (a crash record, then a reset into Safe Mode), because there is no per-process fault isolation yet (Phase 29). A process that exhausts memory can therefore reset the machine.
- The page directory and page tables of an exited process are not freed yet (Phase 23-B). That leaks a few frames per process and shares nothing.
- `SYS_PAGEREF` returns the reference count of a page in the caller's own address space, for the selftest. It returns no physical address.

## Relevant files

```
kernel/
  syscall.c/h         user_ptr_valid(), copy_from_user(), copy_to_user(), user_kptr(), user_kptr_write(), sys_pageref()
  memory/vmm.c        vmm_get_user_phys_from_dir(), vmm_cow_break(), CR0.WP in vmm_init()
  memory/pmm.c/h      per-page reference count (pmm_page_ref(), pmm_free_page())
  process.c           process_fork() sharing pass, release_user_pages(), process_exit()
  idt.c               exception_handler(): copy-on-write fault check
  irq.h               irq_save()/irq_restore()
  elf.c/h             elf_load(): validates the program file against its size
```
