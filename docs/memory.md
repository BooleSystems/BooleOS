# Memory management

- PMM: physical page bitmap (64 MB)
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
