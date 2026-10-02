# Memory management

- PMM: physical page bitmap over **0–8 MB only** (`PMM_LIMIT_ADDR = 0x800000`, `PMM_MAX_PAGES` = 2048 pages × 4 KB, `kernel/memory/pmm.c`), regardless of how much RAM QEMU is given (`-m 256M`)
  - **Where the map comes from:** `kmain` calls `boot_get_memory_map()` (HAL, `docs/hal.md`; the Multiboot2 memory-map tag) and passes the regions to `pmm_init(map, nregions)`. Only `BOOT_MEM_USABLE` regions are freed, rounded inward to whole pages, clamped to the limit — so fragmented maps (usable / BIOS-reserved / ACPI / MMIO ranges, QEMU gives ~7) work, and regions above the limit or above 4 GB are ignored. With no map (`nregions <= 0`) it falls back to assuming 1 MB–8 MB usable, with a warning.
  - **Why 8 MB:** the kernel touches a page through its *physical address* (`elf.c` zeroes user pages, `vmm_cow_break()` copies them) and only 0–8 MB is identity-mapped. Handing out a page above 8 MB used to end in a kernel page fault; now the PMM simply runs out and `exec`/`fork` report an error. This is a **mitigation, not the fix** — the definitive fix (a temporary-mapping mechanism or a kernel direct map at a high address, then lifting the cap) is Phase 23; see `PROGRESS.md`, "Known technical debt", identity map.
  - **Always reserved** (marked used after the map is applied): the first 1 MB (real-mode area, BIOS, VGA), 1–4 MB (kernel image, IDT, bitmap, page directory/tables), and — by their real addresses, from `kmain` — the ramfs module and the Multiboot2 info structure, so neither can be handed out even if the bootloader placed them outside 1–4 MB.
  - **`[PMM] Total` in the boot log** is therefore the *allocatable* range (8192 KB), not physical RAM. Boot free count on the default setup: 1792 pages usable in 1–8 MB minus 768 reserved (1–4 MB) = **1024 pages (4096 KB)**.
  - Page counting: every page starts used (bitmap all ones, counter = 2048) and regions are released. The counter used to start at 0, which made `pmm_free_pages()` report `total + real free` (the old boot log said `Free: 61440KB` for `Total: 32768KB`).
  - `pmm_init` used to take `mem_upper` and compute `256 + mem_upper / 4` pages (fixing an old `uint32_t` overflow); the number was then clamped to a 32 MB compile-time cap, which is why the old log always said `Total: 32768KB` whatever RAM QEMU had.
  - Every page also has a reference count, see "Copy-on-write fork()" below. `pmm_free_page()` drops one reference and only returns the page to the pool when the count reaches 0.
- VMM: 32-bit paging with 0–8 MB identity map, per-process directories
  - `vmm_map_page()` / `vmm_map_user_page()` return `int`: 0 on success, `VMM_ERR_RANGE` (-1) or `VMM_ERR_NOMEM` (-2) on failure (`map_page_early()` also reports an exhausted page-table pool). Every caller checks the result: `heap_expand()` frees the page and returns 0, `exec()`'s user-stack loop prints an error and returns 0, `elf_load()` returns -1, `process_fork()` unwinds through its `failed` path; the `vmm_map_user_page` call sites in `exec()` and `elf_load()` also free the physical page they just allocated instead of leaking it
  - `vmm_map_user_page()` rejects `virt < 0x800000` (`VMM_ERR_RANGE`): the first 8 MB is the kernel identity map shared by every process's page directory, so a user mapping there would alter it for all of them
- Kernel heap: `kmalloc`/`kfree` with first-fit
  - **The heap's virtual addresses are the physical ones** (4–8 MB is the identity-mapped range, and the kernel reaches page directories, page tables and user pages there by physical address). `heap_expand()` therefore takes exactly the physical page at `heap_end` (`pmm_alloc_page_at()`), never "the lowest free page" — mapping `heap_end` to some other page silently repoints the identity view of the physical page that lives at that address, and if a process's page directory is there the kernel faults on garbage (this is what crashed the first `run hello.elf`, when `exec()` grew the heap after a process already owned the pages right after it).
  - Because the pool 4–8 MB is shared with processes, the heap is grown to **256 KB up front** in `heap_init()`, while those pages are still free; once processes exist the pages after `heap_end` are taken and the heap cannot grow (an allocation that does not fit returns NULL). Fixing this properly (a heap that does not live in the pool the processes draw from) belongs with Phase 23.

## Copy-on-write fork()

Since Phase 21, `fork()` does not copy the parent's memory. The parent and the child map the same physical frames read-only, and the first write by either one copies the page it wrote to. The pieces are the `VMM_COW` PTE bit, a reference count per physical page, the page-fault handler, and `CR0.WP`.

### The VMM_COW bit

`VMM_COW` is PTE bit 9 (`0x200`, `kernel/memory/vmm.h`), one of the three bits the CPU leaves for the OS to use. A copy-on-write page always has `VMM_WRITABLE` clear and `VMM_COW` set. That is what tells it apart from a page that is read-only for its own reasons: a write to a read-only page without `VMM_COW` is a genuine fault, and the page-fault handler does not touch it. The handler acts only on pages with the bit set.

### Reference count per physical page

`kernel/memory/pmm.c` keeps `pmm_refcount[]`, one `uint16_t` per managed page (2048 entries, 4 KB in `.bss`), next to the allocation bitmap.

- The count is the number of page tables that map the frame. A user page is mapped at most once per process, so the count cannot exceed `PROCESS_MAX` (16). `uint16_t` is far more than that needs, and a compile-time check in `pmm.c` fails the build if `PROCESS_MAX` ever reaches 65535.
- `pmm_alloc_page()` and `pmm_alloc_page_at()` set the count to 1. `pmm_page_ref()` adds one. `pmm_free_page()` subtracts one and returns the frame to the pool only when the count reaches 0, so a caller that owns a page alone still frees it the way it always did.
- A page with a count of 0 that is marked used was reserved with `pmm_mark_used()` and never allocated. `pmm_free_page()` frees it directly, and `pmm_page_ref()` treats it as having one owner before adding the new one, so sharing it can never make it look free.
- Allocation, reference and free run with interrupts off. A timer tick in the middle of a read-modify-write would lose an update, and a fork or a copy-on-write break in another process changes the same counts.
- `pmm_page_refcount(addr)` reads the count, and `SYS_PAGEREF` exposes it to tests (see [syscalls.md](syscalls.md)).

### What fork() does per page

`process_fork()` (`kernel/process.c`) walks every present user PTE of the parent (PDE 2 and up; PDE 0 and 1 are the shared kernel identity map) and, for each one:

1. Calls `pmm_page_ref()` on the frame.
2. Maps the same frame into the child with `vmm_map_user_page_flags()`, with `VMM_WRITABLE` cleared and `VMM_COW` set if the parent's PTE was writable. A page that was already read-only keeps its flags, and one that is already `VMM_COW` (a process forking again) only gains another reference.
3. Rewrites the parent's PTE with the same flags and runs `invlpg` on that address right away when the parent's directory is the active one. There is no batch flush at the end.

The whole walk runs with interrupts off (`irq_save()`/`irq_restore()`, `kernel/irq.h`). A tick in the middle would let the parent run with some PTEs converted and some not, or with a count one short of its mappings. Nothing in the walk blocks, so the section is bounded by the size of the address space.

If the walk fails (`pmm_page_ref()` or the mapping returns an error), the child's references are dropped and its slot is released. Pages the parent had already turned read-only stay `VMM_COW` with a count of 1, and its next write takes each one back writable in place.

### What the page-fault handler does

`exception_handler()` (`kernel/idt.c`) checks first for a write to a present page: exception 14 with error-code bits 0 and 1 set. It calls `vmm_cow_break()` for the faulting address (CR2) in the active directory. If that returns 1, the handler returns and the CPU retries the write. Everything else, and a copy-on-write fault with no memory left for the copy, goes to the fatal path that saves a crash record and resets into Safe Mode.

`vmm_cow_break(pd, virt)` (`kernel/memory/vmm.c`), with interrupts off for the whole call:

1. Looks up the user PTE. If it is missing or does not have `VMM_COW`, it returns 0 and changes nothing.
2. If the frame's count is 1 or less, this process is the last owner: the PTE becomes writable and loses `VMM_COW`, and no copy is made.
3. Otherwise it allocates a new frame, copies the 4 KB, and points the PTE at the new frame, writable and without `VMM_COW`. It returns -1 if there is no free frame.
4. Runs `invlpg` for the address if `pd` is the active directory. Other directories need no flush: CR3 is reloaded on every context switch and user pages are not global.
5. Calls `pmm_free_page()` on the old frame, which drops this process's reference. The frame is freed only if that was the last one.

The copy runs with interrupts off. It is about a thousand word moves, and shortening it would mean handling a second break or a fork of the same page in the middle of the first. The tests below run this under timer preemption.

### Kernel writes into user memory

The kernel writes user memory through the physical address, not through the process's page tables, so a read-only PTE does not stop it. Two paths write into user buffers, and both call `vmm_cow_break()` first through `user_kptr_write()` (`kernel/syscall.c`): `copy_to_user()` (used by `SYS_READ`, `SYS_MEMINFO`, `SYS_PIPE` and others) and `SYS_GETARG`. Without it, a `read()` into a buffer on a shared page would write into the frame the other process still uses. Reading needs no break, so `copy_from_user()` and `user_kptr()` are unchanged.

### CR0.WP

`vmm_init()` sets `CR0.WP` (bit 16) together with `CR0.PG`. With WP clear, ring 0 can write through a read-only PTE, so a kernel write to a user address of a copy-on-write page would land in the shared frame without faulting. With WP set, that write faults and the same handler resolves it. The kernel's own identity map is writable, so nothing else changes.

### Freeing a process's address space (Phase 23-B)

`vmm_destroy_directory(pd)` (`kernel/memory/vmm.c`) frees a whole address space in one interrupt-off section:

1. If `pd` is the directory loaded in CR3, it loads the kernel directory first. The kernel directory has the same PDE 0 and 1 (the identity map of 0–8 MB), so the caller's kernel code, kernel stack and data stay mapped. It only loses its user pages.
2. For every PDE from 2 up that is present and has `VMM_USER`: one `pmm_free_page()` per present PTE (it drops one reference, so a page still shared copy-on-write stays allocated), then the page-table page itself.
3. Frees the directory page.

What may be freed comes from how directories are built. `vmm_create_directory()` copies only PDE 0 and 1 from the kernel directory; they point at the two static kernel page tables at `0x301000` and `0x302000`, inside the 1–4 MB region `pmm_init()` reserves, and they never have `VMM_USER`. Every other page table in a process directory is allocated by `vmm_map_user_page_flags()` for that directory alone, with `VMM_USER` on the PDE, and `fork()` builds the child's tables the same way. So PDE 0 and 1 are never touched, and a PDE from 2 up without `VMM_USER` is skipped. The kernel directory and 0 are refused outright.

`process_exit()` calls it inside its own interrupt-off section: it copies `cr3`, zeroes the field, frees the directory, and marks the slot `PROCESS_UNUSED`. A second exit of the same process sees `PROCESS_UNUSED` and returns, so the directory can't be freed twice. There is no zombie state and no deferred reaper, because nothing needs one:

- The kernel stack is not allocated. It is a static per-slot array (`process_stacks[]` in `process.c`) that the slot's next occupant reuses.
- A process that exits itself (`sys_exit()`, or `sys_kill()` of its own pid) is running on the directory being freed. Step 1 moves it to the kernel directory first. Once its state is `PROCESS_UNUSED` the timer no longer preempts it (`timer_callback()` only preempts a `RUNNING` process), so it reaches `scheduler_yield()` and is never scheduled again. `sys_kill()` of its own pid used to return to user mode, which only worked because the directory leaked; it now leaves the CPU like `sys_exit()`.
- A process killed by another one (Ctrl+C, `kill`) is not running. It is ready, sleeping or blocked, and it is never scheduled again once its slot is unused, so nothing walks its directory afterwards.

`fork()`'s failure path and `exec()`'s failure paths (ELF load, user stack, no free slot) call `vmm_destroy_directory()` too. Before 23-B they leaked the half-built directory.

Cases the code handles:

- **Killed while blocked on an ATA command.** The ATA IRQ handler and the ATA gate keep only a `process_t` pointer and wake it only if its state is still `PROCESS_BLOCKED`; they never touch its address space. The sector data goes through FAT16's own kernel buffers, never straight into user pages, so freeing the directory while the command is in flight is safe for memory. (The pointer itself can outlive the process; see `docs/TODO.md`.)
- **Killed with Ctrl+C.** The shell calls `nos_kill()` on the foreground pid, so this is the "killed by another process" case above.
- **Parent exits before its child.** BooleOS keeps no parent link and `sys_wait()` reaps nothing; each process frees its own address space when it exits, whatever its parent did.
- **Child exits with pages still shared copy-on-write with its parent.** The child drops one reference per page. The parent keeps the page; if the count reaches 1, its next write takes it back writable in place with no copy.
- **The shell.** It is an ordinary process with its own directory, freed only if the shell itself exits (`exit`, or `kill` of its own pid). The kernel directory, which the scheduler and the boot code run on, is never freed.

### Double frees

`pmm_free_page()` on a page that is not allocated (already free, or outside the managed range) changes nothing and prints `pmm: ERROR free of a page that is not allocated (double free?): 0x...` on the serial port only. Before, it returned silently.

### Tests

`selftest` tests 22 to 24 (see [testing.md](testing.md)) check that writes stay private in both orders with the count at 2 and then 1, that the count follows three generations of live holders, and that four children rewriting four shared pages under preemption leave every page at a count of 1 in the parent. Tests 33 and 34 check that 30 `fork()`+exit and 20 `exec()`+exit cycles leave the free PMM page count exactly where it was.

For the Phase 14 hardening of userland-pointer resolution built on top of the
VMM (`vmm_get_user_phys_from_dir()`, `user_ptr_valid()`, `copy_from_user()`/
`copy_to_user()`), see [security.md](security.md).

## Relevant files

```
kernel/memory/
  pmm.c             Physical Memory Manager, per-page reference count
  vmm.c             Virtual Memory Manager, vmm_cow_break()
  heap.c            kmalloc/kfree
kernel/irq.h        irq_save()/irq_restore()
```
