# NullOS — Future roadmap

Completed phases (0–16) are documented in `README.md`. This file covers
planned, not-yet-started phases only.

| Phase | Description | Status |
|------|-----------|--------|
| **17** | Copy-on-write `fork()`: defer the address-space copy until the first write instead of duplicating everything upfront (the classic optimization for the `fork()`+`exec()` pattern) | 🔜 Planned |
| **18** | `e1000` network driver (already visible in Phase 11's PCI enumeration) + a minimal TCP/IP stack; initial goal is answering `ping` | 🔜 Planned |
| **19** | AHCI (modern SATA) driver — requires switching the QEMU machine to `-machine q35` (ICH9), since the default i440FX chipset doesn't expose AHCI | 🔜 Planned |
| **20** | USB HID via the xHCI controller, so keyboard/mouse work on modern hardware without a physical PS/2 port | 🔜 Planned |
| **21** | Linear framebuffer (via the Multiboot2 framebuffer tag) + a simple GUI (rectangular windows, mouse), replacing VGA text mode | 🔜 Planned |
| **22** | Formal syscall deprecation and compatibility strategy — evolve/fix existing syscalls without breaking already-compiled user programs, once the project reaches v1.0.0 | 🔜 Planned |

## Detailed planning (Phases 17–22)

The table above gives the one-line summary of each planned phase. This section expands each one with its goal, intended approach, main risk, and dependencies on other phases, as of the current planning pass. No code has changed as part of this — this is a documentation-only update. (Phases 15–21 here were Phases 14–20 before Phase 14 was taken by the security-hardening work — see CHANGELOG.md. They were renumbered a second time when FAT16 subdirectories — originally planned and listed here as "Phase 17" — was actually implemented ahead of the two process-related phases that preceded it in this list, landing as Phase 15 instead; see CHANGELOG.md `[0.15.0]`. The phases below were 15, 16, 18, 19, 20, 21, 22 before that: only the first two shifted by one, everything from the former network phase onward kept its number. Phase 16, inter-process pipes + real `waitpid()`, was completed as planned — see `[0.16.0]` in CHANGELOG.md and Phase 16 in README.md.)

### Phase 17 — Copy-on-write `fork()`

- **Goal:** `fork()` no longer copies all physical memory up front; the parent's pages become read-only and shared until the first write.
- **Approach:** requires a smart page-fault handler (exception 14) that distinguishes a COW fault from a real fault, allocates a new page on demand, copies the data, and remaps it read-write. Needs a per-physical-page refcount in the PMM (which likely doesn't exist yet) to know when it's safe to free a shared page.
- **Main risk:** without a correct refcount, one process can free a page the other is still using.
- **Depends on:** Phase 13 (`fork()`) — already done. Phase 16 (pipes), previously recommended as a prerequisite to avoid debugging two new features at once, is also already done.

### Phase 18 — `e1000` network driver + minimal TCP/IP

- **Goal:** a modest starting point — respond to `ping` (ICMP echo request).
- **Approach:** the `e1000` device was already detected via PCI enumeration in Phase 11. Steps: (a) use `pci.c` to find the device's memory BAR and map it via the VMM (it's memory-mapped I/O, unlike port I/O as used by ATA); (b) initialize RX/TX descriptor rings (the Intel datasheet is well documented publicly); (c) parse Ethernet frames; (d) implement ARP; (e) implement enough of IP+ICMP to answer a ping.
- **Main risk:** the largest scope in the roadmap — recommended to split into sub-phases (18a: raw driver sending/receiving a frame; 18b: ARP; 18c: IP+ICMP) rather than attempting it all at once.
- **Depends on:** Phase 11 (PCI) — already done. Independent of Phases 16–17.

### Phase 19 — AHCI driver (modern SATA)

- **Goal:** disk access on a real SATA controller via AHCI, not just the legacy emulated IDE.
- **Approach:** requires switching the QEMU machine to `-machine q35` (the ICH9 chipset exposes AHCI; the default i440FX chipset doesn't). AHCI uses memory-mapped registers (BAR5) with a "command list" + "FIS" structure, quite different from the current ATA PIO interface. The VFS interface (`vfs_read`/`vfs_write`) shouldn't need to change — only the driver underneath it.
- **Main risk / note:** switching QEMU machine type also changes which PCI devices get enumerated (different chipset = different IDs) — this is expected, not a bug, but can be confusing if tested without knowing this in advance.
- **Depends on:** Phase 11 (PCI). Recommended after Phase 18 (networking), since networking doesn't require a chipset switch — this isolates the environment change to a single phase.

### Phase 20 — USB HID via the xHCI controller

- **Goal:** keyboard/mouse working over USB — essential for running on modern hardware without a physical PS/2 port.
- **Approach:** xHCI has its own descriptor structures and considerably more state than AHCI, with a full USB protocol stack on top (device enumeration, descriptors, endpoints, control and interrupt transfers).
- **Main risk:** by far the largest scope/complexity phase in the entire roadmap — recommended to treat as its own sub-roadmap (20a: enumerate the xHCI controller; 20b: port reset; 20c: enumerate the connected device; 20d: parse HID reports; etc.) rather than one monolithic phase.
- **Depends on:** Phase 11 (PCI). Technically independent of Phases 16–19, but recommended to come last among the driver phases since it's the largest complexity jump.

### Phase 21 — Linear framebuffer + simple GUI

- **Goal:** move off VGA text mode into a real graphics mode (pixels), with rectangular windows and mouse support.
- **Approach:** GRUB2/Multiboot2 can hand over a linear framebuffer directly via a Multiboot2 protocol tag (no need for a real GPU driver like VBE/BIOS calls, which don't work anymore once protected mode has been entered) — just request it in `grub.cfg` and read the physical address from the structure.
- **Main risk / note:** without a working mouse (Phase 20), a "GUI" with no decent input has limited value — recommended after Phase 20, even though the framebuffer itself has no technical dependency on USB.
- **Depends on:** none technically, but gains much more value after Phase 20 (mouse).

### Phase 22 — Syscall deprecation and compatibility strategy

- **Goal:** allow evolving/fixing existing syscalls without breaking already-compiled user programs — important especially once the project adopts real semver (documented milestone: once the project reaches v1.0.0, the syscall interface becomes the reference "public API", per `CHANGELOG.md`).
- **Approach (to be decided in detail when this phase is implemented, but the general direction is):**
  - Never remove or rewrite the behavior of an existing syscall number once the project is past v1.0 — instead, add a NEW syscall number (e.g. `SYS_WRITE_FILE_V2`) for the new behavior, keeping the old one working as before, documented as deprecated in `kernel/syscall.h` with an explicit comment pointing to its replacement.
  - Consider introducing a small shared library (a minimal libc-style layer) that user programs link against, instead of issuing `int 0x80` with a raw syscall number directly — this allows swapping the implementation underneath (including redirecting old calls to new syscalls internally) without recompiling existing user programs, similar to glibc's role on Linux.
  - Before v1.0.0, syscall changes remain free (as already documented — major version 0 allows any change), so this phase's compatibility discipline only actually takes effect once the project reaches v1.0.0.
- **Main risk / note:** this is more an architecture/process discipline decision than a single isolated code feature — it may not require one-off "implementation," but rather be applied gradually as each future syscall is added/changed after v1.0.0.
- **Depends on:** none technically, but only makes sense to actively apply starting at the v1.0.0 milestone (real semver).

## Recommended priority order

**Phase 18 → Phase 17 → Phase 19 → Phase 20 → Phase 21.**

Rationale: start with the lowest-risk work that doesn't require changing the test environment, and save the highest-complexity / environment-changing phases for last. (FAT16 subdirectories, formerly first in this list as "Phase 17", is now done — see Phase 15 in `README.md`. Phase 16, inter-process pipes + real `waitpid()`, is also now done — see Phase 16 in `README.md`.)
