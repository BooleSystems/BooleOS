// booleos/kernel/net/net.c — network layer above the e1000 driver (Phase 24).
// Context: kernel, not IRQ, one caller at a time (see net.h).

#include "net.h"
#include "arp.h"
#include "../drivers/e1000.h"
#include "../hal.h"
#include "../messages.h"
#include "../timer.h"
#include <stdint.h>

#define NET_POLL_MAX 16   /* frames per net_poll() call */

net_config_t net_config = {
    NET_IP(10, 0, 2, 15),     /* QEMU user-mode guest address */
    NET_IP(255, 255, 255, 0),
    NET_IP(10, 0, 2, 2),      /* QEMU user-mode gateway */
    { 0, 0, 0, 0, 0, 0 },
};

uint16_t net_get_be16(const uint8_t *p) {
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

uint32_t net_get_be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8)  |  (uint32_t)p[3];
}

void net_put_be16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}

void net_put_be32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

void net_init(void) {
    e1000_get_mac(net_config.mac);
}

void net_input(const uint8_t *frame, uint16_t len) {
    if (!frame || len < ETH_HDR_LEN) return;
    switch (net_get_be16(frame + 12)) {
    case ETH_TYPE_ARP:
        arp_input(frame, len);
        break;
    default:
        break;   /* IPv4 arrives in 24-C */
    }
}

int net_poll(void) {
    static uint8_t rx[E1000_MAX_FRAME];
    int taken = 0;
    for (int i = 0; i < NET_POLL_MAX; i++) {
        int n = e1000_poll_rx(rx, sizeof(rx));
        if (n == 0) break;          /* ring empty */
        taken++;
        if (n > 0) net_input(rx, (uint16_t)n);   /* n < 0: dropped by the driver */
    }
    return taken;
}

/* ── console helpers ──────────────────────────────────────────────── */

void net_print_ip(uint32_t ip) {
    for (int i = 3; i >= 0; i--) {
        console_put_dec((ip >> (8 * i)) & 0xFF);
        if (i) console_putc('.');
    }
}

void net_print_mac(const uint8_t mac[6]) {
    static const char hex[] = "0123456789abcdef";
    for (int i = 0; i < 6; i++) {
        if (i) console_putc(':');
        console_putc(hex[mac[i] >> 4]);
        console_putc(hex[mac[i] & 0xF]);
    }
}

static void net_tag(void) {
    console_set_color(CONSOLE_WHITE, CONSOLE_BLACK);
    console_puts(msg(MSG_NET_TAG));
    console_set_color(CONSOLE_LIGHT_GREY, CONSOLE_BLACK);
}

/* ── boot self-test (24-B) ────────────────────────────────────────── */

/* QEMU's e1000 model holds received frames for about 1 s after RCTL is
   written (they are queued, then delivered). arp_resolve() gives up on a
   request after ~500 ms and sends another, so a resolve started right after
   init would put two or three who-has on the wire for one answer. The boot
   self-test therefore first lets that window pass, polling (and dropping)
   whatever arrives. The wait is bounded by ticks and by the iteration count,
   like every other wait here. Real hardware has no such window; this only
   costs boot time, and it goes away with the self-test (docs/TODO.md). */
#define QEMU_RX_HOLD_TICKS 120

static void wait_rx_window(void) {
    uint32_t since = e1000_rx_enable_tick();
    for (uint32_t it = 0; it < QEMU_RX_HOLD_TICKS && timer_get_ticks() - since < QEMU_RX_HOLD_TICKS; it++) {
        net_poll();
        timer_poll_delay_ms(1);
    }
}

void net_boot_selftest(void) {
    if (!e1000_ready()) return;
    wait_rx_window();

    /* (a) resolve the gateway: one who-has, one reply */
    uint8_t mac[6];
    net_tag();
    console_puts(msg(MSG_NET_ARP_PREFIX));
    net_print_ip(net_config.gateway);
    if (arp_resolve(net_config.gateway, mac) != 0) {
        console_puts(msg(MSG_NET_ARP_NO_ANSWER));
        e1000_rx_diag();
        return;
    }
    console_puts(msg(MSG_NET_ARP_IS_AT));
    net_print_mac(mac);
    console_putc('\n');

    /* (b) again: must come from the cache, with nothing transmitted */
    uint32_t sent = e1000_tx_count();
    uint8_t mac2[6];
    int r = arp_resolve(net_config.gateway, mac2);
    net_tag();
    console_puts(msg(r == 0 && e1000_tx_count() == sent ? MSG_NET_ARP_CACHE_HIT
                                                          : MSG_NET_ARP_CACHE_MISS));

    /* (c) a synthetic who-has 10.0.2.15 from 10.0.2.99 / 02:00:00:00:00:99,
       straight into net_input(): the responder must send exactly one reply
       (it goes out on the wire to that MAC and shows up in the pcap). */
    static uint8_t req[42];
    static const uint8_t peer[6] = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x99 };
    for (int i = 0; i < 6; i++) {
        req[i] = net_config.mac[i];   /* unicast to us, as a peer that knew us would */
        req[6 + i] = peer[i];
        req[22 + i] = peer[i];        /* sender hardware address */
        req[32 + i] = 0;              /* target hardware address: unknown */
    }
    net_put_be16(req + 12, ETH_TYPE_ARP);
    net_put_be16(req + 14, 1);
    net_put_be16(req + 16, ETH_TYPE_IPV4);
    req[18] = 6;
    req[19] = 4;
    net_put_be16(req + 20, 1);
    net_put_be32(req + 28, NET_IP(10, 0, 2, 99));
    net_put_be32(req + 38, net_config.ip);

    sent = e1000_tx_count();
    net_input(req, sizeof(req));
    net_tag();
    console_puts(msg(e1000_tx_count() == sent + 1 ? MSG_NET_ARP_RESPONDER_OK
                                                  : MSG_NET_ARP_RESPONDER_FAIL));
}
