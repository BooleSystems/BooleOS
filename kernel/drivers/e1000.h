// booleos/kernel/drivers/e1000.h — Intel 8254x (e1000) network driver, raw
// frames only (Phase 24-A). Polling, no interrupts. See docs/network.md.

#ifndef E1000_H
#define E1000_H

#include <stdint.h>

#define E1000_MAX_FRAME 1514   /* Ethernet frame without the FCS (no jumbo frames) */

/* Finds a supported NIC on the PCI bus (pci_scan_bus() must have run), maps
   its registers, resets it, reads the MAC, brings the link up and sets up the
   RX/TX rings. Returns 1 if the card is ready, 0 if there is none or init
   failed; on failure everything it allocated is released and the boot goes
   on without network. Every wait has a timeout. Must run at boot, before the
   first process is created (the MMIO mapping has to be in the kernel page
   directory before any process directory is cloned from it). */
int e1000_init(void);

/* 1 once e1000_init() succeeded. */
int e1000_ready(void);

/* Copies the card's MAC address (6 bytes) into mac. Only valid when ready. */
void e1000_get_mac(uint8_t mac[6]);

/* Sends one Ethernet frame (destination MAC onwards, no FCS: the card adds
   it). len must be 14..E1000_MAX_FRAME; short frames are padded by the card.
   Waits for the card to report the descriptor done, with a timeout. Returns
   0 on success, -1 on a bad length, no card, or timeout. */
int e1000_send(const uint8_t *frame, uint16_t len);

/* Takes the next received frame, if any: copies it into out and returns its
   length (no FCS), or 0 if nothing is waiting. A frame longer than max, or
   one the card flagged with an error, is dropped and returns -1. Either way
   the descriptor goes back to the card. */
int e1000_poll_rx(uint8_t *out, uint16_t max);

/* Frames handed to the card since boot (a send that timed out counts too).
   Lets a caller check whether something was transmitted. */
uint32_t e1000_tx_count(void);

/* Timer tick at which receiving was enabled (the RCTL.EN write). QEMU's
   e1000 model holds received frames for about 1 s after that write; see
   docs/network.md. */
uint32_t e1000_rx_enable_tick(void);

/* Prints one serial line with the RX registers, descriptor 0 and the
   GPRC/MPC/RNBC counters (clear-on-read), for when an expected frame never
   arrived. */
void e1000_rx_diag(void);

#endif
