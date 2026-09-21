# Memory management

- PMM: physical page bitmap over **0–8 MB only** (`PMM_LIMIT_ADDR = 0x800000`, `PMM_MAX_PAGES` = 2048 pages × 4 KB, `kernel/memory/pmm.c`), regardless of how much RAM QEMU is given (`-m 256M`)
  - **Where the map comes from:** `kmain` calls `boot_get_memory_map()` (HAL, `docs/hal.md`; the Multiboot2 memory-map tag) and passes the regions to `pmm_init(map, nregions)`. Only `BOOT_MEM_USABLE` regions are freed, rounded inward to whole pages, clamped to the limit — so fragmented maps (usable / BIOS-reserved / ACPI / MMIO ranges, QEMU gives ~7) work, and regions above the limit or above 4 GB are ignored. With no map (`nregions <= 0`) it falls back to assuming 1 MB–8 MB usable, with a warning.
  - **Why 8 MB:** the kernel touches a freshly allocated page through its *physical address* (`elf.c` zeroes user pages, `process_fork()` copies them) and only 0–8 MB is identity-mapped. Handing out a page above 8 MB used to end in a kernel page fault; now the PMM simply runs out and `exec`/`fork` report an error. This is a **mitigation, not the fix** — the definitive fix (a temporary-mapping mechanism or a kernel direct map at a high address, then lifting the cap) is Phase 23; see `PROGRESS.md`, "Known technical debt", identity map.
  - **Always reserved** (marked used after the map is applied): the first 1 MB (real-mode area, BIOS, VGA), 1–4 MB (kernel image, IDT, bitmap, page directory/tables), and — by their real addresses, from `kmain` — the ramfs module and the Multiboot2 info structure, so neither can be handed out even if the bootloader placed them outside 1–4 MB.
  - **`[PMM] Total` in the boot log** is therefore the *allocatable* range (8192 KB), not physical RAM. Boot free count on the default setup: 1792 pages usable in 1–8 MB minus 768 reserved (1–4 MB) = **1024 pages (4096 KB)**.
  - Page counting: every page starts used (bitmap all ones, counter = 2048) and regions are released. The counter used to start at 0, which made `pmm_free_pages()` report `total + real free` (the old boot log said `Free: 61440KB` for `Total: 32768KB`).
  - `pmm_init` used to take `mem_upper` and compute `256 + mem_upper / 4` pages (fixing an old `uint32_t` overflow); the number was then clamped to a 32 MB compile-time cap, which is why the old log always said `Total: 32768KB` whatever RAM QEMU had.
- VMM: 32-bit paging with 0–8 MB identity map, per-process directories
  - `vmm_map_page()` / `vmm_map_user_page()` return `int`: 0 on success, `VMM_ERR_RANGE` (-1) or `VMM_ERR_NOMEM` (-2) on failure (`map_page_early()` also reports an exhausted page-table pool). Every caller checks the result: `heap_expand()` frees the page and returns 0, `exec()`'s user-stack loop prints an error and returns 0, `elf_load()` returns -1, `process_fork()` unwinds through its `failed` path; the three `vmm_map_user_page` call sites also free the physical page they just allocated instead of leaking it
  - `vmm_map_user_page()` rejects `virt < 0x800000` (`VMM_ERR_RANGE`): the first 8 MB is the kernel identity map shared by every process's page directory, so a user mapping there would alter it for all of them
- Kernel heap: `kmalloc`/`kfree` with first-fit
  - **The heap's virtual addresses are the physical ones** (4–8 MB is the identity-mapped range, and the kernel reaches page directories, page tables and user pages there by physical address). `heap_expand()` therefore takes exactly the physical page at `heap_end` (`pmm_alloc_page_at()`), never "the lowest free page" — mapping `heap_end` to some other page silently repoints the identity view of the physical page that lives at that address, and if a process's page directory is there the kernel faults on garbage (this is what crashed the first `run hello.elf`, when `exec()` grew the heap after a process already owned the pages right after it).
  - Because the pool 4–8 MB is shared with processes, the heap is grown to **256 KB up front** in `heap_init()`, while those pages are still free; once processes exist the pages after `heap_end` are taken and the heap cannot grow (an allocation that does not fit returns NULL). Fixing this properly (a heap that does not live in the pool the processes draw from) belongs with Phase 23.

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
