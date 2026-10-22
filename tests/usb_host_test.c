/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat 0.4.7: the USB drive test (firmware/src/usb_host.c) against a model of the controller in host mode
 * and of the devices on the other end: a mass storage flash drive (EP0 packets of 64 or 8 bytes), a hub with the
 * drive on port 3, a keyboard (not a drive), a drive that is not ready at first, and a controller that does not
 * answer. It tests the probe's logic (the requests, their order, the addresses, the bulk-only transport and what
 * the page says), not the real controller: the bits it writes are the ones JieLi documents for MUSB host mode.
 * (0.4.10) The drive holds a FAT32 card made by an independent FAT implementation (tests/make_fat_fixture.py,
 * tests/fixtures/fat32_card.bin), and the file browser (usb_files.c, FatFs) is walked through it as the page would. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#include "../firmware/hal/fm1_usb.h"
#pragma GCC diagnostic pop

/* ------------------------------------------------------------------ the controller model --- */
static uint8_t common[16], regs[4][32], index_reg, read_reg;
static uint32_t ep_cnt[4], lines, sie_alive = 1, attached_host, attached_device, reads;
static void *ep_tadr[4], *ep_radr[4];
static int dev_setup(const uint8_t *s);              /* 1: a device took it */
static int dev_in(uint8_t *d);                       /* the next control IN packet: its size, < 0 stall */
static void dev_bulk_out(const uint8_t *p, uint32_t n);
static int dev_bulk_in(uint8_t *d);
static int dev_status_ok(void);
static void bus_reset(void);                         /* (0.4.10) POWER bit 3: everything back to address 0 */

static void mock_enable(uint32_t eps) { (void)eps; }
static void mock_txbuf(uint32_t ep, void *p) { ep_tadr[ep] = p; }
static void mock_rxbuf(uint32_t ep, void *p) { ep_radr[ep] = p; }
static void mock_send(uint32_t ep, void *p, uint32_t n) { ep_tadr[ep] = p; ep_cnt[ep] = n; }
static void mock_ep0_send(void *p, uint32_t n) { ep_tadr[0] = ep_radr[0] = p; ep_cnt[0] = n; }
static void mock_ep0_buf(void *p) { ep_radr[0] = p; }
static void mock_nop(void) {}
static void mock_host_attach(void *p) { ep_radr[0] = p; attached_host++; }
static void mock_attach(void *p) { (void)p; attached_device++; }
static uint32_t mock_lines(void) { return lines; }
static uint32_t mock_sie_on(void) { return 1; }
static uint32_t mock_done(void) { return sie_alive; }
static uint32_t mock_zero(void) { return 0; }
static void mock_read(uint32_t r) { read_reg = (uint8_t)r; }
static volatile uint32_t fm1_ms;
static uint32_t mock_data(void)
{
    uint32_t r = read_reg;
    if (!(++reads & 255u)) fm1_ms++;                    /* time passes while the probe polls */
    if (r == 15) return 0x5D;                           /* DEVCTL: session, host mode, full speed device */
    return r < 16 ? common[r] : regs[index_reg][r];
}
static void mock_write(uint32_t r, uint32_t v)
{
    if (r == 14) { index_reg = (uint8_t)v; return; }
    if (r == 1 && (v & 0x08u)) bus_reset();
    if (r < 16) { common[r] = (uint8_t)v; return; }
    if (index_reg == 0 && r == 17) {                    /* host CSR0 */
        if (v == 0x0A) {                                /* SETUP: the 8 bytes from the EP0 buffer */
            assert(ep_cnt[0] == 8);
            regs[0][17] = dev_setup((const uint8_t *)ep_tadr[0]) ? 0 : 0x02;               /* nobody there: TxPktRdy never clears */
        } else if (v == 0x20) {                         /* ReqPkt: an IN data packet */
            int n = dev_in((uint8_t *)ep_radr[0]);
            if (n < 0) regs[0][17] = 0x04;
            else { regs[0][22] = (uint8_t)n; regs[0][17] = 0x01; }
        } else if (v == 0x42) {                         /* the OUT status */
            assert(ep_cnt[0] == 0);
            regs[0][17] = dev_status_ok() ? 0 : 0x04;
        } else if (v == 0x60) {                         /* the IN status */
            regs[0][22] = 0;
            regs[0][17] = dev_status_ok() ? 0x01 : 0x04;
        } else {
            regs[0][17] = 0;
        }
        return;
    }
    if (index_reg == 1 && r == 17) {                    /* host TXCSR1 */
        if (v == 0x01) dev_bulk_out((const uint8_t *)ep_tadr[1], ep_cnt[1]);
        regs[1][17] = 0;
        return;
    }
    if (index_reg == 1 && r == 20) {                    /* host RXCSR1 */
        if (v == 0x20) {
            int n = dev_bulk_in((uint8_t *)ep_radr[1]);
            if (n < 0) regs[1][20] = 0x40;
            else { regs[1][22] = (uint8_t)n; regs[1][23] = 0; regs[1][20] = 0x01; }
        } else {
            regs[1][20] = 0;
        }
        return;
    }
    regs[index_reg][r] = (uint8_t)v;
}
#define fm1_usb_ep_enable mock_enable
#define fm1_usb_ep_txbuf mock_txbuf
#define fm1_usb_ep_rxbuf mock_rxbuf
#define fm1_usb_ep_send mock_send
#define fm1_usb_ep0_send mock_ep0_send
#define fm1_usb_ep0_buf mock_ep0_buf
#define fm1_usb_rx_sync mock_nop
#define fm1_usb_sie_on mock_sie_on
#define fm1_usb_sie_done mock_done
#define fm1_usb_sie_wr_start mock_write
#define fm1_usb_sie_rd_start mock_read
#define fm1_usb_sie_data mock_data
#define fm1_usb_reset mock_nop
#define fm1_usb_off mock_nop
#define fm1_usb_attach mock_attach
#define fm1_usb_host_attach mock_host_attach
#define fm1_usb_sof_take mock_zero
#define UH_LINES_READ() mock_lines()
#define RING_PUBLISH() ((void)0)
#define MELODEE_USB_AUDIO 0
#define MELODEE_CDC 0
#define MELODEE_OTA 0
static void fm1_delay_ms(uint32_t ms) { fm1_ms += ms; }
static void fm1_wdt_feed(void) {}
static unsigned str_len(const char *s) { unsigned n = 0; while (s[n]) n++; return n; }
static void str_cpy(char *d, const char *s, unsigned max)
{
    unsigned i = 0;
    while (s[i] && i + 1 < max) { d[i] = s[i]; i++; }
    d[i] = 0;
}
#define fm1_ms fm1_ms_usb                               /* (usb.c declares its own; this file's clock is the one) */
#include "../firmware/src/usb.c"
#undef fm1_ms
#include "../firmware/src/usb_host.c"
#define UF_HOST_LIBC 1                                  /* (strchr, strlen: the C library's) */
#define CP_GAIN() 4096                                  /* (0.4.11) MASTER at 1 */
#define UF_CPU() 0u
#define FM1_TICKS_PER_US 1u
static uint32_t fm1_ticks(void) { return fm1_ms * 1000u; }
#define CS_WAIT() cs_tick()                             /* (0.4.12) the timer's work while the main loop waits */
#include "../firmware/src/card_play.c"
#include "../firmware/src/card_stream.c"
#include "../firmware/src/usb_files.c"

/* ------------------------------------------------------------------ the devices --- */
/* (0.4.8) a tree: the device on the port (parent -1) and, behind hubs, devices on their ports. A device answers
 * once it is "on": the first one after the bus reset, the others after their hub port's reset. Two devices on at
 * the same address is a conflict the walk must never make. */
enum { D_DRIVE, D_HUB, D_KEYS, D_READER };
typedef struct {
    int kind, addr, mps0, configured, parent, port, on, ls, nports;
    int maxlun, card_lun, deaf, stall_in;              /* stall_in: the next bulk IN stalls (a failed data phase) */                         /* (0.4.8) a reader's slots, the one with a card (-1 none); deaf:
                                                         * every bulk IN stalls until a Bulk-Only reset */
    uint16_t vid, pid;
    int pstate[8];                                      /* a hub's ports: 1 reset done (C_PORT_RESET), 2 cleared */
    uint8_t in[512 + 64];                               /* the control IN data queued */
    uint32_t in_n, in_at;
    int stall;
} udev_t;
static udev_t devs[8];
static int ndev, drive_not_ready, scsi_tur_count, conflicts, bulk_resets;
static uint32_t setups, faddr_bad;
static udev_t *cur;

static const uint8_t DRIVE_DEV[18] = {18, 1, 0x00, 0x02, 0, 0, 0, 64, 0x81, 0x07, 0x67, 0x55, 0x00, 0x01, 1, 2, 3, 1};
static const uint8_t DRIVE_CFG[32] = {9, 2, 32, 0, 1, 1, 0, 0x80, 50,
                                      9, 4, 0, 0, 2, 8, 6, 0x50, 0,
                                      7, 5, 0x81, 2, 64, 0, 0,
                                      7, 5, 0x02, 2, 64, 0, 0};
static const uint8_t HUB_DEV[18] = {18, 1, 0x00, 0x02, 9, 0, 1, 64, 0xE3, 0x05, 0x08, 0x06, 0x00, 0x01, 0, 1, 0, 1};
static const uint8_t HUB_CFG[25] = {9, 2, 25, 0, 1, 1, 0, 0xE0, 50,
                                    9, 4, 0, 0, 1, 9, 0, 0, 0,
                                    7, 5, 0x81, 3, 1, 0, 255};
static const uint8_t KEYS_DEV[18] = {18, 1, 0x10, 0x01, 0, 0, 0, 8, 0x6D, 0x04, 0x1C, 0xC3, 0x00, 0x01, 1, 2, 0, 1};
static const uint8_t KEYS_CFG[34] = {9, 2, 34, 0, 1, 1, 0, 0xA0, 50,
                                     9, 4, 0, 0, 1, 3, 1, 1, 0,
                                     9, 0x21, 0x11, 1, 0, 1, 0x22, 63, 0,
                                     7, 5, 0x81, 3, 8, 0, 10};

static void q(const void *p, uint32_t n, uint32_t wlen)
{
    if (n > wlen) n = wlen;
    memcpy(cur->in, p, n);
    cur->in_n = n;
    cur->in_at = 0;
}
static void q_string(const char *s, uint32_t wlen)
{
    uint8_t b[64];
    uint32_t n = (uint32_t)strlen(s), i;
    b[0] = (uint8_t)(2 + 2 * n);
    b[1] = 3;
    for (i = 0; i < n; i++) { b[2 + 2 * i] = (uint8_t)s[i]; b[3 + 2 * i] = 0; }
    q(b, b[0], wlen);
}
static udev_t *route(void)
{
    int i, n = 0;
    udev_t *d = 0;
    for (i = 0; i < ndev; i++)
        if (devs[i].on && devs[i].addr == common[S_FADDR]) {
            if (!d) d = &devs[i];
            n++;
        }
    if (n > 1) conflicts++;
    if (!d) faddr_bad++;
    return d;
}
static void bus_reset(void)
{
    int i;
    for (i = 0; i < ndev; i++) {
        devs[i].addr = 0;
        devs[i].configured = 0;
        devs[i].on = devs[i].parent < 0;                /* behind a hub: off until its port is reset again */
    }
}
static udev_t *child(udev_t *hub, int port)
{
    int i;
    for (i = 0; i < ndev; i++)
        if (devs[i].parent == hub - devs && devs[i].port == port) return &devs[i];
    return 0;
}
static int dev_setup(const uint8_t *s)
{
    uint8_t rt = s[0], rq = s[1];
    uint16_t val = (uint16_t)(s[2] | s[3] << 8), idx = (uint16_t)(s[4] | s[5] << 8), len = (uint16_t)(s[6] | s[7] << 8);
    udev_t *c;
    setups++;
    cur = route();
    if (!cur) return 0;
    cur->in_n = cur->in_at = 0;
    cur->stall = 0;
    if (rt == 0x80 && rq == 6) {                        /* GET_DESCRIPTOR */
        uint8_t t = (uint8_t)(val >> 8), i = (uint8_t)val;
        const uint8_t *dd = cur->kind == D_HUB ? HUB_DEV : cur->kind == D_KEYS ? KEYS_DEV : DRIVE_DEV;
        if (t == 1) {
            uint8_t d[18];
            memcpy(d, dd, 18);
            d[7] = (uint8_t)cur->mps0;
            if (cur->vid) { d[8] = (uint8_t)cur->vid; d[9] = (uint8_t)(cur->vid >> 8); d[10] = (uint8_t)cur->pid; d[11] = (uint8_t)(cur->pid >> 8); }
            q(d, 18, len);
        } else if (t == 2) {
            if (cur->kind == D_HUB) q(HUB_CFG, sizeof HUB_CFG, len);
            else if (cur->kind == D_KEYS) q(KEYS_CFG, sizeof KEYS_CFG, len);
            else q(DRIVE_CFG, sizeof DRIVE_CFG, len);
        } else if (t == 3) {
            static const uint8_t LANG[4] = {4, 3, 0x09, 0x04};
            if (i == 0) q(LANG, 4, len);
            else if (cur->kind == D_DRIVE) q_string(i == 1 ? " SanDisk" : i == 2 ? "Cruzer  Blade " : "4C530001", len);
            else if (cur->kind == D_READER) q_string(i == 1 ? "Generic" : i == 2 ? "USB Storage" : "000000000", len);
            else if (cur->kind == D_KEYS) q_string(i == 1 ? "Logitech" : "USB Keyboard", len);
            else q_string("USB 2.0 Hub", len);
        } else cur->stall = 1;
    } else if (rt == 0x00 && rq == 5) {                 /* SET_ADDRESS (takes effect after the status stage) */
        cur->configured = -(int)val - 1;
    } else if (rt == 0x00 && rq == 9) {
        cur->configured = 1;
    } else if (rt == 0xA0 && rq == 6 && cur->kind == D_HUB) {    /* the hub descriptor: 100 ms power good */
        uint8_t hd[9] = {9, 0x29, 4, 0, 0, 50, 100, 0, 0xFF};
        hd[2] = (uint8_t)cur->nports;
        q(hd, 9, len);
    } else if (rt == 0x23 && rq == 3 && cur->kind == D_HUB) {    /* SET_FEATURE */
        assert(idx >= 1 && (int)idx <= cur->nports);
        if (val == 4) {                                 /* PORT_RESET: the device there answers at 0 */
            cur->pstate[idx] = 1;
            if ((c = child(cur, idx)) != 0) { c->on = 1; c->addr = 0; c->configured = 0; }
        }
    } else if (rt == 0x23 && rq == 1 && cur->kind == D_HUB) {    /* CLEAR_FEATURE */
        if (val == 20) cur->pstate[idx] = 2;
        if (val == 1 && (c = child(cur, idx)) != 0) c->on = 0;   /* PORT_ENABLE */
    } else if (rt == 0xA3 && rq == 0 && cur->kind == D_HUB) {    /* GET_STATUS(port) */
        uint8_t st[4] = {0, 1, 0, 0};                   /* (powered) */
        if ((c = child(cur, idx)) != 0) {
            st[0] = (uint8_t)(1 | (c->on ? 2 : 0));
            st[1] = (uint8_t)(1 | (c->ls ? 2 : 0));
            st[2] = cur->pstate[idx] == 1 ? 0x10 : 0;
        }
        q(st, 4, len);
    } else if (rt == 0xA1 && rq == 0xFE) {              /* GET_MAX_LUN: a drive stalls it (allowed), a reader answers */
        uint8_t m = (uint8_t)cur->maxlun;
        if (cur->kind == D_READER) q(&m, 1, len); else cur->stall = 1;
    } else if (rt == 0x21 && rq == 0xFF) {              /* Bulk-Only Mass Storage Reset */
        if (cur->deaf == 1) cur->deaf = 0;              /* (deaf 2: a reset does not help) */
        bulk_resets++;
    } else if (rt == 0x02 && rq == 1) {                 /* CLEAR_FEATURE(ENDPOINT_HALT) */
    } else {
        cur->stall = 1;
    }
    return 1;
}
static int dev_in(uint8_t *d)
{
    uint32_t n;
    if (!cur || cur->stall) return -1;
    n = cur->in_n - cur->in_at;
    if (n > (uint32_t)cur->mps0) n = (uint32_t)cur->mps0;
    memcpy(d, cur->in + cur->in_at, n);
    cur->in_at += n;
    return (int)n;
}
static int dev_status_ok(void)
{
    if (!cur || cur->stall) return 0;
    if (cur->configured < 0) { cur->addr = -cur->configured - 1; cur->configured = 0; }
    return 1;
}

/* (0.4.10) the card: the fixture's sectors, zeros elsewhere */
static struct { uint32_t lba; uint8_t d[512]; } *card;
static uint32_t ncard;
static void card_load(void)
{
    FILE *f = fopen("tests/fixtures/fat32_card.bin", "rb");
    long n;
    assert(f);
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    ncard = (uint32_t)(n / 516);
    card = malloc((size_t)ncard * sizeof *card);
    for (uint32_t i = 0; i < ncard; i++) {
        uint8_t le[4];
        assert(fread(le, 1, 4, f) == 4 && fread(card[i].d, 1, 512, f) == 512);
        card[i].lba = le[0] | (uint32_t)le[1] << 8 | (uint32_t)le[2] << 16 | (uint32_t)le[3] << 24;
    }
    fclose(f);
}
static void card_sector(uint32_t lba, uint8_t *d)
{
    uint32_t i;
    for (i = 0; i < ncard; i++)
        if (card[i].lba == lba) { memcpy(d, card[i].d, 512); return; }
    memset(d, 0, 512);
}

/* bulk-only transport on the drive */
static uint8_t bin[64 * 512 + 64];
static uint32_t bin_n, bin_at, last_tag, bin_data;   /* bin_data: where the data ends and the CSW starts */
static uint8_t sense_key;
static void csw(uint32_t residue, uint8_t status)
{
    uint8_t s[13] = {'U', 'S', 'B', 'S'};
    bin_data = bin_n;
    memcpy(s + 4, &last_tag, 4);
    memcpy(s + 8, &residue, 4);
    s[12] = status;
    memcpy(bin + bin_n, s, 13);
    bin_n += 13;
}
static void dev_bulk_out(const uint8_t *p, uint32_t n)
{
    udev_t *d = route();
    uint32_t len;
    int lun = p[13], card;
    assert(d && (d->kind == D_DRIVE || d->kind == D_READER) && d->configured == 1);
    assert(lun <= d->maxlun);
    card = d->kind == D_DRIVE || lun == d->card_lun;
    assert(n == 31 && !memcmp(p, "USBC", 4));          /* every CBW in one packet */
    memcpy(&last_tag, p + 4, 4);
    memcpy(&len, p + 8, 4);
    bin_n = bin_at = 0;
    switch (p[15]) {
    case 0x12: {
        uint8_t r[36] = {0, 0x80, 4, 2, 31};
        memcpy(r + 8, d->kind == D_READER ? "Generic STORAGE DEVICE  1.00" : "SanDisk Cruzer Blade    1.00", 28);
        memcpy(bin, r, 36); bin_n = 36; csw(0, 0);
        break;
    }
    case 0x00:
        scsi_tur_count++;
        if (!card) { sense_key = 2; csw(0, 1); }        /* no card in that slot */
        else if (drive_not_ready) { drive_not_ready--; sense_key = 2; csw(0, 1); }
        else csw(0, 0);
        break;
    case 0x03: {
        uint8_t r[18] = {0x70, 0, sense_key};
        memcpy(bin, r, 18); bin_n = 18; csw(0, 0);
        break;
    }
    case 0x25: {
        if (!card) { csw(len, 1); d->stall_in = 1; break; }   /* no data: the IN pipe stalls, then the CSW */
        uint8_t r[8] = {0x01, 0xCA, 0x3F, 0xFF, 0, 0, 2, 0};     /* 30031872 blocks of 512 */
        memcpy(bin, r, 8); bin_n = 8; csw(0, 0);
        break;
    }
    case 0x28: {
        uint32_t lba = (uint32_t)p[17] << 24 | (uint32_t)p[18] << 16 | (uint32_t)p[19] << 8 | p[20];
        uint32_t cnt = (uint32_t)p[22] << 8 | p[23];
        assert(len == cnt * 512u && cnt <= 64u);
        for (uint32_t k = 0; k < cnt; k++) card_sector(lba + k, bin + k * 512u);   /* (0.4.10) the card */
        bin_n = cnt * 512u;
        csw(0, 0);
        break;
    }
    default:
        csw(len, 1);
    }
}
static int dev_bulk_in(uint8_t *d)
{
    udev_t *v = route();
    if (v && v->deaf) return -1;
    if (v && v->stall_in) { v->stall_in = 0; return -1; }
    uint32_t n = (bin_at < bin_data ? bin_data : bin_n) - bin_at;   /* the data, then the CSW on its own */
    if (n > 64u) n = 64u;
    memcpy(d, bin + bin_at, n);
    bin_at += n;
    return (int)n;
}

/* ------------------------------------------------------------------ the scenes --- */
static void reset_world(void)
{
    memset(devs, 0, sizeof devs);
    memset(&usbh, 0, sizeof usbh);
    memset(common, 0, sizeof common);
    memset(regs, 0, sizeof regs);
    ndev = drive_not_ready = scsi_tur_count = conflicts = bulk_resets = 0;
    setups = faddr_bad = 0;
    lines = 0;
    sie_alive = 1;
    attached_host = attached_device = 0;
    usb.up = 1;
    usb.detached = 0;
}
/* a device: on the port (parent -1, on from the start: the bus reset reaches it) or on a hub's port */
static int add(int kind, int mps0, int parent, int port)
{
    udev_t *d = &devs[ndev];
    d->kind = kind;
    d->mps0 = mps0;
    d->parent = parent;
    d->port = port;
    d->on = parent < 0;
    d->nports = 4;
    if (kind == D_READER) { d->vid = 0x05E3; d->pid = 0x0749; d->card_lun = -1; }
    return ndev++;
}
static void run_ms(uint32_t ms)
{
    uint32_t end = fm1_ms + ms;
    while ((int32_t)(fm1_ms - end) < 0) {
        usbf_task();                                    /* (0.4.10: the main loop's, with the files) */
        for (int t = 0; t < 10; t++) cs_tick();         /* (0.4.12: the 10 kHz timer) */
        fm1_ms++;
    }
}
static void show(const char *title)
{
    uint32_t i;
    char d[32];
    usbh_diag(d);
    printf("  %s:", title);
    for (i = 0; i < UH_LINES; i++) if (usbh.line[i][0]) printf(" [%s]", usbh.line[i]);
    printf(" {%s}\n", d);
    fflush(stdout);
}
static int line_is(uint32_t i, const char *s) { return !strcmp(usbh.line[i], s); }

static int t_drive(int mps0)
{
    reset_world();
    add(D_DRIVE, mps0, -1, 0);
    usbh_want(1);
    run_ms(5);
    assert(usbh.st == UH_WAIT && usb.up == 0 && usb.detached == 1 && attached_host == 1);
    assert(line_is(0, "Host mode on") && line_is(1, "Waiting for a drive"));
    lines = 2;                                          /* plugged in: D+ high */
    run_ms(400);
    show(mps0 == 64 ? "drive, EP0 64" : "drive, EP0 8");
    assert(usbh.st == UH_DONE && !faddr_bad);
    assert(devs[0].addr == 1 && devs[0].configured == 1);
    assert(line_is(1, "Found: full speed"));
    assert(line_is(2, "ID 0781:5567"));
    assert(line_is(3, "SanDisk Cruzer Blade"));
    assert(!usbh.line[4][0]);                          /* the same name as line 3: not twice */
    assert(line_is(5, "Drive 14.3 GB"));
    assert(line_is(6, "Disk: FAT32"));
    assert(!strncmp(usbh.line[7], "Read ", 5));
    lines = 0;                                          /* unplugged */
    run_ms(400);
    assert(usbh.st == UH_WAIT && line_is(0, "Host mode on") && line_is(1, "Waiting for a drive") && !usbh.line[2][0]);
    usbh_want(0);                                       /* OCT-: the device again */
    run_ms(2);
    assert(usbh.st == UH_OFF && usb.detached == 0 && attached_device == 1);
    return 1;
}
/* (0.4.8) the RCA C109 as the first unit test found it: a hub with a second hub (1A40:0801) on port 1; behind that
 * a low speed keyboard (passed over), the SD reader with no card (passed over), and on port 4 the drive */
static void rca(int with_drive)
{
    int top = add(D_HUB, 64, -1, 0), h2;
    devs[top].vid = 0x05E3; devs[top].pid = 0x0610;
    h2 = add(D_HUB, 64, top, 1);
    devs[h2].vid = 0x1A40; devs[h2].pid = 0x0801;
    devs[add(D_KEYS, 8, h2, 1)].ls = 1;
    add(D_READER, 64, h2, 2);
    if (with_drive) add(D_DRIVE, 64, h2, 4);
    add(D_KEYS, 8, top, 3);                             /* (not in the list: hubs and disks only) */
}
static int t_hub(void)
{
    reset_world();
    rca(1);
    usbh_want(1);
    run_ms(5);
    lines = 2;
    run_ms(12000);
    show("two hubs, empty reader, drive on 1.4");
    assert(usbh.st == UH_DONE && !conflicts);
    assert(devs[0].addr == 1 && devs[1].addr == 2 && devs[3].addr == 3 && devs[4].addr == 4 && devs[4].configured == 1);
    assert(!devs[2].on);                                /* (the low speed keyboard: never enabled) */
    assert(line_is(1, "Through hubs: 1.4") && line_is(2, "ID 0781:5567") && line_is(5, "Drive 14.3 GB"));
    assert(line_is(6, "Disk: FAT32") && !strncmp(usbh.line[7], "Read ", 5));
    usbh_want(0);
    run_ms(2);
    return 1;
}
static int t_hub_no_drive(void)
{
    reset_world();
    rca(0);
    usbh_want(1);
    run_ms(5);
    lines = 2;
    run_ms(12000);
    show("two hubs, empty reader, no drive");
    assert(usbh.st == UH_DONE && !conflicts);
    assert(line_is(1, "No drive read"));
    assert(line_is(2, "top 05E3:0610 hub") && line_is(3, "1 1A40:0801 hub") && line_is(4, "1.2 05E3:0749 disk"));
    assert(!usbh.line[5][0] && line_is(7, "1.2: no card?"));
    usbh_want(0);
    run_ms(2);
    return 1;
}
/* (0.4.8) the second unit test: the stick reader on the top hub's port 3, here deaf until a Bulk-Only reset, with
 * two slots and the card in the second; the hub's own reader (1.2) empty */
static int t_reader_stick(void)
{
    int k;
    reset_world();
    rca(0);
    k = add(D_READER, 64, 0, 4);
    devs[k].maxlun = 1;
    devs[k].card_lun = 1;
    devs[k].deaf = 1;
    usbh_want(1);
    run_ms(5);
    lines = 2;
    run_ms(20000);
    show("stick reader: reset, card in slot 2");
    assert(usbh.st == UH_DONE && !conflicts && bulk_resets == 1);
    assert(line_is(1, "Through hubs: 4") && line_is(2, "ID 05E3:0749") && line_is(5, "Drive 14.3 GB, slot 2"));
    assert(line_is(6, "Disk: FAT32"));
    usbh_want(0);
    run_ms(2);
    return 1;
}
/* a reader that never answers: the reason keeps the error */
static int t_reader_deaf(void)
{
    int k;
    reset_world();
    rca(0);
    k = add(D_READER, 64, 0, 4);
    devs[k].deaf = 2;                                   /* (a reset does not help) */
    usbh_want(1);
    run_ms(5);
    lines = 2;
    run_ms(20000);
    show("stick reader that never answers");
    assert(line_is(1, "No drive read") && line_is(5, "4 05E3:0749 disk") && line_is(7, "4: no answer, stall"));
    usbh_want(0);
    run_ms(2);
    return 1;
}
/* (0.4.10) the files: what the page asks (view, cmd, sel), the main loop does (usbf_task) */
static void ask(uint8_t cmd) { usbh.cmd = cmd; run_ms(3); assert(usbh.cmd == UC_NONE); }
static int row_of(const char *name)                     /* its index in the folder (all rows read through windows) */
{
    uint32_t i;
    int r = -1;
    for (usbh.top = 0; usbh.top < usbh.count && r < 0; usbh.top += UH_ROWS) {
        ask(UC_WINDOW);
        for (i = 0; i < usbh.nent; i++)
            if (!strcmp(usbh.ent[i].name, name)) r = usbh.top + (int)i;
    }
    usbh.top = 0;
    ask(UC_WINDOW);
    return r;
}
static void pick(const char *name) { int r = row_of(name); assert(r >= 0); usbh.sel = (uint16_t)r; ask(UC_ENTER); }
static int t_files(void)
{
    FIL f;
    UINT got;
    uint8_t b[4096];
    uint32_t i, total = 0;
    reset_world();
    add(D_DRIVE, 64, -1, 0);
    usbh_want(1);
    run_ms(5);
    lines = 2;
    run_ms(400);
    assert(usbh.drive == 1 && line_is(6, "Disk: FAT32"));
    usbh.view = 1;
    ask(UC_LIST);
    printf("  files: / %u:", usbh.count);
    for (i = 0; i < usbh.nent; i++) printf(" [%s%s %u]", usbh.ent[i].name, usbh.ent[i].dir ? "/" : "", usbh.ent[i].size);
    printf(" {%s}\n", usbh.info);
    fflush(stdout);
    assert(usbh.count == 4 && usbh.ent_top == 0 && !usbh.path[0]);       /* ._README.TXT hidden */
    assert(row_of("README.TXT") >= 0 && row_of("BIG.BIN") >= 0 && row_of("._README.TXT") < 0);
    pick("SAMPLES");
    assert(!strcmp(usbh.path, "/SAMPLES") && usbh.count == 4);
    pick("Kick 808 Long Name.wav");
    printf("  Kick: [%s] [%s]\n", usbh.info, usbh.info2);
    fflush(stdout);
    assert(!strcmp(usbh.info, "44.1 kHz 16-bit mono 0.5 s") && !strcmp(usbh.info2, "Playing") && cp[0].on && up.n == 1);
    ask(UC_ENTER);                                      /* (0.4.11) OCT+ again: stopped */
    assert(!cp[0].on && !up.n && !strcmp(usbh.info2, "Stopped"));
    pick("drums");
    assert(!strcmp(usbh.path, "/SAMPLES/drums") && usbh.count == 1);
    pick("snare.wav");
    printf("  snare: [%s] [%s]\n", usbh.info, usbh.info2);
    fflush(stdout);
    assert(!strncmp(usbh.info, "48 kHz 24-bit stereo 0.2", 24) && up.n == 1);
    ask(UC_UP);                                         /* (0.4.13: it plays on through the folders) */
    assert(up.n == 1 && cp[0].on);
    assert(!strcmp(usbh.path, "/SAMPLES") && usbh.sel == 0);
    ask(UC_UP);
    assert(!usbh.path[0] && usbh.count == 4);
    pick("MANY");
    assert(usbh.count == 20 && usbh.nent == UH_ROWS && !strcmp(usbh.ent[0].name, "FILE01.BIN"));
    usbh.sel = 10; usbh.top = 5;                        /* (the page scrolled) */
    ask(UC_WINDOW);
    assert(usbh.ent_top == 5 && !strcmp(usbh.ent[0].name, "FILE06.BIN") && usbh.ent[UH_ROWS - 1u].size == 100);
    ask(UC_UP);
    pick("BIG.BIN");
    printf("  BIG.BIN: [%s] [%s]\n", usbh.info, usbh.info2);
    fflush(stdout);
    assert(!strcmp(usbh.info, "File, 64 KB") && !strncmp(usbh.info2, "Read 64 KB at ", 14));
    assert(f_open(&f, "/BIG.BIN", FA_READ) == FR_OK);   /* every byte as written */
    while (f_read(&f, b, sizeof b, &got) == FR_OK && got) {
        for (i = 0; i < got; i++) assert(b[i] == (uint8_t)(((total + i) * 7 + ((total + i) >> 9)) & 255));
        total += got;
    }
    f_close(&f);
    assert(total == 65536);
    lines = 0;                                          /* unplugged: the volume let go */
    run_ms(400);
    assert(!usbh.drive && !usbh.view && !uf_mounted);
    usbh_want(0);
    run_ms(2);
    return 1;
}
/* (0.4.11) a WAV playing: the ring holds the file's frames in order, the audio side plays them with no gaps while
 * the reading keeps it full, across the loop back to the start; a 22.05 kHz 24-bit mono file at half speed.
 * (0.4.13) Two at once, summed sample for sample; one stopped by OCT+ on it while the other plays on; four at once;
 * an error stops them all. */
static int32_t ramp(uint32_t i) { return (int32_t)((i * 37u) % 32768u) - 16384; }
static int32_t at_half(uint32_t k)                     /* tone24 (2,205 frames, looped) at half speed, frame k */
{
    uint32_t i = (k / 2u) % 2205u, j = (i + 1u) % 2205u;
    int32_t a = ramp(i), b = ramp(j);
    return (k & 1u) ? a + (((b - a) * 16384) >> 15) : a;
}
static void block(int32_t *out, uint32_t ticks)        /* one 64-frame block, then the timer and the main loop */
{
    uint32_t i;
    memset(out, 0, 2 * 64 * sizeof *out);
    card_render(out, 64);
    for (i = 0; i < ticks; i++) cs_tick();
    usbf_task();
    fm1_ms++;
}
static uint32_t gaps_all(void) { uint32_t s, g = 0; for (s = 0; s < CP_N; s++) g += cp[s].gaps; return g; }
static int t_play(void)
{
    int32_t out[2 * 64];
    uint32_t i, j, n = 11025, frames = 0, k24 = 0, bad = 0;
    reset_world();
    add(D_DRIVE, 64, -1, 0);
    usbh_want(1);
    run_ms(5);
    lines = 2;
    run_ms(400);
    usbh.view = 1;
    ask(UC_LIST);
    pick("SAMPLES");
    pick("tone.wav");
    printf("  tone: [%s] [%s] step %u\n", usbh.info, usbh.info2, cp[0].step);
    fflush(stdout);
    assert(!strcmp(usbh.info, "44.1 kHz 16-bit stereo 0.2 s") && cp[0].on && cp[0].step == 65536u && cp[0].ch == 2u);
    assert(cp[0].w >= cp[0].cap / 2u && cs.run && cs.st[0].next == 1u);   /* half filled first; one run of sectors */
    for (i = 0; i < 64u; i++) assert(cp[0].buf[2 * i] == ramp(i) && cp[0].buf[2 * i + 1] == -ramp(i));
    for (i = 0; i < usbh.nent; i++)                     /* its row says PLAY */
        assert(usbh.ent[i].play == !strcmp(usbh.ent[i].name, "tone.wav"));
    for (j = 0; j < 700u; j++) {                        /* 44,800 frames: four times round the file */
        block(out, 15);                                 /* (15 ticks: 1.45 ms of the 10 kHz timer, one block's time) */
        for (i = 0; i < 64u; i++, frames++) {
            int32_t v = ramp(frames % n);               /* -6 dB, as the ISR scales: (x * MASTER) >> 13 */
            if (out[2 * i] != (v * 4096) >> 13 || out[2 * i + 1] != (-v * 4096) >> 13) bad++;
        }
        if (j == 300u) { usbh.top = 1; ask(UC_WINDOW); usbh.top = 0; ask(UC_WINDOW); }   /* FatFs on the bus mid-play */
    }
    up.msg_ms -= 2000u; up.show_ms -= 250u; usbf_task(); /* (0.4.14: "Playing" is held 2 s, then the loads) */
    printf("  tone: %u frames, %u wrong, %u gaps, %u commands, [%s] [%s]\n", frames, bad, gaps_all(), cs.cmds, usbh.info2,
           usbh.info3);
    fflush(stdout);
    assert(!bad && !gaps_all() && !strncmp(usbh.info2, "1 on, CPU 0%, card ", 19) && !strncmp(usbh.info3, "gap 0, ", 7));
    pick("tone24.wav");                                 /* (0.4.13) a second one, beside the first */
    printf("  tone24: [%s] [%s] step %u\n", usbh.info, usbh.info2, cp[1].step);
    fflush(stdout);
    assert(!strcmp(usbh.info, "22.05 kHz 24-bit mono 0.1 s") && cp[1].step == 32768u && cp[1].ch == 1u && up.n == 2);
    for (j = 0; j < 400u; j++) {                        /* the two summed, sample for sample */
        block(out, 15);
        for (i = 0; i < 64u; i++, frames++, k24++) {
            int32_t v = ramp(frames % n), m = at_half(k24);
            if (out[2 * i] != ((v * 4096) >> 13) + ((m * 4096) >> 13) ||
                out[2 * i + 1] != ((-v * 4096) >> 13) + ((m * 4096) >> 13)) bad++;
        }
    }
    printf("  two: %u wrong, %u gaps\n", bad, gaps_all());
    fflush(stdout);
    assert(!bad && !gaps_all());
    pick("tone.wav");                                   /* OCT+ on a playing one: that one stops, the other plays on */
    assert(!cp[0].on && !cs.st[0].run && cp[1].on && up.n == 1);
    for (j = 0; j < 50u; j++) {
        block(out, 15);
        for (i = 0; i < 64u; i++, k24++)
            if (out[2 * i] != (at_half(k24) * 4096) >> 13) bad++;
    }
    assert(!bad && !gaps_all());
    pick("tone.wav");                                   /* four at once: tone, tone24, Kick, drums/snare */
    pick("Kick 808 Long Name.wav");
    pick("drums");
    pick("snare.wav");                                  /* (0.4.15) past the card's limit: refused */
    assert(up.n == 3 && !cp[3].on && !strcmp(usbh.info2, "Too much for the card"));
    uf_budget = 1000000u;                               /* (the test's card is fast: four, 604 KB/s) */
    pick("snare.wav");
    assert(up.n == 4 && cp[0].on && cp[1].on && cp[2].on && cp[3].on);
    for (j = 0; j < 400u; j++) block(out, 20);
    up.msg_ms -= 2000u; up.show_ms -= 250u; usbf_task();
    printf("  four: [%s] [%s] %u commands\n", usbh.info2, usbh.info3, cs.cmds);
    fflush(stdout);
    assert(!gaps_all() && !strncmp(usbh.info2, "4 on, CPU ", 10));
    ask(UC_UP);
    devs[0].deaf = 2;                                   /* (0.4.12) the card stops answering mid-play: all stop */
    for (i = 0; i < 2000u && up.n; i++) block(out, 15);
    printf("  card error: [%s]\n", usbh.info2);
    fflush(stdout);
    assert(!up.n && !cs.run && !cp[0].on && !cp[3].on && !strcmp(usbh.info2, "Stopped: card error"));
    devs[0].deaf = 0;
    pick("tone.wav");                                   /* and play again after */
    assert(up.n == 1 && cs.run && cp[0].on);
    lines = 0;                                          /* unplugged while playing: stopped */
    run_ms(400);
    assert(!up.n && !cp[0].on && !cs.run);
    usbh_want(0);
    run_ms(2);
    return 1;
}
static int t_files_reprobe(void)
{
    reset_world();
    add(D_KEYS, 8, -1, 0);                              /* no drive: OCT+ looks again */
    usbh_want(1);
    run_ms(5);
    lines = 2;
    run_ms(400);
    assert(!usbh.drive && line_is(5, "Not a drive, class 03"));
    setups = 0;
    ask(UC_PROBE);
    run_ms(400);
    assert(setups > 5 && line_is(5, "Not a drive, class 03"));
    ask(UC_LIST);                                       /* (no drive: refused) */
    assert(!strcmp(usbh.info, "Drive not reading"));
    usbh_want(0);
    run_ms(2);
    return 1;
}
static int t_hub_empty(void)
{
    reset_world();
    add(D_HUB, 64, -1, 0);
    usbh_want(1);
    run_ms(5);
    lines = 2;
    run_ms(5000);
    show("a hub with nothing on it");
    assert(line_is(5, "Hub, nothing on it"));
    usbh_want(0);
    run_ms(2);
    return 1;
}
static int t_keys(void)
{
    reset_world();
    add(D_KEYS, 8, -1, 0);
    usbh_want(1);
    run_ms(5);
    lines = 2;
    run_ms(400);
    show("keyboard");
    assert(line_is(2, "ID 046D:C31C") && line_is(3, "Logitech USB Keyboard") && line_is(5, "Not a drive, class 03"));
    usbh_want(0);
    run_ms(2);
    return 1;
}
static int t_not_ready(void)
{
    reset_world();
    add(D_DRIVE, 64, -1, 0);
    drive_not_ready = 3;
    usbh_want(1);
    run_ms(5);
    lines = 2;
    run_ms(2000);
    show("drive not ready at first");
    assert(scsi_tur_count == 4 && line_is(5, "Drive 14.3 GB") && line_is(6, "Disk: FAT32"));
    usbh_want(0);
    run_ms(2);
    return 1;
}
static int t_no_answer(void)
{
    reset_world();
    add(D_DRIVE, 64, -1, 0);
    sie_alive = 0;                                      /* the controller does not answer (no 5 V?) */
    usbh_want(1);
    run_ms(5);
    show("controller not answering");
    assert(line_is(0, "USB chip silent"));
    sie_alive = 1;                                      /* 5 V arrives with what is plugged in */
    run_ms(1100);
    assert(line_is(0, "Host mode on"));
    usbh_want(0);
    run_ms(2);
    assert(usbh.st == UH_OFF);
    return 1;
}
static int t_silent_device(void)
{
    reset_world();
    usbh_want(1);                                       /* D+ high but nothing answers: every wait ends */
    run_ms(5);
    lines = 2;
    run_ms(400);
    show("nothing answers");
    assert(usbh.st == UH_DONE && !strncmp(usbh.line[5], "Did not answer", 14));
    usbh_want(0);
    run_ms(2);
    return 1;
}

int main(void)
{
    card_load();
    int ok = t_drive(64) && t_drive(8) && t_hub() && t_hub_no_drive() && t_reader_stick() && t_reader_deaf() && t_hub_empty() && t_files() && t_play() && t_files_reprobe() && t_keys() && t_not_ready() && t_no_answer() && t_silent_device();
    printf("usb_host: %s\n", ok ? "all passed" : "FAILED");
    return !ok;
}
