// booleos/kernel/net/arp.h — ARP over Ethernet/IPv4 (Phase 24-B).
// Same calling context as net.h: kernel, not IRQ, one caller at a time.

#ifndef ARP_H
#define ARP_H

#include <stdint.h>

#define ARP_CACHE_SIZE   8
#define ARP_TTL_TICKS    6000   /* 60 s at the PIT's 100 Hz */

/* Looks ip up. A cache entry younger than ARP_TTL_TICKS answers at once,
   without sending anything. Otherwise broadcasts a who-has and polls
   (net_poll()) for the reply, up to 3 requests of ~500 ms each. Returns 0
   with the MAC in mac_out, or -1. */
int arp_resolve(uint32_t ip, uint8_t mac_out[6]);

/* Handles one received ARP frame (Ethernet header included). Ignores
   anything that is not Ethernet/IPv4 ARP or shorter than 42 bytes; reads
   only the ARP fields, so padding after them does not matter. A request for
   our IP: learns the sender and sends a unicast reply. A reply: learns the
   sender (unless its IP is 0.0.0.0). */
void arp_input(const uint8_t *frame, uint16_t len);

#endif
