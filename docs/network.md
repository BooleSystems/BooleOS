# Networking (Phase 24, in progress)

Status: 24-A (raw e1000 driver) is done and validated in QEMU. 24-B (ARP) is implemented on `nightly` and waits for QEMU validation. IP/ICMP (24-C) is not started. Nothing is reachable from userland yet: no syscall, no shell command.

## Layers

```
kernel/net/arp.c    ARP: cache, resolve, responder            (24-B)
kernel/net/net.c    config, big-endian helpers, poll/dispatch  (24-B)
kernel/drivers/e1000.c   raw frames in and out                 (24-A)
```

`net_poll()` drains up to 16 frames from `e1000_poll_rx()` and hands each to `net_input()`, which dispatches on the ethertype: `0x0806` goes to `arp_input()`, everything else is dropped for now (24-C adds IPv4). There are no interrupts and no background polling: frames are only read when some kernel code calls `net_poll()`, today only from `arp_resolve()` and the boot self-test.

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
- `e1000_ready()`, `e1000_get_mac()`, `e1000_tx_count()`, `e1000_rx_enable_tick()` (tick of the `RCTL.EN` write), `e1000_rx_diag()`.

### Concurrency

The driver and the `net`/`arp` code run only from kmain at boot, in a single context, before any process exists, so nothing guards the ring indexes, the TX buffer, the ARP cache or the static frame buffers. When 24-C makes any of it reachable from a process or an IRQ handler, each entry point needs `irq_save()`/`irq_restore()` or a lock first: two callers interleaved by the timer would hand the card the same descriptor. The 0.22.1 VGA race was the same mistake.

### One TX buffer

Every TX descriptor points at the single TX buffer. `e1000_send()` waits for the frame's `DD`, but it can give up after 10 ticks while the card still owns the descriptor. The next send therefore checks the descriptor before the tail (the last one handed to the card) and returns -1 if it has no `DD` yet, instead of overwriting the buffer under the card. Every descriptor starts with `DD` set, so the first send passes. `e1000_tx_count()` counts frames handed to the card.

## Network configuration

`net_config` (`kernel/net/net.h`) holds our IP 10.0.2.15, netmask 255.255.255.0 and gateway 10.0.2.2, the defaults of QEMU's user-mode network, plus the MAC copied from the driver by `net_init()`. No DHCP. IPv4 addresses are `uint32_t` in host order (`NET_IP(10,0,2,15)`); `net_get_be16/32()` and `net_put_be16/32()` move them in and out of frames. No struct is laid over a frame.

## ARP (24-B)

- **Cache:** 8 static entries `{ip, mac, tick, valid}`. An entry is fresh for 6000 ticks (60 s). A new address refreshes its own entry, else takes a free or expired one, else replaces the oldest.
- **`arp_resolve(ip, mac)`:** a fresh cache entry answers at once and sends nothing. Otherwise it broadcasts "who-has ip, tell 10.0.2.15" and calls `net_poll()` with `timer_poll_delay_ms(1)` between tries, about 500 ms per request, up to 3 requests. Returns 0 or -1.
- **`arp_input(frame, len)`:** needs `len >= 42`, hardware type 1, protocol type 0x0800, hlen 6, plen 4, and ignores anything else. Only the 28 ARP bytes after the Ethernet header are read; slirp pads its frames to 60–64 bytes and the padding is never looked at. A request whose target IP is ours: the sender goes into the cache and a unicast reply goes to the sender's MAC. A reply: the sender goes into the cache, unless its IP is 0.0.0.0.
- Frames are built in one static 42-byte buffer. `e1000_send()` copies it into the card's TX buffer before returning, so a reply sent while `arp_resolve()` waits can reuse it.

### Boot self-test

`net_boot_selftest()` (`kernel/net/net.c`), called from kmain right after `e1000_init()` succeeds:

1. Waits until 120 ticks have passed since receiving was enabled, polling and dropping whatever arrives. QEMU's e1000 model holds received frames for about 1 s after the `RCTL` write (24-A found this; see the 24-A history below). Without this wait, `arp_resolve()` would give up on its first request after 500 ms and send a second one before the first reply came through, and the pcap would show two who-has for one answer.
2. `arp_resolve(10.0.2.2)`: prints `[NET] ARP: 10.0.2.2 is at 52:55:0a:00:02:02`, or `... did not answer (3 requests)` plus the `[NET] diag:` serial line below.
3. `arp_resolve(10.0.2.2)` again: prints `[NET] ARP cache hit` if it succeeded and `e1000_tx_count()` did not move.
4. A synthetic request (sender 10.0.2.99 / 02:00:00:00:00:99, target 10.0.2.15) is fed to `net_input()`. Prints `[NET] ARP responder: reply sent` if exactly one frame went out. That reply really goes on the wire, to 02:00:00:00:00:99, so it shows up in the pcap.

The whole self-test costs about 1.2 s of boot time under QEMU; `docs/TODO.md` has it removed once `ping` (24-C) exists. Nothing in it can stop the boot.

`e1000_rx_diag()` prints, on the serial port only, `[NET] diag: RCTL=... RDLEN=... RDH=... RDT=... RDBAL=... STATUS=... RAL0=... RAH0=... D0STA=... D0LEN=... next=... GPRC=... MPC=... RNBC=...`: the RX registers, the status and length of descriptor 0, the next descriptor the driver expects, and the card's good-packets-received, missed-packets and no-buffer counters (clear-on-read, read once each). GPRC > 0 means the card took a frame; RNBC or MPC > 0 means it had no buffer; all zero means the frame was filtered or never reached the card.

### 24-A history: the receive window

The first QEMU run of 24-A saw the ARP reply in the pcap 26 µs after the request while the driver's self-test reported none, with every RX setting checked and correct. The self-test wait was too short: it also stopped after 10M fast polls, and QEMU's e1000 model holds received frames for about one second of virtual time after each `RCTL` write (`flush_queue_timer` in QEMU's `hw/net/e1000.c`) and only then delivers them. With a 300-tick wait the reply arrived, about 1 s into the wait. 24-B moved the self-test from the driver into `kernel/net/net.c`.

## Running with the network

- `make run`: no network option, so QEMU adds its default NIC, an e1000 on a user-mode backend. The driver finds it there too.
- `make run-net`: the same e1000 given explicitly (`-netdev user,id=n0 -device e1000,netdev=n0`) plus `-object filter-dump,...,file=../build/net.pcap`. Read the dump with `tcpdump -nn -e -r ../build/net.pcap`; the boot self-test should show exactly one who-has 10.0.2.2, slirp's reply, and our reply "10.0.2.15 is-at 52:54:00:12:34:56" sent to 02:00:00:00:00:99. Then `run selftest` (34 tests) and `fetch`: 24-B allocates nothing, so the free PMM count stays at 3760 KB.

## Things that bite

- **Uncached MMIO.** The register window is mapped `PCD | PWT`; a cached mapping can return stale register values and delay writes.
- **The PDE has to exist before the first process.** Process directories copy the kernel PDEs present when they are created ([memory.md](memory.md)), so the mapping is made at boot. A kernel PDE added later would reach no existing process.
- **Bus Master.** Memory Space alone lets the CPU reach the registers; the card still cannot DMA the rings without Bus Master.
- **EERD done bit.** The 82540EM reports done in bit 4; some later parts use bit 1. Adding such a part to `e1000_ids[]` needs that handled.
- **`fork()` and kernel PDEs.** Before 24-A, `process_fork()` treated every PDE from 2 up as user memory; with the MMIO PDE copied into every directory it would have tried to share MMIO frames and failed every fork. It now skips PDEs without `VMM_USER`.

## Relevant files

```
kernel/drivers/e1000.c/h   driver, rings, send/poll, RX diag line
kernel/net/net.c/h         config, big-endian helpers, net_poll/net_input, boot self-test
kernel/net/arp.c/h         ARP cache, arp_resolve, arp_input
kernel/drivers/pci.c/h     config writes, BAR sizing
kernel/memory/vmm.c        kernel PDEs copied into every new directory
tools/Makefile             run-net target
```
