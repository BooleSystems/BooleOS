// booleos/kernel/net/arp.c — ARP over Ethernet/IPv4 (Phase 24-B).
//
// Fixed cache of ARP_CACHE_SIZE entries, no allocation. A frame is built in
// one static buffer and handed to e1000_send(), which copies it into the
// card's TX buffer before returning, so the buffer can be reused right away
// (arp_resolve() sends a request, then the replies arp_input() sends while
// it waits reuse the same buffer).
//
// Context: kernel, not IRQ, one caller at a time (see net.h).

#include "arp.h"
#include "net.h"
#include "../drivers/e1000.h"
#include "../timer.h"
#include <stdint.h>

/* ARP packet for Ethernet/IPv4, offsets from the start of the frame. */
#define ARP_HTYPE   (ETH_HDR_LEN + 0)
#define ARP_PTYPE   (ETH_HDR_LEN + 2)
#define ARP_HLEN    (ETH_HDR_LEN + 4)
#define ARP_PLEN    (ETH_HDR_LEN + 5)
#define ARP_OPER    (ETH_HDR_LEN + 6)
#define ARP_SHA     (ETH_HDR_LEN + 8)
#define ARP_SPA     (ETH_HDR_LEN + 14)
#define ARP_THA     (ETH_HDR_LEN + 18)
#define ARP_TPA     (ETH_HDR_LEN + 24)
#define ARP_FRAME_LEN (ETH_HDR_LEN + 28)   /* 42 */

#define ARP_OP_REQUEST 1
#define ARP_OP_REPLY   2

#define ARP_TRIES        3
#define ARP_WAIT_TICKS   50    /* ~500 ms per request */

typedef struct {
    uint32_t ip;
    uint8_t  mac[6];
    uint32_t tick;     /* when the entry was learned or last refreshed */
    int      valid;
} arp_entry_t;

static arp_entry_t g_cache[ARP_CACHE_SIZE];
static uint8_t     g_frame[ARP_FRAME_LEN];

static void copy_mac(uint8_t *dst, const uint8_t *src) {
    for (int i = 0; i < 6; i++) dst[i] = src[i];
}

static int fresh(const arp_entry_t *e, uint32_t now) {
    return e->valid && (now - e->tick) < ARP_TTL_TICKS;
}

static const arp_entry_t *cache_lookup(uint32_t ip) {
    uint32_t now = timer_get_ticks();
    for (int i = 0; i < ARP_CACHE_SIZE; i++)
        if (fresh(&g_cache[i], now) && g_cache[i].ip == ip)
            return &g_cache[i];
    return 0;
}

/* Learns ip -> mac: refreshes the entry for ip if there is one, else takes
   a free or expired slot, else replaces the oldest entry. */
static void cache_store(uint32_t ip, const uint8_t *mac) {
    uint32_t now = timer_get_ticks();
    arp_entry_t *slot = 0;
    for (int i = 0; i < ARP_CACHE_SIZE; i++)
        if (g_cache[i].valid && g_cache[i].ip == ip) { slot = &g_cache[i]; break; }
    if (!slot)
        for (int i = 0; i < ARP_CACHE_SIZE; i++)
            if (!fresh(&g_cache[i], now)) { slot = &g_cache[i]; break; }
    if (!slot) {
        slot = &g_cache[0];
        for (int i = 1; i < ARP_CACHE_SIZE; i++)
            if (now - g_cache[i].tick > now - slot->tick) slot = &g_cache[i];
    }
    slot->ip = ip;
    copy_mac(slot->mac, mac);
    slot->tick = now;
    slot->valid = 1;
}

/* Builds an Ethernet/IPv4 ARP frame from us in g_frame and sends it. */
static int send_arp(uint16_t oper, const uint8_t *eth_dst,
                    const uint8_t *tha, uint32_t tpa) {
    copy_mac(g_frame, eth_dst);
    copy_mac(g_frame + 6, net_config.mac);
    net_put_be16(g_frame + 12, ETH_TYPE_ARP);
    net_put_be16(g_frame + ARP_HTYPE, 1);
    net_put_be16(g_frame + ARP_PTYPE, ETH_TYPE_IPV4);
    g_frame[ARP_HLEN] = 6;
    g_frame[ARP_PLEN] = 4;
    net_put_be16(g_frame + ARP_OPER, oper);
    copy_mac(g_frame + ARP_SHA, net_config.mac);
    net_put_be32(g_frame + ARP_SPA, net_config.ip);
    copy_mac(g_frame + ARP_THA, tha);
    net_put_be32(g_frame + ARP_TPA, tpa);
    return e1000_send(g_frame, ARP_FRAME_LEN);
}

int arp_resolve(uint32_t ip, uint8_t mac_out[6]) {
    static const uint8_t broadcast[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    static const uint8_t zero[6]      = { 0, 0, 0, 0, 0, 0 };

    const arp_entry_t *e = cache_lookup(ip);
    if (e) { copy_mac(mac_out, e->mac); return 0; }
    if (!e1000_ready()) return -1;

    for (int t = 0; t < ARP_TRIES; t++) {
        (void)send_arp(ARP_OP_REQUEST, broadcast, zero, ip);   /* a failed send just waits out its try */

        /* timer_poll_delay_ms() counts on the PIT itself and rounds up to
           one timer period (10 ms), so the iteration count also bounds the
           wait if the tick counter stopped. */
        uint32_t start = timer_get_ticks();
        for (uint32_t it = 0; it < ARP_WAIT_TICKS && timer_get_ticks() - start < ARP_WAIT_TICKS; it++) {
            net_poll();
            e = cache_lookup(ip);
            if (e) { copy_mac(mac_out, e->mac); return 0; }
            timer_poll_delay_ms(1);
        }
    }
    return -1;
}

void arp_input(const uint8_t *frame, uint16_t len) {
    /* Only the 28 ARP bytes after the Ethernet header are read; whatever
       padding follows (slirp sends 60-64 byte frames) is never looked at. */
    if (!frame || len < ARP_FRAME_LEN) return;
    if (net_get_be16(frame + ARP_HTYPE) != 1) return;
    if (net_get_be16(frame + ARP_PTYPE) != ETH_TYPE_IPV4) return;
    if (frame[ARP_HLEN] != 6 || frame[ARP_PLEN] != 4) return;

    uint16_t oper = net_get_be16(frame + ARP_OPER);
    uint32_t spa  = net_get_be32(frame + ARP_SPA);
    uint32_t tpa  = net_get_be32(frame + ARP_TPA);
    uint8_t  sha[6];
    copy_mac(sha, frame + ARP_SHA);   /* g_frame may be rebuilt below; keep our own copy */

    if (oper == ARP_OP_REQUEST) {
        if (tpa != net_config.ip) return;
        if (spa != 0) cache_store(spa, sha);
        (void)send_arp(ARP_OP_REPLY, sha, sha, spa);
    } else if (oper == ARP_OP_REPLY) {
        if (spa != 0) cache_store(spa, sha);
    }
}
