/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat 0.4.7: the USB port as a host (firmware/src/usb_host.c) against a model of the controller in host mode
 * and of the devices on the other end: a mass storage flash drive (EP0 packets of 64 or 8 bytes), hubs with the drive
 * behind them, a card reader with two slots, a keyboard (not a drive), a drive that is not ready at first, and a
 * controller that does not answer. It tests the probe's logic (the requests, their order, the addresses, the
 * bulk-only transport), not the real controller: the bits it writes are the ones JieLi documents for MUSB host mode.
 * (0.4.10) The drive holds a FAT32 card made by an independent FAT implementation (tests/make_fat_fixture.py,
 * tests/fixtures/fat32_card.bin), read through usb_files.c's FatFs.
 * (0.4.11 to 0.4.20) WAVs streamed from the card (card_stream.c, card_play.c); frozen tracks recorded to it and played
 * back from it (card_freeze.c, usb_files.c frz_*), the card then checked by pyfatfs (tests/check_rec_card.py).
 * (0.5.5) The frozen track's stem runs past 16 bits (as a loud track's does): the take keeps it whole (FZ_HEAD).
 * (0.5.3) The drive test's page is gone: what it showed (its lines, the file list, its players and recorder) is no
 * longer tested here; the probe is checked by what it leaves (a drive ready, its address and slot), the streams by
 * starting them as a freeze does. */
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
#define CS_WAIT() cs_tick()                             /* (0.4.12) the timer's work while the main loop waits */
static uint8_t mix_stem_trk = 0xFFu;                    /* (0.4.19) as mix.c: the track the recorder takes */
/* (0.4.20) the sequencer, the mixer and the arrangement as the freeze code sees them: just what it reads and calls */
#define NTRK 4u
#define NPART 4u
#define CTL 64u                                         /* (this test's blocks) */
static struct { uint32_t playing; } song = {1};         /* (the sequencer as card_freeze.c sees it) */
#define t_playing (song.playing)
static uint32_t t_starts;
#define seq_start_count t_starts
#define SEQ_NOPASS 0xFFFFu
static uint16_t seq_pass_at[NTRK];
enum { P_LEVEL, P_MUTE, P_N };
typedef struct { int16_t p[P_N]; } track_t;
static track_t trk[NTRK];
static uint32_t trk_n(void) { return NTRK; }
static int32_t LEVEL_Q12[128];
static int32_t solo_gain[NPART] = {32768, 32768, 32768, 32768};
static uint8_t track_solo, fx_send_on;
static volatile uint8_t panic_req;
static uint16_t pattern_epoch;
static uint32_t fx_send_gain(const track_t *t) { (void)t; return 0; }
static int32_t fx_scale(int32_t a, uint32_t k) { return (a >> 16) * (int32_t)k + (int32_t)(((uint32_t)(a & 65535) * k) >> 16); }
static int32_t clamp(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }
static int32_t mix_stem_l[CTL], mix_stem_r[CTL], t_mix_l[CTL], t_mix_r[CTL];
static uint32_t mix_stem_n;
#define MIX_FRZ_FADE 32u
static volatile uint8_t mix_frozen, mix_frz_swap;
static uint8_t mix_frz_at[NPART];
static void (*mix_frz_block)(uint32_t n);
static int (*mix_frz_take)(track_t *t, uint32_t n);
static void (*mix_frz_after)(uint32_t n);
static void mix_add(int32_t l, int32_t r, uint32_t i, uint32_t send) { (void)send; t_mix_l[i] += l; t_mix_r[i] += r; }
enum { OCL_PHRASE, OCL_CHAIN, OCL_SONG };
#define OCL_NCHAIN 64u
#define OCL_NENT 16u
#define OCL_NROW 64u
static struct { uint8_t chain[OCL_NCHAIN][OCL_NENT][2]; uint8_t song[OCL_NROW][NTRK]; uint8_t tchain[NTRK], cur[NTRK], row[NTRK];
                uint8_t mode; } lists;
static void ocl_chain_span(uint32_t c, uint32_t *first, uint32_t *last)   /* (as oc_lists.c) */
{
    uint32_t f, l;
    for (f = 0; f < OCL_NENT && !lists.chain[c][f][0]; f++)
        ;
    if (f == OCL_NENT) { *first = *last = OCL_NENT; return; }
    for (l = OCL_NENT - 1u; !lists.chain[c][l][0]; l--)
        ;
    *first = f; *last = l;
}
#include "../firmware/src/card_play.c"
#include "../firmware/src/card_stream.c"
#include "../firmware/src/card_freeze.c"
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
/* (0.4.16) a sector written: replaces the one there, or joins the card */
static void card_put(uint32_t lba, const uint8_t *d)
{
    uint32_t i;
    for (i = 0; i < ncard; i++)
        if (card[i].lba == lba) { memcpy(card[i].d, d, 512); return; }
    card = realloc(card, (size_t)(ncard + 1u) * sizeof *card);
    card[ncard].lba = lba;
    memcpy(card[ncard].d, d, 512);
    ncard++;
}
static uint32_t card_writes, card_written;              /* (0.4.16) WRITE(10) commands, sectors */
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
static uint32_t bout_left, bout_lba, bout_n;            /* (0.4.16) a WRITE(10)'s data still to come, where, in sec */
static uint8_t bout_sec[512];
static void dev_bulk_out(const uint8_t *p, uint32_t n)
{
    udev_t *d = route();
    uint32_t len;
    int lun = p[13], card;
    assert(d && (d->kind == D_DRIVE || d->kind == D_READER) && d->configured == 1);
    if (bout_left) {                                    /* (0.4.16) the data of a WRITE(10), 64 bytes a packet */
        assert(n == 64u && n <= bout_left);
        memcpy(bout_sec + bout_n, p, n);
        bout_n += n;
        bout_left -= n;
        if (bout_n == 512u) { card_put(bout_lba++, bout_sec); bout_n = 0; card_written++; }
        if (!bout_left) csw(0, 0);
        return;
    }
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
    case 0x2A: {                                        /* (0.4.16) WRITE(10): the data comes next, then the CSW */
        uint32_t cnt = (uint32_t)p[22] << 8 | p[23];
        assert(len == cnt * 512u && cnt && cnt <= 64u && !(p[12] & 0x80u));
        bout_lba = (uint32_t)p[17] << 24 | (uint32_t)p[18] << 16 | (uint32_t)p[19] << 8 | p[20];
        bout_left = len;
        bout_n = 0;
        card_writes++;
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
    printf("  %s: %s, address %u, slot %u, error %d\n", title, usbh.drive ? "drive ready" : "no drive", usbh.addr, usbh.lun,
           usbh.err);
    fflush(stdout);
}

static int t_drive(int mps0)
{
    reset_world();
    add(D_DRIVE, mps0, -1, 0);
    usbh_want(1);
    run_ms(5);
    assert(usbh.st == UH_WAIT && usb.up == 0 && usb.detached == 1 && attached_host == 1 && usbh.sie);
    lines = 2;                                          /* plugged in: D+ high */
    run_ms(400);
    show(mps0 == 64 ? "drive, EP0 64" : "drive, EP0 8");
    assert(usbh.st == UH_DONE && !faddr_bad && usbh.drive && usbh.addr == 1 && !usbh.lun);
    assert(devs[0].addr == 1 && devs[0].configured == 1);
    lines = 0;                                          /* unplugged */
    run_ms(400);
    assert(usbh.st == UH_WAIT && !usbh.drive);
    usbh_want(0);                                       /* no track frozen: the device again */
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
    assert(usbh.drive && usbh.addr == 4 && !usbh.lun);
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
    assert(usbh.st == UH_DONE && !conflicts && !usbh.drive);
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
    assert(usbh.drive && usbh.lun == 1 && usbh.addr == devs[k].addr);
    usbh_want(0);
    run_ms(2);
    return 1;
}
/* a reader that never answers: no drive */
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
    assert(usbh.st == UH_DONE && !usbh.drive);
    usbh_want(0);
    run_ms(2);
    return 1;
}
/* (0.4.10) the card through FatFs: the folders, a long name, every byte of a 64 KB file; unplugged, the volume let go */
static int t_fs(void)
{
    FIL f;
    DIR d;
    FILINFO fi;
    UINT got;
    uint8_t b[4096];
    uint32_t i, total = 0, n = 0;
    reset_world();
    add(D_DRIVE, 64, -1, 0);
    usbh_want(1);
    run_ms(5);
    lines = 2;
    run_ms(400);
    assert(usbh.drive == 1 && uf_mount() == FR_OK && uf_mounted);
    assert(f_opendir(&d, "/SAMPLES") == FR_OK);
    while (f_readdir(&d, &fi) == FR_OK && fi.fname[0]) n += !strcmp(fi.fname, "Kick 808 Long Name.wav");
    f_closedir(&d);
    assert(n == 1);
    assert(f_open(&f, "/BIG.BIN", FA_READ) == FR_OK);   /* every byte as written */
    while (f_read(&f, b, sizeof b, &got) == FR_OK && got) {
        for (i = 0; i < got; i++) assert(b[i] == (uint8_t)(((total + i) * 7 + ((total + i) >> 9)) & 255));
        total += got;
    }
    f_close(&f);
    assert(total == 65536);
    printf("  files: /SAMPLES read, BIG.BIN %u bytes right\n", total);
    fflush(stdout);
    lines = 0;                                          /* unplugged: the volume let go */
    run_ms(400);
    assert(!usbh.drive && !uf_mounted);
    usbh_want(0);
    run_ms(2);
    return 1;
}
/* (0.4.11) WAVs streamed from the card into the players' rings (as frz_load starts a take): the ring holds the file's
 * frames in order, the audio side plays them with no gaps while the reading keeps it full, across the loop back to
 * the start; a 22.05 kHz 24-bit mono file at half speed beside it, sample for sample; the card stopping mid-way stops
 * them all. */
static int32_t ramp(uint32_t i) { return (int32_t)((i * 37u) % 32768u) - 16384; }
static int32_t at_half(uint32_t k)                     /* tone24 (2,205 frames, looped) at half speed, frame k */
{
    uint32_t i = (k / 2u) % 2205u, j = (i + 1u) % 2205u;
    int32_t a = ramp(i), b = ramp(j);
    return (k & 1u) ? a + (((b - a) * 16384) >> 15) : a;
}
static void stream_start(const char *path, uint32_t s)
{
    FILINFO fi;
    struct uf_wi w;
    struct cs_one *o = &cs.st[s];
    uint32_t t0, want;
    assert(f_stat(path, &fi) == FR_OK && f_open(&uf_pf, path, FA_READ) == FR_OK);
    assert(uf_wav_scan(&uf_pf, (uint32_t)fi.fsize, &w) && uf_extents(&uf_pf, (uint32_t)fi.fsize, o));
    f_close(&uf_pf);
    cp_setup(s, w.ch, w.rate);
    o->bytes = (uint8_t)(w.bits / 8u);
    o->bpf = (uint8_t)(o->bytes * w.ch);
    o->bps = w.rate * o->bpf;
    o->right = (uint8_t)(w.ch > 1u ? o->bytes : 0u);
    o->ch = cp[s].ch;
    o->data0 = w.data0;
    o->data = w.data;
    o->pos = 0;
    o->nc = 0;
    sie_wr(S_FADDR, usbh.addr);
    o->run = 1;
    cs_update_run();
    want = w.data / o->bpf < cp[s].cap / 2u ? w.data / o->bpf : cp[s].cap / 2u;
    for (t0 = fm1_ms; cp[s].w < want && !cs.err && fm1_ms - t0 < 1000u;) CS_WAIT();
    assert(cp[s].w >= want);
    cp[s].on = 1;
}
static int32_t st_l[64], st_r[64];
static void stream_block(uint32_t ticks)                /* one 64-frame block of the timer and the main loop */
{
    uint32_t i;
    for (i = 0; i < ticks; i++) cs_tick();
    usbf_task();
    fm1_ms++;
}
static uint32_t gaps_all(void) { uint32_t s, g = 0; for (s = 0; s < CP_N; s++) g += cp[s].gaps; return g; }
static int t_stream(void)
{
    uint32_t i, j, n = 11025, frames = 0, k24 = 0, bad = 0;
    reset_world();
    add(D_DRIVE, 64, -1, 0);
    usbh_want(1);
    run_ms(5);
    lines = 2;
    run_ms(400);
    assert(usbh.drive && uf_mount() == FR_OK);
    stream_start("/SAMPLES/tone.wav", 0);
    assert(cp[0].step == 65536u && cp[0].ch == 2u && cs.run && cs.st[0].next == 1u);   /* one run of sectors */
    for (i = 0; i < 64u; i++) assert(cp[0].buf[2 * i] == ramp(i) && cp[0].buf[2 * i + 1] == -ramp(i));
    for (j = 0; j < 700u; j++) {                        /* 44,800 frames: four times round the file */
        cp_pull(&cp[0], st_l, st_r, 0, 64);
        for (i = 0; i < 64u; i++, frames++)
            if (st_l[i] != ramp(frames % n) || st_r[i] != -ramp(frames % n)) bad++;
        stream_block(15);                               /* (15 ticks: 1.45 ms of the 10 kHz timer, one block's time) */
    }
    printf("  tone: %u frames, %u wrong, %u gaps\n", frames, bad, gaps_all());
    fflush(stdout);
    assert(!bad && !gaps_all());
    stream_start("/SAMPLES/tone24.wav", 1);             /* a second one beside it: 22.05 kHz 24-bit mono */
    assert(cp[1].step == 32768u && cp[1].ch == 1u);
    for (j = 0; j < 400u; j++) {
        cp_pull(&cp[0], st_l, st_r, 0, 64);
        for (i = 0; i < 64u; i++, frames++) if (st_l[i] != ramp(frames % n)) bad++;
        cp_pull(&cp[1], st_l, st_r, 0, 64);
        for (i = 0; i < 64u; i++, k24++) if (st_l[i] != at_half(k24) || st_r[i] != at_half(k24)) bad++;
        stream_block(15);
    }
    printf("  two: %u wrong, %u gaps\n", bad, gaps_all());
    fflush(stdout);
    assert(!bad && !gaps_all());
    devs[0].deaf = 2;                                   /* (0.4.12) the card stops answering mid-play: all stop */
    for (i = 0; i < 2000u && cs.run; i++) { cp_pull(&cp[0], st_l, st_r, 0, 64); stream_block(15); }
    assert(!cs.run && !cp[0].on && !cp[1].on && !cs.err);
    devs[0].deaf = 0;
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
    assert(usbh.st == UH_DONE && !usbh.drive);
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
    assert(usbh.st == UH_DONE && !usbh.drive && devs[0].configured == 1);
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
    assert(scsi_tur_count == 4 && usbh.drive);
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
    assert(!usbh.sie);
    sie_alive = 1;                                      /* 5 V arrives with what is plugged in */
    run_ms(1100);
    assert(usbh.sie);
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
    assert(usbh.st == UH_DONE && !usbh.drive && usbh.err);
    usbh_want(0);
    run_ms(2);
    return 1;
}

static void dump_card(const char *path)
{
    FILE *f = fopen(path, "wb");
    uint32_t i;
    assert(f);
    for (i = 0; i < ncard; i++) { fseek(f, (long)card[i].lba * 512L, SEEK_SET); fwrite(card[i].d, 1, 512, f); }
    fseek(f, 96L * 1024L * 512L - 1L, SEEK_SET);        /* (the whole 48 MB card) */
    fputc(0, f);
    fclose(f);
}
/* (0.4.20) frozen tracks (card_freeze.c, usb_files.c frz_*): a model sequencer marks each track's passes (PL frames
 * a pass, from the block after PLAY, as seq.c); a track's stem is periodic over its cycle (fz_val). KNOB 3 right
 * wakes the card and arms the freeze; PLAY; one cycle played, the next recorded and FZ_X frames past it; the take
 * blended, cut to the cycle, loaded; it comes in at the following cycle and from then on the track's output is its
 * stem again, sample for sample, cycle after cycle (no drift, no seam). The sequencer stopping stops it and it comes
 * in again at the next start, from its first frame; LEVEL scales it against the level it was frozen at; KNOB 3 left
 * unfreezes. A two-pass cycle (the chain mode) the same; a freeze cancelled, and one stopped mid-take, leave no
 * file. frz_cycle counts the chain and song modes' passes. */
static uint32_t fz_abs, fz_start_abs, fz_pl;           /* frames since the test's start; pass 0's; a pass's frames */
static int fz_seq;                                      /* the model sequencer runs */
static int32_t fz_val(uint32_t k, uint32_t p, int right) /* track k's stem, p frames into its cycle: (0.5.5) past 16 bits
                                                         * both ways, up to 4.6 times, as a loud track's is */
{
    return right ? (int32_t)((p * (17u + k) + 300u) % 300000u) - 150000 : (int32_t)((p * (13u + k) + k * 77u) % 240000u) - 120000;
}
static int32_t fz_file(int32_t v) { return v >> FZ_HEAD; }   /* (0.5.5) what the take holds: the headroom's bits down */
static uint32_t fz_cyc_frames(uint32_t k) { return fz_pl * (frz[k].passes ? frz[k].passes : 1u); }
static void fz_block(void)                              /* one 64-frame block: the sequencer, the mix, the card, the main loop */
{
    uint32_t i, k;
    memset(t_mix_l, 0, sizeof t_mix_l);
    memset(t_mix_r, 0, sizeof t_mix_r);
    for (k = 0; k < NTRK; k++) seq_pass_at[k] = SEQ_NOPASS;
    if (fz_seq && fz_abs + 64u > fz_start_abs) {       /* a pass begins in this block */
        uint32_t from = fz_abs > fz_start_abs ? fz_abs - fz_start_abs : 0u, p = (from + fz_pl - 1u) / fz_pl * fz_pl;
        if (fz_abs < fz_start_abs) p = 0;
        if (fz_start_abs + p < fz_abs + 64u)
            for (k = 0; k < NTRK; k++) seq_pass_at[k] = (uint16_t)(fz_start_abs + p - fz_abs);
    }
    frz_block(64);
    for (k = 0; k < NTRK; k++)
        if ((mix_frozen >> k) & 1u) frz_take(&trk[k], 64);
    mix_stem_n = 0;
    if (mix_stem_trk < NTRK) {                          /* (as mix.c: the picked track's stem) */
        uint32_t k2 = mix_stem_trk;
        for (i = 0; i < 64u; i++) {
            uint32_t a = fz_abs + i, p = a >= fz_start_abs ? (a - fz_start_abs) % fz_cyc_frames(k2) : 0u;
            mix_stem_l[i] = fz_val(k2, p, 0);
            mix_stem_r[i] = fz_val(k2, p, 1);
        }
        mix_stem_n = 64;
    }
    frz_after(64);
    for (i = 0; i < 15u; i++) cs_tick();
    usbf_task();
    fm1_ms++;
    fz_abs += 64u;
}
static void fz_play(void) { fz_seq = 1; t_playing = 1; t_starts++; fz_start_abs = fz_abs + 64u; }   /* PLAY (seq.c: pass 0 at
                                                         * the next block) */
static void fz_stop(void) { fz_seq = 0; t_playing = 0; }
/* blocks until track k's take plays, then cycles of it checked: the stem again, after the fade-in; frames wrong */
static uint32_t fz_check(uint32_t k, uint32_t blocks, int32_t g)   /* (g: the level against the frozen one, Q12) */
{
    uint32_t b, i, bad = 0, fade_end = 0;
    for (b = 0; b < 4000u && !((mix_frozen >> k) & 1u); b++) fz_block();
    if (!((mix_frozen >> k) & 1u)) return 1000000u;
    fade_end = fz_abs - 64u + mix_frz_at[k] + MIX_FRZ_FADE;   /* (the block it came in: faded in from its frame) */
    for (b = 0; b < blocks; b++) {
        uint32_t base = fz_abs;
        if (b) fz_block();
        for (i = 0; i < 64u; i++) {
            uint32_t a = (b ? base : fz_abs - 64u) + i, p = (a - fz_start_abs) % fz_cyc_frames(k);
            int32_t l = (fz_file(fz_val(k, p, 0)) * g) >> (12 - FZ_HEAD), r = (fz_file(fz_val(k, p, 1)) * g) >> (12 - FZ_HEAD);
            if (a < fade_end) continue;
            if (t_mix_l[i] != l || t_mix_r[i] != r) bad++;
        }
    }
    return bad;
}
static int t_freeze(void)
{
    uint32_t i, b, bad, s;
    FILINFO fi;
    reset_world();
    add(D_DRIVE, 64, -1, 0);
    for (i = 0; i < 128u; i++) LEVEL_Q12[i] = (int32_t)i * 32;
    for (i = 0; i < NTRK; i++) trk[i].p[P_LEVEL] = 100, trk[i].p[P_MUTE] = 0;
    fz_stop();
    fz_pl = 5003u;
    lists.mode = OCL_PHRASE;
    frz_init();
    lines = 2;
    frz_knob(1, 1);                                     /* KNOB 3 right on track 2: the card woken, the freeze armed */
    for (b = 0; b < 600u && frz[1].st != FZ_ARMED; b++) fz_block();
    printf("  freeze: %s, [%s], %u passes\n", frz_word(1, 0), ur.path, frz[1].passes);
    fflush(stdout);
    assert(frz[1].st == FZ_ARMED && usbh.want && usbh.drive && frz[1].passes == 1u && mix_stem_trk == 1u &&
           !strcmp(ur.path, "/ORANGECAT/T2_001.WAV") && !strcmp(frz_word(1, 0), "Ready") && !strcmp(frz_word(1, 1), "Wait"));
    usbh_want(0);                                       /* (asked to let go: a freezing track keeps the port a host) */
    assert(usbh.want);
    fz_play();
    for (b = 0; b < 2000u && frz[1].st != FZ_ON && frz[1].st != FZ_OFF; b++) fz_block();
    assert(f_stat("/ORANGECAT/T2_001.WAV", &fi) == FR_OK);
    printf("  frozen: %s after %u blocks, take %u bytes (cycle %u frames)\n", frz_word(1, 1), b, (unsigned)fi.fsize, fz_pl);
    fflush(stdout);
    assert(frz[1].st == FZ_ON && fi.fsize == 512u + fz_pl * 4u && cp[frz[1].slot].trk == 2u && !ur.on && mix_stem_trk == 0xFFu);
    {   /* the take on the card: the cycle exactly (its blend of equal ends changes nothing) */
        FIL fl;
        UINT got;
        int16_t sm[2];
        uint32_t wrong = 0, first = 0xFFFFFFFFu;
        assert(f_open(&fl, "/ORANGECAT/T2_001.WAV", FA_READ) == FR_OK && f_lseek(&fl, 512) == FR_OK);
        for (i = 0; i < fz_pl; i++) {
            assert(f_read(&fl, sm, 4, &got) == FR_OK && got == 4u);
            if (sm[0] != fz_file(fz_val(1, i, 0)) || sm[1] != fz_file(fz_val(1, i, 1))) { if (first == 0xFFFFFFFFu) first = i; wrong++; }
        }
        f_close(&fl);
        printf("  T2_001.WAV: %u frames wrong (first %d)\n", wrong, (int)first);
        fflush(stdout);
        assert(!wrong);
    }
    bad = fz_check(1, 400, 4096);                       /* five cycles of the take, as the track was */
    s = frz[1].slot;
    printf("  take playing: %u wrong over 400 blocks, %u gaps\n", bad, cp[s].gaps);
    fflush(stdout);
    assert(!bad && !cp[s].gaps);
    fz_stop();                                          /* STOP: the take stops, rewinds, waits */
    for (b = 0; b < 20u; b++) fz_block();
    assert(!(mix_frozen & 2u) && !cp[s].on && cp[s].armed && !strcmp(frz_word(1, 0), "Frozen"));
    fz_play();                                          /* PLAY again: from its first frame, at pass 0 */
    bad = fz_check(1, 200, 4096);
    printf("  again: %u wrong\n", bad);
    fflush(stdout);
    assert(!bad);
    trk[1].p[P_LEVEL] = 50;                             /* LEVEL against the level it was frozen at */
    for (b = 0; b < 3u; b++) fz_block();
    for (i = 0, bad = 0; i < 100u; i++) {              /* (it plays on: the next blocks at half) */
        uint32_t base = fz_abs, j;
        fz_block();
        for (j = 0; j < 64u; j++) {
            uint32_t p = (base + j - fz_start_abs) % fz_cyc_frames(1);
            if (t_mix_l[j] != ((fz_file(fz_val(1, p, 0)) * 2048) >> (12 - FZ_HEAD)) ||
                t_mix_r[j] != ((fz_file(fz_val(1, p, 1)) * 2048) >> (12 - FZ_HEAD))) bad++;
        }
    }
    printf("  LEVEL 50 of 100: %u wrong\n", bad);
    fflush(stdout);
    assert(!bad);
    trk[1].p[P_LEVEL] = 100;
    frz_knob(1, -1);                                    /* KNOB 3 left: the engine back */
    fz_block();
    assert(frz[1].st == FZ_OFF && !(mix_frozen & 2u) && !cs.st[s].run && !cp[s].trk && (panic_req & 2u));
    panic_req = 0;
    fz_stop();
    for (b = 0; b < 5u; b++) fz_block();
    lists.mode = OCL_CHAIN;                             /* a two-pass cycle: its chain, one step played twice */
    lists.tchain[2] = 5;
    memset(lists.chain[5], 0, sizeof lists.chain[5]);
    lists.chain[5][0][0] = 9; lists.chain[5][0][1] = 1;
    fz_pl = 3001u;
    frz_knob(2, 1);
    for (b = 0; b < 600u && frz[2].st != FZ_ARMED; b++) fz_block();
    assert(frz[2].st == FZ_ARMED && frz[2].passes == 2u && !strcmp(ur.path, "/ORANGECAT/T3_001.WAV"));
    fz_play();
    for (b = 0; b < 3000u && frz[2].st != FZ_ON; b++) fz_block();
    assert(frz[2].st == FZ_ON && f_stat("/ORANGECAT/T3_001.WAV", &fi) == FR_OK && fi.fsize == 512u + 2u * fz_pl * 4u);
    bad = fz_check(2, 400, 4096);
    printf("  two-pass cycle: take %u bytes, %u wrong\n", (unsigned)fi.fsize, bad);
    fflush(stdout);
    assert(!bad);
    frz_knob(2, -1);
    fz_block();
    fz_stop();
    lists.mode = OCL_PHRASE;
    frz_knob(0, 1);                                     /* armed, then KNOB 3 left: cancelled, no file */
    for (b = 0; b < 600u && frz[0].st != FZ_ARMED; b++) fz_block();
    assert(frz[0].st == FZ_ARMED && !strcmp(ur.path, "/ORANGECAT/T1_001.WAV"));
    frz_knob(0, -1);
    fz_block();
    assert(frz[0].st == FZ_OFF && !ur.on && f_stat("/ORANGECAT/T1_001.WAV", &fi) == FR_NO_FILE);
    frz_knob(0, 1);                                     /* stopped mid-take: dropped, "STOP" */
    for (b = 0; b < 600u && frz[0].st != FZ_ARMED; b++) fz_block();
    fz_play();
    for (b = 0; b < 3000u && frz[0].st != FZ_REC; b++) fz_block();
    for (b = 0; b < 20u; b++) fz_block();
    fz_stop();
    for (b = 0; b < 3u; b++) fz_block();
    printf("  stopped mid-take: %s\n", frz_word(0, 0));
    fflush(stdout);
    assert(frz[0].st == FZ_OFF && !strcmp(frz_word(0, 0), "Stop") && f_stat("/ORANGECAT/T1_001.WAV", &fi) == FR_NO_FILE);
    lists.mode = OCL_CHAIN;                             /* the cycle counts: a chain with a rest, REPS 2 and 3 */
    lists.tchain[1] = 3;
    memset(lists.chain[3], 0, sizeof lists.chain[3]);
    lists.chain[3][0][0] = 5; lists.chain[3][0][1] = 1;
    lists.chain[3][2][0] = 7; lists.chain[3][2][1] = 2;
    lists.chain[3][1][1] = 0;                           /* (a rest between: one pass) */
    assert(frz_cycle(1) == 6u);
    lists.mode = OCL_SONG;                              /* its block of rows 2..4, chain 3 each: 18 */
    memset(lists.song, 0, sizeof lists.song);
    for (i = 2; i <= 4u; i++) lists.song[i][1] = 4;
    lists.row[1] = 3;
    assert(frz_cycle(1) == 18u);
    lists.mode = OCL_PHRASE;
    assert(frz_cycle(1) == 1u);
    {   /* (0.5.3) the card stops answering while a take plays: the track back to its engine, "Error" */
        frz_knob(3, 1);
        for (b = 0; b < 600u && frz[3].st != FZ_ARMED; b++) fz_block();
        fz_play();
        for (b = 0; b < 3000u && frz[3].st != FZ_ON; b++) fz_block();
        assert(frz[3].st == FZ_ON);
        for (b = 0; b < 200u && !((mix_frozen >> 3) & 1u); b++) fz_block();
        assert((mix_frozen >> 3) & 1u);
        dump_card("build/host/rec_card.img");           /* (tests/check_rec_card.py reads it: T2, T3 and T4's takes) */
        devs[0].deaf = 2;
        for (b = 0; b < 2000u && frz[3].st != FZ_OFF; b++) fz_block();
        printf("  card gone while frozen: %s\n", frz_word(3, 1));
        fflush(stdout);
        assert(frz[3].st == FZ_OFF && !strcmp(frz_word(3, 1), "Error") && !cs.run);
        devs[0].deaf = 0;
        fz_stop();
    }
    frz_drop_all("");
    usbh_want(0);
    run_ms(2);
    return 1;
}
int main(void)
{
    card_load();
    int ok = t_drive(64) && t_drive(8) && t_hub() && t_hub_no_drive() && t_reader_stick() && t_reader_deaf() && t_hub_empty() && t_fs() && t_stream() && t_keys() && t_not_ready() && t_no_answer() && t_silent_device() && t_freeze();
    printf("usb_host: %s\n", ok ? "all passed" : "FAILED");
    return !ok;
}
