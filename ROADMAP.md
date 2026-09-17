# NullOS — Future roadmap

Completed phases (0–14) are documented in `README.md`. This file covers
planned, not-yet-started phases only.

| Phase | Description | Status |
|------|-----------|--------|
| **15** | Inter-process pipes + a real `waitpid()`; the shell gains `cmd1 \| cmd2` redirection built on top of the existing `fork()` | 🔜 Planned |
| **16** | Copy-on-write `fork()`: defer the address-space copy until the first write instead of duplicating everything upfront (the classic optimization for the `fork()`+`exec()` pattern) | 🔜 Planned |
| **17** | FAT16 subdirectories (today only the root directory exists) | 🔜 Planned |
| **18** | `e1000` network driver (already visible in Phase 11's PCI enumeration) + a minimal TCP/IP stack; initial goal is answering `ping` | 🔜 Planned |
| **19** | AHCI (modern SATA) driver — requires switching the QEMU machine to `-machine q35` (ICH9), since the default i440FX chipset doesn't expose AHCI | 🔜 Planned |
| **20** | USB HID via the xHCI controller, so keyboard/mouse work on modern hardware without a physical PS/2 port | 🔜 Planned |
| **21** | Linear framebuffer (via the Multiboot2 framebuffer tag) + a simple GUI (rectangular windows, mouse), replacing VGA text mode | 🔜 Planned |

## Detailed planning (Phases 15–21)

The table above gives the one-line summary of each planned phase. This section expands each one with its goal, intended approach, main risk, and dependencies on other phases, as of the current planning pass. No code has changed as part of this — this is a documentation-only update. (Phases 15–21 here were Phases 14–20 before Phase 14 was taken by the security-hardening work — see CHANGELOG.md.)

### Phase 15 — Inter-process pipes + `waitpid()`

- **Goal:** the shell supports `cmd1 | cmd2`; `waitpid(pid)` blocks until one *specific* process terminates (not just the generic `sys_wait`).
- **Approach:** a pipe is a circular buffer allocated on the kernel heap, following the same pattern as the existing keyboard ringbuffer, with a read fd and a write fd. Built on top of `fork()` (Phase 13) plus fd redirection into the read/write ends of the pipe. `waitpid` reuses the `PROCESS_BLOCKED` state introduced in Phase 12.
- **Main risk:** a writer blocking on a full pipe and a reader blocking on an empty pipe at the same time — the same class of deadlock hazard that Phase 12 (IRQ-driven ATA) already required care around.
- **Depends on:** Phase 13 (`fork()`) — already done.

### Phase 16 — Copy-on-write `fork()`

- **Goal:** `fork()` no longer copies all physical memory up front; the parent's pages become read-only and shared until the first write.
- **Approach:** requires a smart page-fault handler (exception 14) that distinguishes a COW fault from a real fault, allocates a new page on demand, copies the data, and remaps it read-write. Needs a per-physical-page refcount in the PMM (which likely doesn't exist yet) to know when it's safe to free a shared page.
- **Main risk:** without a correct refcount, one process can free a page the other is still using.
- **Depends on:** Phase 13 (`fork()`). Recommended after Phase 15 (pipes) is stable, to avoid debugging two new features at once.

### Phase 17 — FAT16 subdirectories

- **Goal:** `mkdir`, `cd`, and commands (`ls`/`edit`/`touch`) accept a path with a subfolder (e.g. `edit docs/notes.txt`), not just a flat name at the root.
- **Approach:** FAT16 natively supports this (a dirent with the `ATTR_DIRECTORY` attribute points to a cluster holding another dirent table). Needs: a path parser (split on `/`), recursive navigation reusing the existing dirent lookup logic, and `fat16_mkdir` (creates a directory-attribute entry and allocates a cluster containing `.` and `..`).
- **Main risk / opportunity:** a good moment to refactor the dirent lookup that's currently duplicated between `fat16_find` and `fat16_write_file` (known tech debt — an out-of-bounds bug was previously fixed in one copy but not the other, see `docs/filesystem.md` and `PROGRESS.md`). Unifying it into a single function before extending to subdirectories avoids repeating that bug a third time.
- **Depends on:** nothing beyond Phase 10 (already done). Can be done at any time, independent of the process-related phases (15/16).

### Phase 18 — `e1000` network driver + minimal TCP/IP

- **Goal:** a modest starting point — respond to `ping` (ICMP echo request).
- **Approach:** the `e1000` device was already detected via PCI enumeration in Phase 11. Steps: (a) use `pci.c` to find the device's memory BAR and map it via the VMM (it's memory-mapped I/O, unlike port I/O as used by ATA); (b) initialize RX/TX descriptor rings (the Intel datasheet is well documented publicly); (c) parse Ethernet frames; (d) implement ARP; (e) implement enough of IP+ICMP to answer a ping.
- **Main risk:** the largest scope in the roadmap — recommended to split into sub-phases (18a: raw driver sending/receiving a frame; 18b: ARP; 18c: IP+ICMP) rather than attempting it all at once.
- **Depends on:** Phase 11 (PCI) — already done. Independent of Phases 15–17.

### Phase 19 — AHCI driver (modern SATA)

- **Goal:** disk access on a real SATA controller via AHCI, not just the legacy emulated IDE.
- **Approach:** requires switching the QEMU machine to `-machine q35` (the ICH9 chipset exposes AHCI; the default i440FX chipset doesn't). AHCI uses memory-mapped registers (BAR5) with a "command list" + "FIS" structure, quite different from the current ATA PIO interface. The VFS interface (`vfs_read`/`vfs_write`) shouldn't need to change — only the driver underneath it.
- **Main risk / note:** switching QEMU machine type also changes which PCI devices get enumerated (different chipset = different IDs) — this is expected, not a bug, but can be confusing if tested without knowing this in advance.
- **Depends on:** Phase 11 (PCI). Recommended after Phase 18 (networking), since networking doesn't require a chipset switch — this isolates the environment change to a single phase.

### Phase 20 — USB HID via the xHCI controller

- **Goal:** keyboard/mouse working over USB — essential for running on modern hardware without a physical PS/2 port.
- **Approach:** xHCI has its own descriptor structures and considerably more state than AHCI, with a full USB protocol stack on top (device enumeration, descriptors, endpoints, control and interrupt transfers).
- **Main risk:** by far the largest scope/complexity phase in the entire roadmap — recommended to treat as its own sub-roadmap (20a: enumerate the xHCI controller; 20b: port reset; 20c: enumerate the connected device; 20d: parse HID reports; etc.) rather than one monolithic phase.
- **Depends on:** Phase 11 (PCI). Technically independent of Phases 15–19, but recommended to come last among the driver phases since it's the largest complexity jump.

### Phase 21 — Linear framebuffer + simple GUI

- **Goal:** move off VGA text mode into a real graphics mode (pixels), with rectangular windows and mouse support.
- **Approach:** GRUB2/Multiboot2 can hand over a linear framebuffer directly via a Multiboot2 protocol tag (no need for a real GPU driver like VBE/BIOS calls, which don't work anymore once protected mode has been entered) — just request it in `grub.cfg` and read the physical address from the structure.
- **Main risk / note:** without a working mouse (Phase 20), a "GUI" with no decent input has limited value — recommended after Phase 20, even though the framebuffer itself has no technical dependency on USB.
- **Depends on:** none technically, but gains much more value after Phase 20 (mouse).

## Recommended priority order

**Phase 17 → Phase 15 → Phase 18 → Phase 16 → Phase 19 → Phase 20 → Phase 21.**

Rationale: start with the lowest-risk work that doesn't require changing the test environment, and save the highest-complexity / environment-changing phases for last.
