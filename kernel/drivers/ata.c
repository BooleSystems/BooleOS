/* nullos/kernel/drivers/ata.c — ATA PIO polling, tenta os 4 slots */
#include "ata.h"
#include "../timer.h"
#include <stdint.h>

/* ── offsets de registrador relativos à base do canal ───────── */
#define REG_DATA        0   /* 0x1F0 / 0x170 */
#define REG_ERROR       1
#define REG_FEATURES    1
#define REG_SECCOUNT    2
#define REG_LBA_LO      3
#define REG_LBA_MID     4
#define REG_LBA_HI      5
#define REG_DRIVE_HEAD  6
#define REG_STATUS      7
#define REG_CMD         7

/* offsets do registrador de controle (base_ctrl) */
/* basta escrever/ler base_ctrl+0 */

/* bits de status */
#define ATA_SR_BSY  0x80
#define ATA_SR_DRQ  0x08
#define ATA_SR_ERR  0x01

/* seletor de drive no registrador DRIVE_HEAD */
#define DRIVE_MASTER 0xA0
#define DRIVE_SLAVE  0xB0
/* versão LBA (bits 6 e 4 setados) */
#define DRIVE_LBA_MASTER 0xE0
#define DRIVE_LBA_SLAVE  0xF0

/* comandos */
#define CMD_READ     0x20
#define CMD_WRITE    0x30
#define CMD_IDENTIFY 0xEC
#define CMD_FLUSH    0xE7

/* assinaturas ATAPI no LBA_MID/HI após IDENTIFY */
#define ATAPI_MID 0x14
#define ATAPI_HI  0xEB

/* ── canais conhecidos ───────────────────────────────────────── */
static const uint16_t BASES[2]      = { 0x1F0, 0x170 };
static const uint16_t CTRL_BASES[2] = { 0x3F6, 0x376 };

/* ── inline I/O ─────────────────────────────────────────────── */
static inline uint8_t inb(uint16_t p) {
    uint8_t v; __asm__ volatile ("inb %1,%0":"=a"(v):"Nd"(p)); return v;
}
static inline void outb(uint16_t p, uint8_t v) {
    __asm__ volatile ("outb %0,%1"::"a"(v),"Nd"(p));
}
static inline uint16_t inw(uint16_t p) {
    uint16_t v; __asm__ volatile ("inw %1,%0":"=a"(v):"Nd"(p)); return v;
}
static inline void outw(uint16_t p, uint16_t v) {
    __asm__ volatile ("outw %0,%1"::"a"(v),"Nd"(p));
}

/* ── estado do drive selecionado ────────────────────────────── */
static int      g_present   = 0;
static uint16_t g_base      = 0;    /* base de dados (ex: 0x1F0) */
static uint16_t g_ctrl      = 0;    /* base de controle (ex: 0x3F6) */
static uint8_t  g_drive_sel = 0;    /* 0xA0=master, 0xB0=slave */
static uint8_t  g_lba_sel   = 0;    /* 0xE0=master LBA, 0xF0=slave LBA */

/* ── helpers (usam g_base/g_ctrl) ───────────────────────────── */
static void ata_delay(void) {
    inb(g_ctrl); inb(g_ctrl); inb(g_ctrl); inb(g_ctrl);
}
static int wait_not_busy(void) {
    for (uint32_t i = 0; i < 0x10000000; i++)
        if (!(inb(g_base + REG_STATUS) & ATA_SR_BSY)) return 0;
    return -1;
}
static int wait_drq(void) {
    for (uint32_t i = 0; i < 0x10000000; i++) {
        uint8_t s = inb(g_base + REG_STATUS);
        if (s & ATA_SR_ERR) return -1;
        if (s & ATA_SR_DRQ) return 0;
    }
    return -1;
}

/* timeout de reset por datasheet ATA: BSY pode ficar setado por ate
   ~500ms-1s apos um soft-reset (SRST). timer_init() roda a 100Hz
   (main.c), entao 1 tick = 10ms; ver comentario analogo em timer.c. */
#define ATA_RESET_TIMEOUT_TICKS 100u  /* ~1s */

/* espera o BSY (bit 7) do registrador de status limpar depois de um
   soft-reset, usando o PIT (timer_get_ticks) como timeout real ao
   inves de um delay fixo — o tempo de reset varia entre boots.
   le pelo registrador de controle (alternate status) pra nao mexer
   em nenhum estado de IRQ pendente. retorna 0 se limpou, -1 se
   estourou o timeout (tratado como "sem disco nesse slot"). */
static int wait_bsy_clear_after_reset(uint16_t ctrl) {
    uint32_t start = timer_get_ticks();
    for (;;) {
        uint8_t s = inb(ctrl);
        if (!(s & ATA_SR_BSY)) return 0;
        /* 0xFF = barramento flutuante (nenhum controlador/drive
           respondendo) — nao adianta esperar o timeout inteiro,
           a checagem de floating-bus de verdade continua depois
           do drive-select, isso aqui e so pra nao segurar o boot */
        if (s == 0xFF) return -1;
        if ((timer_get_ticks() - start) >= ATA_RESET_TIMEOUT_TICKS) return -1;
    }
}

/* ── tenta identificar um slot; retorna 1 se ATA (não ATAPI) ── */
static int probe(uint16_t base, uint16_t ctrl, uint8_t drive_sel) {
    /* reset suave */
    outb(ctrl, 0x04); outb(ctrl, 0x00);
    /* delay minimo antes de comecar a poll: da tempo do dispositivo
       assumir BSY internamente antes de checarmos */
    inb(ctrl); inb(ctrl); inb(ctrl); inb(ctrl);

    /* espera BSY==0 de verdade (timeout ~1s), em vez de um delay
       fixo de 400ns que nao garante que o reset terminou */
    if (wait_bsy_clear_after_reset(ctrl) < 0) return 0;

    outb(base + REG_DRIVE_HEAD, drive_sel);
    /* delay de ~400ns: 4 leituras do alternate status register */
    inb(ctrl); inb(ctrl); inb(ctrl); inb(ctrl);

    uint8_t status_after_select = inb(base + REG_STATUS);
    if (status_after_select == 0xFF) return 0;  /* floating bus */

    /* zera registradores e envia IDENTIFY */
    outb(base + REG_SECCOUNT, 0);
    outb(base + REG_LBA_LO,   0);
    outb(base + REG_LBA_MID,  0);
    outb(base + REG_LBA_HI,   0);
    outb(base + REG_CMD,      CMD_IDENTIFY);
    inb(ctrl); inb(ctrl); inb(ctrl); inb(ctrl);

    uint8_t st = inb(base + REG_STATUS);
    if (st == 0x00) return 0;   /* drive não existe */

    /* espera BSY=0 (timeout rápido) */
    for (uint32_t i = 0; i < 0x10000000; i++) {
        if (!(inb(base + REG_STATUS) & ATA_SR_BSY)) break;
        if (i == 0x0FFFFFFF) return 0;
    }

    uint8_t lba_mid = inb(base + REG_LBA_MID);
    uint8_t lba_hi  = inb(base + REG_LBA_HI);

    /* ATAPI seta LBA_MID=0x14, LBA_HI=0xEB — descarta */
    if (lba_mid == ATAPI_MID && lba_hi == ATAPI_HI) return 0;

    /* espera DRQ */
    for (uint32_t i = 0; i < 0x10000000; i++) {
        st = inb(base + REG_STATUS);
        if (st & ATA_SR_ERR) return 0;
        if (st & ATA_SR_DRQ) break;
        if (i == 0x0FFFFFFF) return 0;
    }

    /* drena os 256 words de IDENTIFY */
    for (int i = 0; i < 256; i++) inw(base + REG_DATA);
    return 1;
}

/* ── API pública ─────────────────────────────────────────────── */

int ata_init(void) {
    g_present = 0;

    /* ordem: primary master, primary slave, secondary master, secondary slave */
    static const uint8_t drv_sel[2] = { DRIVE_MASTER, DRIVE_SLAVE   };
    static const uint8_t lba_sel[2] = { DRIVE_LBA_MASTER, DRIVE_LBA_SLAVE };

    for (int ch = 0; ch < 2; ch++) {
        for (int dr = 0; dr < 2; dr++) {
            if (probe(BASES[ch], CTRL_BASES[ch], drv_sel[dr])) {
                g_base      = BASES[ch];
                g_ctrl      = CTRL_BASES[ch];
                g_drive_sel = drv_sel[dr];
                g_lba_sel   = lba_sel[dr];
                g_present   = 1;
                return 1;
            }
        }
    }
    return 0;
}

int ata_read_sector(uint32_t lba, void *buf) {
    if (!g_present) return -1;
    if (wait_not_busy() < 0) return -1;

    outb(g_base + REG_DRIVE_HEAD, g_lba_sel | ((lba >> 24) & 0x0F));
    outb(g_base + REG_SECCOUNT,   1);
    outb(g_base + REG_LBA_LO,     (uint8_t)(lba));
    outb(g_base + REG_LBA_MID,    (uint8_t)(lba >> 8));
    outb(g_base + REG_LBA_HI,     (uint8_t)(lba >> 16));
    outb(g_base + REG_CMD,        CMD_READ);
    ata_delay();

    if (wait_not_busy() < 0) return -1;
    if (wait_drq()      < 0) return -1;

    uint16_t *dst = (uint16_t *)buf;
    for (int i = 0; i < 256; i++) dst[i] = inw(g_base + REG_DATA);
    return 0;
}

int ata_write_sector(uint32_t lba, const void *buf) {
    if (!g_present) return -1;
    if (wait_not_busy() < 0) return -1;

    outb(g_base + REG_DRIVE_HEAD, g_lba_sel | ((lba >> 24) & 0x0F));
    outb(g_base + REG_SECCOUNT,   1);
    outb(g_base + REG_LBA_LO,     (uint8_t)(lba));
    outb(g_base + REG_LBA_MID,    (uint8_t)(lba >> 8));
    outb(g_base + REG_LBA_HI,     (uint8_t)(lba >> 16));
    outb(g_base + REG_CMD,        CMD_WRITE);
    ata_delay();

    if (wait_not_busy() < 0) return -1;
    if (wait_drq()      < 0) return -1;

    const uint16_t *src = (const uint16_t *)buf;
    for (int i = 0; i < 256; i++) outw(g_base + REG_DATA, src[i]);

    /* confirma que o proprio comando WRITE terminou (BSY=0) e sem
       ERR — isso e o payload de verdade fisicamente gravado, ANTES
       de qualquer flush. Emitir outro comando (FLUSH) com BSY ainda
       setado e invalido pelo protocolo ATA. */
    if (wait_not_busy() < 0) return -1;
    if (inb(g_base + REG_STATUS) & ATA_SR_ERR) return -1;

    /* flush de cache: best-effort. Os dados ja estao confirmados
       gravados pelo WRITE acima (checado logo ali em cima); um
       timeout aqui so quer dizer que o drive nao confirmou o commit
       do cache pra midia dentro do prazo — nao significa que os
       dados sumiram, entao NAO propaga como falha do write. */
    outb(g_base + REG_CMD, CMD_FLUSH);
    wait_not_busy();
    return 0;
}
