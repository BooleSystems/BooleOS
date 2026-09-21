# NullOS — Future roadmap

The table below is the granular phase table: one row per phase AND per
sub-phase, for both completed work (Phases 0–20, detailed in `README.md`,
`CHANGELOG.md` and `docs/`) and planned work (Phases 21–31, ending at the
v1.0.0 milestone, detailed in the section further down). Planned
sub-phases become completed rows as they land.

There is deliberately no package-manager phase anywhere in this roadmap — it was considered and decided against.

**Sub-phase notation:** historical phases (0–16) keep the notation they
actually used in the original commits and CHANGELOG — a lowercase letter
with no hyphen (`2b`, `3a`, `3b`). New phases (17 onward, defined in the
restructuring after Phase 16) use a hyphen and an uppercase letter
(`17-A`, `18-B`). The difference is deliberate, not an inconsistency to
"fix".

| Phase | Description | Status | Details |
|------|-----------|--------|---------|
| **0** | Bootloader (Multiboot2) + VGA text output | ✅ Done | CHANGELOG `[0.0.1]`, `docs/kernel.md` |
| **1** | GDT, IDT, PIC, PIT (100 Hz), PS/2 keyboard | ✅ Done | CHANGELOG `[0.1.0]`, `docs/kernel.md` |
| **2** | PMM (Physical Memory Manager) + VMM with paging + kernel heap (`kmalloc`/`kfree`) | ✅ Done | CHANGELOG `[0.2.0]`, `docs/memory.md` |
| **2b** | VMM with paging + heap (`kmalloc`/`kfree`) | ✅ Done | CHANGELOG `[0.2.0]`, `docs/memory.md` |
| **3** | Process table + cooperative round-robin scheduler, per-process context switch, per-process CR3, exception handlers | ✅ Done | CHANGELOG `[0.3.0]`/`[0.4.0]`, `docs/scheduler.md` |
| **3a** | Process table + cooperative round-robin scheduler | ✅ Done | CHANGELOG `[0.3.0]`, `docs/scheduler.md` |
| **3b** | Per-process context switch, per-process CR3, exception handlers | ✅ Done | CHANGELOG `[0.4.0]`, `docs/scheduler.md` |
| **4** | TSS, ring 3 usermode, syscalls via `int 0x80` | ✅ Done | CHANGELOG `[0.4.0]`, `docs/kernel.md`, `docs/syscalls.md` |
| **5** | Multiboot2 module parser, flat ramfs, ELF32 loader, `exec()`, `user/init` | ✅ Done | CHANGELOG `[0.5.0]`, `docs/kernel.md` |
| **6** | Syscall return value in `eax`, preemption via IRQ0 (10-tick slice) | ✅ Done | CHANGELOG `[0.6.0]`, `docs/scheduler.md`, `docs/syscalls.md` |
| **7** | `SYS_READ`, keyboard ringbuffer, interactive userland shell | ✅ Done | CHANGELOG `[0.7.0]`, `docs/shell.md`, `docs/syscalls.md` |
| **8** | `SYS_EXEC`, Ctrl+C, foreground PID, copy-from-user | ✅ Done | CHANGELOG `[0.8.0]`, `docs/shell.md`, `docs/syscalls.md` |
| **9** | `SYS_OPEN`, `SYS_CLOSE`, `SYS_READ` for ramfs files, per-process fd table | ✅ Done | CHANGELOG `[0.9.0]`, `docs/syscalls.md`, `docs/filesystem.md` |
| **10** | Persistent disk: ATA PIO driver, FAT16 read/write, `SYS_CREATE`/`SYS_WRITE_FILE`, `touch`, editor with real saving | ✅ Done | CHANGELOG `[0.10.0]`/`[0.10.1]`, `docs/filesystem.md` |
| **11** | PCI bus enumeration (legacy Configuration Mechanism #1), device table, `SYS_PCI_LIST`/`lspci` | ✅ Done | CHANGELOG `[0.11.0]`, `docs/pci.md` |
| **12** | ATA IRQ-driven I/O: IRQ14/15 handlers, process blocking instead of busy-wait, exclusion gate, `PROCESS_BLOCKED` | ✅ Done | CHANGELOG `[0.12.0]`, `docs/scheduler.md`, `docs/filesystem.md` |
| **13** | `fork()`: full address-space duplication, fabricated child kernel stack (resumes via `isr128_resume`), fd table duplication, `SYS_FORK` | ✅ Done | CHANGELOG `[0.13.0]`, `docs/scheduler.md` |
| **14** | Kernel memory-safety hardening: userland pointer validation (4 confirmed ring 3 → ring 0 bugs), `kmalloc()` overflow fix, `sys_open`/`sys_create`/`sys_exec`/`sys_getarg` string validation; version centralized in `kernel/version.h` | ✅ Done | CHANGELOG `[0.14.0]`, `docs/security.md` |
| **15** | FAT16 subdirectories: `mkdir`/`cd`, path-aware `touch`/`edit`/`ls`, shared `dir_lookup()`/`dir_insert()`/`resolve_path()` core, `SYS_CHDIR`/`SYS_MKDIR`, `exec()` inherits `cwd_cluster` | ✅ Done | CHANGELOG `[0.15.0]`, `docs/filesystem.md` |
| **16** | Inter-process pipes (`SYS_PIPE`/`SYS_EXEC_PIPE`) and a real blocking `waitpid()`; shell gains `cmd1 \| cmd2` | ✅ Done | CHANGELOG `[0.16.0]`, `docs/pipes.md`, `docs/scheduler.md` |
| **17** | Cleanup A — mechanical fixes from the old audit, known technical debt, libnos/shell tool consolidation, test/build infrastructure | ✅ Done | CHANGELOG `[0.17.0]`, `README.md` |
| **17-A** | Mechanical fixes from the old audit | ✅ Done | CHANGELOG `[0.17.0]` |
| **17-B** | Known technical debt | ✅ Done | CHANGELOG `[0.17.0]` |
| **17-C** | libnos consolidation + shell tools | ✅ Done | CHANGELOG `[0.17.0]` |
| **17-D** | Test/build infrastructure | ✅ Done | CHANGELOG `[0.17.0]` |
| **18** | Safety/portability foundation (HAL + Safe Mode) | ✅ Done | CHANGELOG `[0.18.0]`, `docs/hal.md`, `docs/safemode.md` |
| **18-A** | HAL (hardware abstraction layer), `msg(ID)`, real memory map in the PMM | ✅ Done | CHANGELOG `[0.18.0]`, `docs/hal.md`, `docs/memory.md` |
| **18-B** | Safe Mode (failure counter, text UI, restricted shell, previous-release GRUB entry) | ✅ Done | CHANGELOG `[0.18.0]`, `docs/safemode.md` |
| **19** | SDK / app-development experience | ✅ Done | CHANGELOG `[0.19.0]`, `docs/sdk.md`, `docs/kernel.md` |
| **20** | Crash handler leads into Safe Mode (a crash saves a dump, resets, and Safe Mode shows it) | ✅ Done | CHANGELOG `[0.20.0]`, `docs/safemode.md` |
| **21** | Copy-on-write `fork()` | 🔜 Planned | below |
| **22** | `unlink()`/`rmdir()` | 🔜 Planned | below |
| **23** | Memory/CR3 release in `process_exit()` | 🔜 Planned | below |
| **23-A** | Free the process's physical data pages | 🔜 Planned | below |
| **23-B** | Free the page directory (CR3) and page tables | 🔜 Planned | below |
| **23-C** | Page sharing via fork/COW (needs PMM refcount) | 🔜 Planned | below |
| **24** | `e1000` driver + minimal TCP/IP (ping) | 🔜 Planned | below |
| **24-A** | Raw driver: BAR mapping, RX/TX rings, one Ethernet frame | 🔜 Planned | below |
| **24-B** | ARP | 🔜 Planned | below |
| **24-C** | IP + ICMP (answer ping) | 🔜 Planned | below |
| **25** | AHCI driver | 🔜 Planned | below |
| **26** | USB HID via xHCI | 🔜 Planned | below |
| **26-A** | Enumerate the xHCI controller | 🔜 Planned | below |
| **26-B** | Port reset | 🔜 Planned | below |
| **26-C** | Enumerate the connected device | 🔜 Planned | below |
| **26-D** | Parse HID reports | 🔜 Planned | below |
| **27** | Linear framebuffer + simple GUI | 🔜 Planned | below |
| **28** | Syscall deprecation/compatibility strategy | 🔜 Planned | below |
| **29** | Second pass of audit fixes | 🔜 Planned | below |
| **30** | General polish | 🔜 Planned | below |
| **31** | DOOM engine port (v1.0.0 milestone) | 🔜 Planned | below |
| **31-A** | Portability layer (`i_video`/`i_system`/`i_input`) | 🔜 Planned | below |
| **31-B** | Single-block memory reservation syscall integration for the engine's allocator | 🔜 Planned | below |
| **31-C** | Full engine build/link, first menu screen | 🔜 Planned | below |
| **31-D** | Playable without crashing (no audio) | 🔜 Planned | below |

## Detailed planning (Phases 21–31)

The table above gives the one-line summary of each planned phase. This section expands each one with its goal, intended approach, main risk, and dependencies on other phases, as of the current planning pass. No code has changed as part of this — this is a documentation-only update.

History of the numbering: the phases once listed as 17–22 (copy-on-write `fork()`, `e1000`, AHCI, xHCI, framebuffer/GUI, syscall deprecation) were renumbered in a full restructuring of the roadmap after Phase 16 (FAT16 subdirectories landing as Phase 15 and pipes as Phase 16 are recorded in CHANGELOG.md `[0.15.0]`/`[0.16.0]`). Later, the separate "technical prerequisites for DOOM" phase was folded away: `lseek` moved into general polish (alongside `mv`/`cp`, since it is a generic filesystem capability) and the single-block memory reservation syscall DOOM needs — its own allocator asks for one large contiguous block once at startup — was folded into the DOOM engine port phase itself; DOOM does not need a full userland `malloc`/`free`. Finally, when the crash-handler phase was implemented it took number 20, and every planned phase after it moved up by one (copy-on-write `fork()` is now 21, `unlink()` 22, memory release 23, `e1000` 24, AHCI 25, xHCI 26, framebuffer/GUI 27, syscall deprecation 28, audit pass 2 29, polish 30, the DOOM port 31). Numbers quoted in older CHANGELOG entries and commit messages use the numbering of their time.

### Phase 17 — Cleanup A (closed in 0.17.0)

Done; see the table above and CHANGELOG `[0.17.0]`. One item carried over, not blocking: **test `docs/setup.md` end-to-end on Windows** (no machine available; macOS is out of scope). Do it whenever a Windows environment is available, or in Phase 30 (polish).

### Phase 18 — Safety/portability foundation (closed in 0.18.0)

Done; see the table above, CHANGELOG `[0.18.0]`, `docs/hal.md` and `docs/safemode.md`. Items from the original plan that were deliberately **not** done in this phase, carried over:

- Safe Mode's GRUB entries **GUI debug** and **Text mode**, and "reboot into GUI debug" in its reboot submenu — they need the GUI (Phase 27); for now the menu has Default, serial debug mode, Safe Mode and the previous release.
- Safe Mode's **erase** action (needs `unlink`, Phase 22) and the **disk check with verify-only vs. verify-and-repair** submenu (an fsck-like feature; its own sub-phase, not scheduled yet). The restricted shell is read-only for now.

### Phase 19 — SDK / app-development experience (closed in 0.19.0)

Done; see the table above, CHANGELOG `[0.19.0]` and `docs/sdk.md`. `exec()` finds programs with `vfs_open()` (ramfs, then FAT16) — no second lookup — the ELF loader validates the file against its real size, libnos has a minimal `printf` family, and `sdk/` plus `docs/sdk.md` let someone write, build and run a program without rebuilding the ISO. Carried over, not done in this phase:

- Programs on FAT16 are limited to 192 KB and are read whole into a fixed 256 KB heap that cannot grow once processes exist (`PROGRESS.md`, Phase 23 fixes the underlying heap/identity-map design); a program bigger than that, such as the DOOM engine of Phase 31, needs that work first.
- `run` passes no arguments to a program (`nos_exec()` can); there is no `argc`/`argv` convention yet.
- The printf family has no floating point, no 64-bit integers and no `#` flag.

### Phase 20 — Crash handler leads into Safe Mode (closed in 0.20.0)

Done; see the table above, CHANGELOG `[0.20.0]` and `docs/safemode.md`. An unhandled CPU exception saves a crash record in the boot config sector through polling-only ATA I/O, shows the red screen, and resets the machine; the next boot goes to Safe Mode with the reason and a "view last crash details" screen; the `crash <de|pf|gpf>` shell command tests the whole pipeline (`make run-reboot-test`). Carried over, not done in this phase:

- A crash *inside* the saving code (the re-entry guard), a crash before the disk is up (the halt path) and a fault in kernel mode were not exercised by the manual test; the `crash` command faults in ring 3 (NullOS has no per-process fault isolation yet, so it takes the same fatal path).
- Per-process fault isolation (a user-mode fault killing only that process instead of the machine) is Phase 29's item ("Per-process fault isolation in `idt.c`") and would change what a user-mode crash does.

### Phase 21 — Copy-on-write `fork()`

- **Goal:** `fork()` no longer copies all physical memory up front; the parent's pages become read-only and shared until the first write.
- **Approach:** requires a smart page-fault handler (exception 14) that distinguishes a COW fault from a real fault, allocates a new page on demand, copies the data, and remaps it read-write. Needs a per-physical-page refcount in the PMM (which likely doesn't exist yet) to know when it's safe to free a shared page.
- **Main risk:** without a correct refcount, one process can free a page the other is still using.
- **Depends on:** Phase 13 (`fork()`) — already done. Phase 16 (pipes), previously recommended as a prerequisite to avoid debugging two new features at once, is also already done.

### Phase 22 — `unlink()`/`rmdir()`

- **Goal:** complete the basic set of file operations — today `selftest` itself leaves junk on the disk because no delete syscall exists.
- **Approach:** mark the dirent as `0xE5` (deleted), release the cluster chain back to the FAT's free list; decide a policy for deleting a directory with contents (error vs. recursive).
- **Main risk:** a bug surface similar to mkdir/subdirectories from Phase 15 — treat with the same "approve the approach before the code" rigor.
- **Depends on:** FAT16 (already done).

### Phase 23 — Memory/CR3 release in `process_exit()`

- **Goal:** stop leaking real physical memory every time a process terminates (technical debt since Phase 13).
- **Main risk:** the most dangerous phase in the roadmap — a real risk of double-free or of freeing a page another process still references.
- **Depends on:** Phase 21 (COW fork) changes how memory is shared between processes, so 23-C depends on Phase 21 being closed.

- 23-A: free the process's physical data pages (heap, stack) in `process_exit()`
- 23-B: free the page directory (CR3) and its associated page tables
- 23-C: handle page sharing via fork/COW (Phase 21) — needs a per-physical-page refcount in the PMM before really freeing

### Phase 24 — `e1000` driver + minimal TCP/IP

- **Goal:** a modest starting point — respond to `ping` (ICMP echo request).
- **Approach:** the `e1000` device was already detected via PCI enumeration in Phase 11. Steps: (a) use `pci.c` to find the device's memory BAR and map it via the VMM (it's memory-mapped I/O, unlike port I/O as used by ATA); (b) initialize RX/TX descriptor rings (the Intel datasheet is well documented publicly); (c) parse Ethernet frames; (d) implement ARP; (e) implement enough of IP+ICMP to answer a ping.
- **Main risk:** the largest scope in the roadmap — split into sub-phases rather than attempting it all at once.
- **Depends on:** Phase 11 (PCI) — already done. Independent of the other planned phases.

- 24-A: raw driver — map the memory BAR via the VMM, initialize the RX/TX descriptor rings, send/receive one Ethernet frame
- 24-B: ARP (resolve MAC from IP)
- 24-C: minimal IP + ICMP (answer ping)

### Phase 25 — AHCI driver (modern SATA)

- **Goal:** disk access on a real SATA controller via AHCI, not just the legacy emulated IDE.
- **Approach:** requires switching the QEMU machine to `-machine q35` (the ICH9 chipset exposes AHCI; the default i440FX chipset doesn't). **Also update `user/selftest.c`'s Intel 440FX check (`8086:1237`, test "PCI: Intel 440FX host bridge") to q35's host bridge IDs — it fails there by design.** AHCI uses memory-mapped registers (BAR5) with a "command list" + "FIS" structure, quite different from the current ATA PIO interface. The VFS interface (`vfs_read`/`vfs_write`) shouldn't need to change — only the driver underneath it.
- **Main risk / note:** switching QEMU machine type also changes which PCI devices get enumerated (different chipset = different IDs) — this is expected, not a bug, but can be confusing if tested without knowing this in advance.
- **Depends on:** Phase 11 (PCI). Recommended after Phase 24 (networking), since networking doesn't require a chipset switch — this isolates the environment change to a single phase.

### Phase 26 — USB HID via the xHCI controller

- **Goal:** keyboard/mouse working over USB — essential for running on modern hardware without a physical PS/2 port.
- **Approach:** xHCI has its own descriptor structures and considerably more state than AHCI, with a full USB protocol stack on top (device enumeration, descriptors, endpoints, control and interrupt transfers).
- **Main risk:** by far the largest scope/complexity jump in the entire roadmap — treated as its own sub-roadmap rather than one monolithic phase.
- **Depends on:** Phase 11 (PCI). Technically independent of Phases 23–25, but recommended to come last among the driver phases since it's the largest complexity jump.

- 26-A: enumerate the xHCI controller
- 26-B: port reset
- 26-C: enumerate the connected device
- 26-D: parse HID reports (a real keyboard/mouse)

### Phase 27 — Linear framebuffer + simple GUI

- **Goal:** move off VGA text mode into a real graphics mode (pixels), with rectangular windows and mouse support.
- **Approach:** GRUB2/Multiboot2 can hand over a linear framebuffer directly via a Multiboot2 protocol tag (no need for a real GPU driver like VBE/BIOS calls, which don't work anymore once protected mode has been entered) — just request it in `grub.cfg` and read the physical address from the structure.
- **Main risk / note:** without a working mouse (Phase 26), a "GUI" with no decent input has limited value — recommended after Phase 26, even though the framebuffer itself has no technical dependency on USB.
- **Depends on:** none technically, but gains much more value after Phase 26 (mouse).

### Phase 28 — Syscall deprecation and compatibility strategy

- **Goal:** allow evolving/fixing existing syscalls without breaking already-compiled user programs — important especially once the project adopts real semver (documented milestone: once the project reaches v1.0.0, the syscall interface becomes the reference "public API", per `CHANGELOG.md`).
- **Approach (to be decided in detail when this phase is implemented, but the general direction is):**
  - Never remove or rewrite the behavior of an existing syscall number once the project is past v1.0 — instead, add a NEW syscall number (e.g. `SYS_WRITE_FILE_V2`) for the new behavior, keeping the old one working as before, documented as deprecated in `kernel/syscall.h` with an explicit comment pointing to its replacement.
  - Consider introducing a small shared library (a minimal libc-style layer) that user programs link against, instead of issuing `int 0x80` with a raw syscall number directly — this allows swapping the implementation underneath (including redirecting old calls to new syscalls internally) without recompiling existing user programs, similar to glibc's role on Linux. (libnos, added in `[0.15.1]`, is the start of this.)
  - Before v1.0.0, syscall changes remain free (as already documented — major version 0 allows any change), so this phase's compatibility discipline only actually takes effect once the project reaches v1.0.0.
- **Main risk / note:** this is more an architecture/process discipline decision than a single isolated code feature — it may not require one-off "implementation," but rather be applied gradually as each future syscall is added/changed after v1.0.0.
- **Depends on:** none technically, but only makes sense to actively apply starting at the v1.0.0 milestone (real semver).

### Phase 29 — Second pass of audit fixes

- **Goal:** close the items from the old audit that require design, not just a mechanical fix (they were left out of 17-A on purpose).
- **Depends on:** Phase 18 (HAL) — part of this touches the same I/O areas the HAL abstracts.

- Per-process fault isolation in `idt.c` — today any exception (including a user process's page fault) hangs the whole kernel; it should kill only the offending process
- Whole-operation lock in FAT16 — today only the individual sector is protected by the ATA gate, not the complete `fat16_write_file`/`fat16_create` operation against two processes writing at the same time

### Phase 30 — General polish

- **Goal:** final UX/consistency review before the v1.0 milestone, not a new feature.
- **Depends on:** makes the most sense with the command/error surface already mature (hence it sits near the end).

- `mv`/`cp` (once `unlink` — Phase 22 — exists)
- `lseek` (positional file access; a generic filesystem capability like `mv`/`cp` — the DOOM port's WAD reading (`W_wad.c`) needs it as a hard requirement, so it must land before Phase 31)
- Standardize shell error messages (today each command has its own style)
- Stress test with a huge command line / many spaces
- Measure boot time as a reference for future performance
- Re-read the README with "a stranger's eyes" before the release
- Multiple commands per line with `;` (low priority, optional)
- Actually test `docs/setup.md` on Windows (not blocking; no environment was available when this was written — carried over from Phase 17; macOS is out of scope)

### Phase 31 — DOOM engine port (v1.0.0 milestone)

- **Goal:** the first proof that NullOS runs real, complex third-party software, closing out pre-1.0. Scope explicitly cut: no audio, no performance target, just actually running.
- **Licensing:** the engine (GPL since 1997) can go in the repo; the WAD NEVER goes in the repo — the user injects `doom1.wad` (shareware) or Freedoom on their own.
- **Depends on:** Phase 27 (framebuffer/GUI). Also uses `lseek` from Phase 30 (general polish).

- 31-A: portability layer (`i_video`/`i_system`/`i_input` in the original code) using NullOS's framebuffer, input and timer — reuse 100% of the original game logic (physics, AI, software rendering) untouched
- 31-B: integrate the single-block memory reservation syscall (a one-shot "reserve N MB contiguous" call, `sbrk`-style but called once — DOOM's Z_Zone allocator requests one large block at startup and manages it itself, so no userland `malloc`/`free` is needed); `lseek` for WAD reading already comes from Phase 30
- 31-C: build/link of the full engine running on NullOS, first menu screen appearing
- 31-D: actually playing without crashing (functional level, no audio)

**v1.0.0** closes right after Phase 31.

## Recommended priority order

**Phase 21 → 22 → 23 → 24 → 25 → 26 → 27 → 28 → 29 → 30 → 31 → v1.0.0.**

Dependency notes:

- Phase 23-C depends on Phase 21 (COW fork) being closed.
- Phase 29 depends on Phase 18 (HAL).
- Phase 31 depends on Phase 27 (framebuffer) and on `lseek` from Phase 30.

Rationale: first close accumulated debt and build the safety/portability foundation, then improve the app-development flow, then the process/filesystem/memory work, then the drivers (lowest environment-change risk first, the largest complexity jump — xHCI — last), then the GUI, and finally the second audit pass, polish and the DOOM port that closes pre-1.0.
