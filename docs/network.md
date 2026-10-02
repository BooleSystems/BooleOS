# Networking (Phase 24, in progress)

Status: 24-A (raw e1000 driver) is implemented on `nightly` and waits for QEMU validation. ARP (24-B) and IP/ICMP (24-C) are not started. Nothing is reachable from userland yet: no syscall, no shell command.

## The e1000 driver (24-A)

`kernel/drivers/e1000.c/h` drives the Intel 82540EM (PCI `8086:100E`), the NIC QEMU's `pc` machine adds by default at `00:03.0`. Supported IDs live in the `e1000_ids[]` table at the top of the file; another 8254x part is one more line there.

`kmain` calls `e1000_init()` right after the PCI scan and before the ramfs and the shell. Without a card it prints `[NET] no e1000 found` and the boot goes on. Init:

1. Finds the card with `pci_find_device()`, checks that BAR0 is a 32-bit memory BAR of at least 128 KB above `0x10000000` (`pci_bar_size()`).
2. Sets Memory Space and Bus Master in the PCI command register, keeping the other bits. Without Bus Master the card cannot read or write the rings by DMA and nothing ever moves.
3. Maps the 128 KB register window into the kernel directory at virtual == physical, with `VMM_PCD | VMM_PWT` (uncached) and no `VMM_USER`.
4. Masks every interrupt (`IMC = 0xFFFFFFFF`), sets `CTRL.RST`, waits for it to clear, masks again and reads `ICR` to clear it.
5. Reads the MAC from `RAL0`/`RAH0`; if `RAH0.AV` is clear, reads EEPROM words 0–2 through `EERD` and writes them into `RAL0`/`RAH0`.
6. Clears the multicast table, sets `CTRL.SLU` (and `ASDE`), clears `LRST`/`PHY_RST`/`ILOS`/`VME`.
7. Sets up the rings (below), then waits for `STATUS.LU`. A link that stays down is reported (`link down`) but the driver stays usable.

Every wait has a timeout in PIT ticks and a spin cap, so even a stopped timer cannot hang the boot (the Safe Mode failed-boot counter would count a hang as a failed boot). Any init failure prints `[NET] e1000: init failed, continuing without network: <reason>`, gives back every page it took, unmaps the registers and turns RX/TX off.

### Rings and buffers

- 16 RX and 16 TX descriptors, 16 bytes each, one zeroed PMM page per ring (aligned, and inside one page).
- 16 RX buffers of 2048 bytes (8 PMM pages, two buffers per page) and one 2048-byte TX buffer (one page).
- PMM frames are below 8 MB, inside the identity map, so the address the kernel writes through is the bus address handed to the card. The high halves (`RDBAH`, `TDBAH`, descriptor bits 63:32) are 0.
- `RCTL = EN | BAM | SECRC`, buffer size 2048, no promiscuous mode. `RDH = 0`, `RDT = 15`: the card owns every descriptor but one.
- `TCTL = EN | PSP | CT 0x10 | COLD 0x40`, `TIPG = 0x0060200A`. Every TX descriptor points at the single TX buffer.
- Descriptors are read and written as four 32-bit words at fixed offsets. No struct is laid over hardware memory.

The driver keeps 11 PMM pages (44 KB) for as long as the system runs. The free PMM count after boot drops by that much: from 3804 KB to an expected 3760 KB in `fetch` (to be confirmed in QEMU).

### Internal API

- `e1000_send(frame, len)`: `len` must be 14..1514 (no FCS; the card appends it and pads short frames). Copies the frame into the TX buffer, sets `EOP | IFCS | RS`, advances `TDT` and waits up to 10 ticks for `DD`. Returns 0 or -1.
- `e1000_poll_rx(out, max)`: returns the length of the next received frame (FCS stripped), 0 if nothing is waiting, -1 if the frame was split, flagged with an error, or longer than `max` (it is dropped). The descriptor goes back to the card in every case (status cleared, `RDT` moved onto it).
- `e1000_ready()`, `e1000_get_mac()`.

### Concurrency

In 24-A all of this runs at boot, in kmain's single context, before any process exists, so nothing guards the ring indexes or the TX buffer. When 24-B/C make the driver reachable from a process, each call has to run under `irq_save()`/`irq_restore()` or a lock: two callers interleaved by the timer would hand the card the same descriptor. The 0.22.1 VGA race was the same mistake.

### Boot self-test

`e1000_boot_selftest()` broadcasts an ARP request "who has 10.0.2.2, tell 10.0.2.15" (QEMU's user-mode gateway and guest address) and polls RX for up to 100 ticks. It prints `[NET] ARP reply: 10.0.2.2 is at xx:xx:xx:xx:xx:xx`, `[NET] no ARP reply (no network backend?)`, or `[NET] ARP request not sent (transmit timed out)`. None of them affects the boot. When no reply comes it also prints one line on the serial port only, `[NET] diag: RCTL=... RDLEN=... RDH=... RDT=... RDBAL=... STATUS=... RAL0=... RAH0=... D0STA=... D0LEN=... next=... GPRC=... MPC=... RNBC=...`: the RX registers, the status and length of descriptor 0, the next descriptor the driver expects, and the card's good-packets-received, missed-packets and no-buffer counters (clear-on-read, read once each). GPRC > 0 means the card took a frame; RNBC or MPC > 0 means it had no buffer; all zero means the frame was filtered or never reached the card.

The wait is up to 300 ticks (3 s). It used to be 100 ticks, and a cap of 10M fast polls could end it much sooner. The first QEMU run saw the ARP reply in the pcap 26 µs after the request while the self-test reported none, with every RX setting checked and correct (see `PROGRESS.md`). The likely cause is that QEMU's e1000 model refuses to receive for about one second of virtual time after each `RCTL` write and queues frames meanwhile (`flush_queue_timer` in QEMU's `hw/net/e1000.c`, from memory and not checked against the source); a reply to a request sent right after init then only arrives when that window ends. The diag line will confirm or refute it. The headers are built byte by byte with big-endian helpers; 24-B moves them into proper ARP code.

## Running with the network

- `make run`: no network option, so QEMU adds its default NIC, an e1000 on a user-mode backend. The driver finds it there too.
- `make run-net`: the same e1000 given explicitly (`-netdev user,id=n0 -device e1000,netdev=n0`) plus `-object filter-dump,...,file=../build/net.pcap`. Read the dump with `tcpdump -nn -e -r ../build/net.pcap`; the boot self-test should show one ARP request and one reply.

## Things that bite

- **Uncached MMIO.** The register window is mapped `PCD | PWT`; a cached mapping can return stale register values and delay writes.
- **The PDE has to exist before the first process.** Process directories copy the kernel PDEs present when they are created ([memory.md](memory.md)), so the mapping is made at boot. A kernel PDE added later would reach no existing process.
- **Bus Master.** Memory Space alone lets the CPU reach the registers; the card still cannot DMA the rings without Bus Master.
- **EERD done bit.** The 82540EM reports done in bit 4; some later parts use bit 1. Adding such a part to `e1000_ids[]` needs that handled.
- **`fork()` and kernel PDEs.** Before 24-A, `process_fork()` treated every PDE from 2 up as user memory; with the MMIO PDE copied into every directory it would have tried to share MMIO frames and failed every fork. It now skips PDEs without `VMM_USER`.

## Relevant files

```
kernel/drivers/e1000.c/h   driver, rings, send/poll, boot ARP self-test
kernel/drivers/pci.c/h     config writes, BAR sizing
kernel/memory/vmm.c        kernel PDEs copied into every new directory
tools/Makefile             run-net target
```
