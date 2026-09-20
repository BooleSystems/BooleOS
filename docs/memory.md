# Memory management

- PMM: physical page bitmap, capped at 8192 pages (`PMM_MAX_PAGES`, `kernel/memory/pmm.h`) × 4KB pages = 32 MB tracked, regardless of how much RAM QEMU is actually given (`-m 256M` in `tools/Makefile`'s `run` target) — pages beyond the cap are simply never tracked/allocatable
  - `pmm_init(mem_upper)` computes the page count as `256 + mem_upper / 4` (`mem_upper` = KB above 1 MB). The old `(1024 + mem_upper) * 1024 / PAGE_SIZE` overflowed `uint32_t` for a huge `mem_upper` and wrapped to a tiny count. Freeing memory above 1 MB is skipped, with a warning, when `total_pages <= 256`
- VMM: 32-bit paging with 0–8 MB identity map, per-process directories
  - `vmm_map_page()` / `vmm_map_user_page()` return `int`: 0 on success, `VMM_ERR_RANGE` (-1) or `VMM_ERR_NOMEM` (-2) on failure (`map_page_early()` also reports an exhausted page-table pool). Every caller checks the result: `heap_expand()` frees the page and returns 0, `exec()`'s user-stack loop prints an error and returns 0, `elf_load()` returns -1, `process_fork()` unwinds through its `failed` path; the three `vmm_map_user_page` call sites also free the physical page they just allocated instead of leaking it
  - `vmm_map_user_page()` rejects `virt < 0x800000` (`VMM_ERR_RANGE`): the first 8 MB is the kernel identity map shared by every process's page directory, so a user mapping there would alter it for all of them
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
