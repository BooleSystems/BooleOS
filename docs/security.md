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

## Relevant files

```
kernel/
  syscall.c/h         user_ptr_valid(), copy_from_user(), copy_to_user(), user_kptr()
  memory/vmm.c        vmm_get_user_phys_from_dir()
```
