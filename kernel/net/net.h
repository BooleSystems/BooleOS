// booleos/kernel/net/net.h — network layer above the e1000 driver (Phase 24).
//
// Context: every function here runs from kernel code that is NOT an IRQ
// handler, and in 24-B only from kmain at boot, one caller at a time. The
// receive path is polled (net_poll()), nothing is locked, and the driver
// underneath is not reentrant. Whatever makes this reachable from a process
// or an IRQ (Phase 24-C) has to add the locking first. See docs/network.md.
//
// Headers are read and written byte by byte with the big-endian helpers
// below; no struct is laid over a frame.

#ifndef NET_H
#define NET_H

#include <stdint.h>

#define ETH_HDR_LEN    14
#define ETH_TYPE_ARP   0x0806
#define ETH_TYPE_IPV4  0x0800

/* IPv4 addresses are uint32_t in host order: 10.0.2.15 is 0x0A00020F. */
#define NET_IP(a, b, c, d) \
    (((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | ((uint32_t)(c) << 8) | (uint32_t)(d))

/* Our addressing. The values are the defaults of QEMU's user-mode (slirp)
   network: the guest is 10.0.2.15/24 and the gateway 10.0.2.2. There is no
   DHCP; these are fixed. */
typedef struct {
    uint32_t ip;
    uint32_t netmask;
    uint32_t gateway;
    uint8_t  mac[6];   /* copied from the driver by net_init() */
} net_config_t;

extern net_config_t net_config;

/* Big-endian (network order) access to a byte buffer. */
uint16_t net_get_be16(const uint8_t *p);
uint32_t net_get_be32(const uint8_t *p);
void     net_put_be16(uint8_t *p, uint16_t v);
void     net_put_be32(uint8_t *p, uint32_t v);

/* Copies the MAC from the driver into net_config. Call once, after
   e1000_init() succeeded. */
void net_init(void);

/* Drains up to 16 received frames from the driver and hands each to
   net_input(). Returns how many frames it took (dropped ones included). */
int net_poll(void);

/* Dispatches one received Ethernet frame by ethertype: ARP goes to
   arp_input(), everything else is ignored for now (24-C adds IPv4). Frames
   shorter than an Ethernet header are ignored. */
void net_input(const uint8_t *frame, uint16_t len);

/* Prints "a.b.c.d" / "xx:xx:xx:xx:xx:xx" on the console. */
void net_print_ip(uint32_t ip);
void net_print_mac(const uint8_t mac[6]);

/* Boot-time check of the ARP layer (24-B): resolves the gateway, resolves
   it again from the cache, and feeds a synthetic ARP request to the
   responder. Prints one line per step; every wait has a timeout and nothing
   here can stop the boot. Does nothing if the card is not ready. */
void net_boot_selftest(void);

#endif
