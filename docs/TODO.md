# Documentation TODO

Minimal trail of documentation still owed for work done on the `nightly`
branch. Whenever a relevant code change lands without its full write-up,
leave a one-line stub here instead of leaving no trace, for example:

- `WIP: document <feature> (files: X, Y, Z)`
- `TODO later doc. Related files: ...`

The stubs do not need to be complete at every commit. They are resolved
during the final polish of each version, before `nightly` is merged into
`main`: write the real description into the relevant `docs/<subject>.md`,
then delete the stub from this file. This file should be empty (headers
only) whenever a version is closed.

## Pending

Phase 18-A (HAL, first pass) — see `docs/hal.md`:

Phase 18-B (Safe Mode) — pass 1 of 5 done (infrastructure), see `docs/safemode.md`:

- Pass 2 (done, validated): counter, entry condition, `safemode` flag,
  stub; temporary `[BOOTCFG]` dump removed in pass 3.
- Pass 3 (done, awaiting the QEMU check): Safe Mode TUI, tier 1 (numbered menu,
  reboot submenu, disk info, sector hexdump), static buffers only. Things to
  exercise: every menu key, ESC/0 in the submenu, hexdump with empty / letters /
  a huge number / an LBA past the end of the disk, and the two hexdump pages.
- Pass 4 (done, awaiting the QEMU check): tier-2 restricted shell (menu item 5;
  PMM/VMM/heap/FAT16 on demand once per session; `help`, `ls`, `cat`, `pwd`,
  `cd`, `back`; read-only). To exercise: item 5 twice (must not re-init), `ls`/
  `cd`/`pwd`/`cat` on the files the selftest leaves (`st_root.txt`, `st_d1/...`),
  a missing file/dir, ESC in a long `cat`, `back` then reboot from the menu.
  Not done, by design: writes/edit, delete (Phase 21), fsck-like verify/repair
  (its own later sub-phase). Also `kmain` still has its own copy of the PMM
  reservations (module + boot info); it could use the new HAL accessors.
- Pass 5: GRUB entries (Safe Mode, previous release), `tools/prev/`,
  `make snapshot` and the release-checklist step; document the
  "anterior kernel does not know the counter" behavior.

## TECHNICAL DEBT (pre-existing, real) — kernel touches physical pages through the 0-8 MB identity map

**Found while investigating Phase 18-A's `boot_get_memory_map()`. NOT
introduced by the HAL or by any recent change; it has been there since paging
and `exec()` existed. Belongs to Phase 22 (memory release / CR3).**

The kernel page directory only identity-maps 0-8 MB (`vmm_init()`), but the
PMM (`pmm_alloc_page()`) hands out physical pages up to `PMM_MAX_PAGES` (32 MB
today), lowest address first. Several places then write to a freshly
allocated page **through its physical address as if it were a pointer**:

- `kernel/elf.c:54` — `memzero8((uint8_t *)phys, PAGE_SIZE)` on every user
  code/data page, no range check.
- `kernel/process.c:262-265` — `process_fork()` copies each page with
  `src = (uint32_t *)parent_phys`, `dst = (uint32_t *)child_phys`, no range
  check.
- Only the page-table/page-directory allocations are protected:
  `kernel/memory/vmm.c:126` and `:146` reject `pt_phys/pd_phys >= 0x800000`.

Failure scenario: once the 4-8 MB region is used up (heap growth, page
tables, user pages — and `process_exit()` never frees, see PROGRESS.md), the
next `pmm_alloc_page()` returns a page >= 8 MB, and the kernel's write to it
is a page fault (unmapped) in kernel mode. It has not shown up in testing
only because usage so far is small. The user code base (`0x01000000`,
16 MB) and `vmm_map_user_page()`'s `virt < 0x800000` rejection also mean the
identity map cannot simply be widened.

- **Mitigation applied (Phase 18-A):** the PMM's allocatable range is capped
  at 8 MB (`PMM_LIMIT_ADDR`, `pmm.c`), so exhaustion is now an allocation
  failure (`exec`/`fork` report an error) instead of a kernel page fault.
  This is a mitigation, **not the fix**: usable memory is ~4 MB of free pages
  (1024 at boot), and `process_exit()` still never frees.
- **Real fix (Phase 22 or its own phase):** stop touching frames by physical
  address — a temporary-mapping mechanism (`kmap`) or a kernel direct map at
  a high virtual address — and then lift the cap.

Pre-existing bug (found while testing the HAL first pass, NOT caused by it):

- **`edit` with no file name: Ctrl+S shows the misleading "saved (no disk)"
  and saves nothing.** `user/edit.c` only calls `load_file()` when a name was
  given, so `file_fd` stays at its initial `-1` (`edit.c:59`); the Ctrl+S
  handler (`edit.c` ~L233) then takes its `else` branch, which covers two
  different cases — "no disk" and "no file name" — under one message
  ("saved (no disk)", originally "salvo (sem disco)" since commit
  `cf8a977`). It never reaches `nos_write_file()`, so no disk code is
  involved. Expected: with the `[no name]` buffer, Ctrl+S should ask for a
  name ("save as"), create the file, and only report "no disk" when the write
  really failed. Never caught because every earlier test used `edit <file>`.
  Files: `user/edit.c`.
