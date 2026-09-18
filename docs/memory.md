# Memory management

- PMM: physical page bitmap, capped at 8192 pages (`PMM_MAX_PAGES`, `kernel/memory/pmm.h`) × 4KB pages = 32 MB tracked, regardless of how much RAM QEMU is actually given (`-m 256M` in `tools/Makefile`'s `run` target) — pages beyond the cap are simply never tracked/allocatable
- VMM: 32-bit paging with 0–8 MB identity map, per-process directories
- Kernel heap: `kmalloc`/`kfree` with first-fit

For the Phase 14 hardening of userland-pointer resolution built on top of the
VMM (`vmm_get_user_phys_from_dir()`, `user_ptr_valid()`, `copy_from_user()`/
`copy_to_user()`), see [security.md](security.md).

## Relevant files

```
kernel/memory/
  pmm.c             Physical Memory Manager
  vmm.c             Virtual Memory Manager
  heap.c            kmalloc/kfree
```
