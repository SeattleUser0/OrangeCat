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
static int str_eq(const char *a, const char *b) { while (*a && *a == *b) a++, b++; return *a == *b; }   /* (0.6.1) */
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
/* (0.4.20) the sequencer, the mixer and the arrangement as the freeze code sees them: just what it reads and calls.
 * (0.5.6) A freeze belongs to a phrase: each track's banks name phrases (bank_pid), trk[].pattern the bank playing,
 * pattern_next the one queued; seq.c's quiet bits and its hook; the pool's CRCs; the record store in RAM. */
#define NTRK 4u
#define NPART 4u
#define NPAT 8u
#define CTL 64u                                         /* (this test's blocks) */
enum { G_BPM, G_N };
static struct { uint32_t playing; int16_t g[G_N]; } song = {1, {120}};   /* (the sequencer as card_freeze.c sees it) */
#define t_playing (song.playing)
static uint32_t t_starts;
#define seq_start_count t_starts
#define SEQ_NOPASS 0xFFFFu
static uint16_t seq_pass_at[NTRK];
static volatile uint8_t seq_frz_quiet;                  /* (0.5.6, seq.c) a track's notes quiet: its take plays the pass */
static int (*seq_frz_hook)(uint32_t k);
static volatile uint8_t seq_frz_lock;                   /* (0.5.6, seq.c) tracks on a frozen phrase: nothing recorded */
enum { P_LEVEL, P_MUTE, P_N };
typedef struct { int16_t p[P_N]; volatile uint8_t pattern, pattern_next; } track_t;
static track_t trk[NTRK];
static uint8_t bank_pid[NTRK][NPAT];
static uint32_t trk_n(void) { return NTRK; }
static int32_t LEVEL_Q12[128];
static int32_t solo_gain[NPART] = {32768, 32768, 32768, 32768};
static uint8_t track_solo;
static volatile uint8_t panic_req;
static int32_t fx_scale(int32_t a, uint32_t k) { return (a >> 16) * (int32_t)k + (int32_t)(((uint32_t)(a & 65535) * k) >> 16); }
static int32_t clamp(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }
static int32_t mix_stem_l[CTL], mix_stem_r[CTL], t_mix_l[CTL], t_mix_r[CTL];
static uint32_t mix_stem_n;
static volatile uint8_t mix_frozen;
static void (*mix_frz_block)(uint32_t n);
static int (*mix_frz_take)(track_t *t, uint32_t n);
static void (*mix_frz_after)(uint32_t n);
static void mix_add(int32_t l, int32_t r, uint32_t i) { t_mix_l[i] += l; t_mix_r[i] += r; }   /* (0.6.0) */
static void mix_send(int32_t x, uint32_t i) { (void)x; (void)i; }
static int t_busy;                                      /* transport_busy (a count-in) */
static int transport_busy(void) { return t_busy; }
static struct { uint32_t hash[64]; } ocp;               /* (oc_pool.c) each pool phrase's CRC */
static uint32_t t_bank_hash[NTRK][NPAT];                /* each bank's steps' CRC as they are now */
static int t_keeps;
static uint32_t ocl_bank_hash(uint32_t k, uint32_t b) { return t_bank_hash[k][b]; }
static int ocl_keep(uint32_t k, uint32_t b)             /* (oc_lists.c) the bank kept to the pool: a number if it had none */
{
    t_keeps++;
    if (!bank_pid[k][b]) bank_pid[k][b] = (uint8_t)(k * NPAT + b + 1u);
    ocp.hash[bank_pid[k][b] - 1u] = t_bank_hash[k][b];
    return 0;
}
/* (0.6.1) the take folders and the browser: the music's name (project.c's proj_name), the phrases a slot names
 * (oc_project.c's ocj.names), the browser's rows (project.c's staging RAM) */
static char t_music_name[13];
static uint32_t t_slot_names[4][2];
#define FRZ_MUSIC_NAME() t_music_name
#define FRZ_SLOT_NAMES(s, i) ((t_slot_names[(s) & 3u][(i) >> 5] >> ((i) & 31u)) & 1u)
static uint32_t t_fb_rows[1024];
static uint8_t t_fb_gen;
#define FB_SCRATCH() ((fb_ent_t *)t_fb_rows)
#define FB_BORROW() ((void)0)
#define FB_GEN() t_fb_gen
static int ocl_keep_any(uint32_t k, uint32_t b) { return ocl_keep(k, b); }   /* (0.6.1: the stub numbers empty ones too) */
/* (Orange Cat 0.8.0) the asset pack, as card_browse.c's installer sees it (oc_pack.c itself is tested by oc_pack_test.c): a header
 * check, the banks flag, and the three calls recording what they were given */
#define OC_PACK_HOST 1
#define SMP_FEED() ((void)0)
enum { PK_MISSING, PK_OK };
#define OC_PACK_LEN 2048u
#define OC_PACK_HDR 512u
static uint32_t t_pk_begun, t_pk_at, t_pk_ended, t_pk_sum;
static uint8_t t_pk_hdr[512];
static uint32_t pk_rd32(const uint8_t *p) { return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static uint32_t pk_rd16(const uint8_t *p) { return p[0] | (uint32_t)p[1] << 8; }
static uint32_t pk_header_state(const uint8_t *h) { return pk_rd32(h) == 0x4B50434Fu ? PK_OK : PK_MISSING; }
static uint32_t oc_pack_info(uint32_t *len, uint32_t *hash) { (void)len; (void)hash; return 0; }
static int oc_pack_begin(uint32_t len) { t_pk_begun++; t_pk_at = OC_PACK_HDR; t_pk_sum = 0; return len == OC_PACK_LEN ? 0 : -1; }
static int oc_pack_write(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *q = src;
    if (off != t_pk_at) return -1;
    for (uint32_t i = 0; i < n; i++) t_pk_sum += q[i];
    t_pk_at += n;
    return 0;
}
static int oc_pack_end(const void *h) { if (t_pk_at != OC_PACK_LEN) return -1; memcpy(t_pk_hdr, h, sizeof t_pk_hdr); t_pk_ended++; return 0; }
/* (Orange Cat 0.10.0) an engine module, as card_browse.c's installer sees it (oc_module.c itself is tested by oc_module_test.c): the identifiers it
 * checks the package against, and the calls recording what they were given */
#define OC_MODULES 1
#define FW_API_IFACE 0xA1B2C3D4u
#define FW_API_ADDR 0x02001100u
#define FW_API_VERSION 3u
#define FW_MOD_CODE_OFF 0x100u
#define OC_PACK_HASH 0x5EEDu
typedef struct { uint8_t z[60]; } fw_api_t;
static int t_plan = 0, t_md_fail, t_inplace;
static uint32_t t_md_begun, t_md_engine, t_md_start, t_md_len, t_md_at, t_md_sum, t_md_ended;
static uint8_t t_md_page[256];
static int oc_mod_plan(uint32_t engine) { (void)engine; return t_plan; }
/* (0.12.1) three starts at sectors 0, 6 and 4 (a start with no sector here is 99: no variant matches), the blocker oc_mod_blocked_by names, the engines' names */
static const uint8_t t_sect[8] = { 0, 6, 4, 99, 99, 99, 99, 99 };
#define OC_MOD_START_SECTOR(i) ((uint32_t)t_sect[(i) & 7u])
static int t_blocked = -1;
static int oc_mod_blocked_by(uint32_t engine) { (void)engine; return t_blocked; }
static struct { uint32_t engine[3]; } oc_mod = { { 14u, 14u, 19u } };
#define NENGINES 32u
typedef struct { const char *name; } t_engine_t;
static const t_engine_t t_shapes = { "SHAPES" }, t_prophet = { "PROPHET" }, t_other = { "OTHER" };
static const t_engine_t *const ENGINES[NENGINES] = { &t_other, &t_other, &t_other, &t_other, &t_other, &t_other, &t_other, &t_other, &t_other, &t_other, &t_other, &t_other, &t_other, &t_other,
    &t_shapes, &t_other, &t_other, &t_other, &t_other, &t_prophet };
static int oc_mod_inplace(uint32_t engine, uint32_t start) { (void)engine; (void)start; return t_inplace; }   /* (0.10.1) */
static int oc_mod_begin(uint32_t engine, uint32_t start, uint32_t len)
{
    t_md_begun++; t_md_engine = engine; t_md_start = start; t_md_len = len; t_md_at = FW_MOD_CODE_OFF; t_md_sum = 0;
    return t_md_fail == 1 ? -2 : 0;
}
static int oc_mod_write(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *q = src;
    if (off != t_md_at) return -1;
    for (uint32_t i = 0; i < n; i++) t_md_sum += q[i];
    t_md_at += n;
    return 0;
}
static int oc_mod_end(const void *h) { if (t_md_at != t_md_len || t_md_fail == 2) return -1; memcpy(t_md_page, h, sizeof t_md_page); t_md_ended++; return 0; }
#include "../firmware/src/oc_store.c"                   /* (0.5.6) the frozen phrases' record, in RAM */
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
/* (0.4.20) frozen tracks, (0.5.6) frozen phrases (card_freeze.c, usb_files.c frz_*): a model sequencer marks each
 * track's passes (PL frames a pass, from the block after PLAY, as seq.c), takes a queued bank at a pass's start and
 * asks seq_frz_hook whether the take plays the pass (as seq.c); a track's stem is periodic over the pass (fz_val).
 * KNOB 3 right (stopped) keeps the bank as a phrase, wakes the card and arms the freeze; PLAY; one pass played, the
 * next recorded and FZ_X frames past it; the take blended, cut to the pass, the phrase frozen and its take loaded;
 * it comes in at the following pass and from then on the track's output is its stem again, sample for sample, pass
 * after pass (no drift, no seam), its notes quiet. Another phrase's pass stops it (the notes play); queued again, it
 * is loaded ahead and comes back at its pass. Another track playing the phrase plays the take too. STOP stops it,
 * PLAY brings it in from its first frame; LEVEL scales it; another tempo plays the notes; KNOB 3 left unfreezes the
 * phrase at once. Refused: no phrase number or changes not kept while playing ("Save?"). A freeze cancelled, and one
 * stopped mid-take, leave no file. The table of frozen phrases kept and read back (the record store). */
static uint32_t fz_abs, fz_start_abs, fz_pl;           /* frames since the test's start; pass 0's; a pass's frames */
static int fz_seq;                                      /* the model sequencer runs */
static int32_t fz_val(uint32_t k, uint32_t p, int right) /* track k's stem, p frames into its pass: (0.5.5) past 16 bits
                                                         * both ways, up to 4.6 times, as a loud track's is */
{
    return right ? (int32_t)((p * (17u + k) + 300u) % 300000u) - 150000 : (int32_t)((p * (13u + k) + k * 77u) % 240000u) - 120000;
}
static int32_t fz_file(int32_t v) { return v >> FZ_HEAD; }   /* (0.5.5) what the take holds: the headroom's bits down */
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
            for (k = 0; k < NTRK; k++) {
                seq_pass_at[k] = (uint16_t)(fz_start_abs + p - fz_abs);
                if (trk[k].pattern_next < NPAT) { trk[k].pattern = trk[k].pattern_next; trk[k].pattern_next = 0xFFu; }
                if (seq_frz_hook && seq_frz_hook(k)) seq_frz_quiet |= (uint8_t)(1u << k);
                else seq_frz_quiet &= (uint8_t)~(1u << k);
            }
    }
    frz_block(64);
    for (k = 0; k < NTRK; k++)
        if ((mix_frozen >> k) & 1u) frz_take(&trk[k], 64);
    mix_stem_n = 0;
    if (mix_stem_trk < NTRK) {                          /* (as mix.c: the picked track's stem) */
        uint32_t k2 = mix_stem_trk;
        for (i = 0; i < 64u; i++) {
            uint32_t a = fz_abs + i, p = a >= fz_start_abs ? (a - fz_start_abs) % fz_pl : 0u;
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
static void fz_blocks(uint32_t n) { while (n--) fz_block(); }
/* blocks until track k's take plays, then blocks of it checked against track src's stem (the track it was frozen
 * on): frames wrong; its notes must be quiet throughout */
static uint32_t fz_check(uint32_t k, uint32_t src, uint32_t blocks, int32_t g)   /* (g: the level against the frozen
                                                         * one, Q12) */
{
    uint32_t b, i, bad = 0, from;
    for (b = 0; b < 4000u && !((mix_frozen >> k) & 1u); b++) fz_block();
    if (!((mix_frozen >> k) & 1u)) return 1000000u;
    from = fz_abs - 64u + fzi[k].play_from;             /* (the block it came in: from its pass's first frame) */
    for (b = 0; b < blocks; b++) {
        uint32_t base = fz_abs;
        if (b) fz_block();
        if (!((seq_frz_quiet >> k) & 1u)) bad++;
        for (i = 0; i < 64u; i++) {
            uint32_t a = (b ? base : fz_abs - 64u) + i, p = (a - fz_start_abs) % fz_pl;
            int32_t l = (fz_file(fz_val(src, p, 0)) * g) >> (12 - FZ_HEAD), r = (fz_file(fz_val(src, p, 1)) * g) >> (12 - FZ_HEAD);
            if (a < from) continue;
            if (t_mix_l[i] != l || t_mix_r[i] != r) bad++;
        }
    }
    return bad;
}
/* the blocks up to the next pass's start and past it (n blocks after) */
static void fz_to_pass(uint32_t after)
{
    uint32_t b, k;
    for (b = 0; b < 4000u; b++) {
        int at = 0;
        fz_block();
        for (k = 0; k < NTRK; k++) at |= seq_pass_at[k] != SEQ_NOPASS;
        if (at) break;
    }
    fz_blocks(after);
}
static int t_freeze(void)
{
    uint32_t i, b, bad, s;
    FILINFO fi;
    reset_world();
    add(D_DRIVE, 64, -1, 0);
    for (i = 0; i < 128u; i++) LEVEL_Q12[i] = (int32_t)i * 32;
    for (i = 0; i < NTRK; i++) trk[i].p[P_LEVEL] = 100, trk[i].p[P_MUTE] = 0, trk[i].pattern = 0, trk[i].pattern_next = 0xFFu;
    memset(bank_pid, 0, sizeof bank_pid);
    for (i = 0; i < NTRK * NPAT; i++) t_bank_hash[i / NPAT][i % NPAT] = 0x1000u + i;
    song.g[G_BPM] = 120;
    fz_stop();
    fz_pl = 5003u;
    frz_init();
    frz_table_load();                                   /* (nothing kept: nothing frozen; the pool's CRCs checked) */
    assert(frz_pool_hash == ocp.hash);
    lines = 2;
    frz_knob(1, 1);                                     /* KNOB 3 right on track 2 (stopped): bank kept as phrase 9 */
    for (b = 0; b < 600u && frz[1].st != FZ_ARMED; b++) fz_block();
    printf("  freeze: %s, [%s], phrase %u\n", frz_word(1, 0), ur.path, frz[1].phrase);
    fflush(stdout);
    assert(frz[1].st == FZ_ARMED && usbh.want && usbh.drive && t_keeps == 1 && bank_pid[1][0] == 9u && frz[1].phrase == 9u &&
           mix_stem_trk == 1u && !strcmp(ur.path, "/ORANGECAT/UNTITLED/T2_001.WAV") && !strcmp(frz_word(1, 0), "Ready") &&
           !strcmp(frz_word(1, 1), "Wait") && frz_state(1) == 1u);
    usbh_want(0);                                       /* (asked to let go: a freezing track keeps the port a host) */
    assert(usbh.want);
    fz_play();
    for (b = 0; b < 2000u && frz[1].st != FZ_OFF; b++) fz_block();
    assert(f_stat("/ORANGECAT/UNTITLED/T2_001.WAV", &fi) == FR_OK);
    printf("  frozen: %s after %u blocks, take %u bytes (pass %u frames)\n", frz_word(1, 1), b, (unsigned)fi.fsize, fz_pl);
    fflush(stdout);
    assert(frz[1].st == FZ_OFF && fi.fsize == 512u + fz_pl * 4u && !ur.on && mix_stem_trk == 0xFFu && frzp[8].take == 1u &&
           frzp[8].trk == 1u && frzp[8].frames == fz_pl && frzp[8].lvl == 100 && frzp[8].bpm == 120 && frzp[8].hash == 0x1008u &&
           frzp_dirty && frz_phrase_frozen(9) && frz_track_frozen(1) && !frz_track_frozen(0) && frz_state(1) == 2u &&
           !strcmp(frz_word(1, 0), "Frozen"));
    s = fzt[1].slot;
    assert(s < CP_N && fzt[1].phr == 9u && cp[s].trk == 2u && cp[s].armed && seq_frz_lock == 2u);   /* (loaded ahead of
                                                         * the next pass; nothing to be recorded into it) */
    {   /* the take on the card: the pass exactly (its blend of equal ends changes nothing) */
        FIL fl;
        UINT got;
        int16_t sm[2];
        uint32_t wrong = 0, first = 0xFFFFFFFFu;
        assert(f_open(&fl, "/ORANGECAT/UNTITLED/T2_001.WAV", FA_READ) == FR_OK && f_lseek(&fl, 512) == FR_OK);
        for (i = 0; i < fz_pl; i++) {
            assert(f_read(&fl, sm, 4, &got) == FR_OK && got == 4u);
            if (sm[0] != fz_file(fz_val(1, i, 0)) || sm[1] != fz_file(fz_val(1, i, 1))) { if (first == 0xFFFFFFFFu) first = i; wrong++; }
        }
        f_close(&fl);
        printf("  T2_001.WAV: %u frames wrong (first %d)\n", wrong, (int)first);
        fflush(stdout);
        assert(!wrong);
    }
    bad = fz_check(1, 1, 400, 4096);                    /* five passes of the take, as the track was, its notes quiet */
    printf("  take playing: %u wrong over 400 blocks, %u gaps\n", bad, cp[s].gaps);
    fflush(stdout);
    assert(!bad && !cp[s].gaps);
    fz_stop();                                          /* STOP: the take stops, rewinds, waits */
    fz_blocks(20);
    assert(!(mix_frozen & 2u) && !cp[s].on && cp[s].armed && !strcmp(frz_word(1, 0), "Frozen") && !seq_frz_quiet);
    frz_table_save();                                   /* stopped: the table kept */
    assert(!frzp_dirty);
    {   /* read back as at boot; an entry of another version is not read */
        frzp_t keep = frzp[8];
        frzd_t keepd = frzd;                            /* (0.6.1: its folder's table with it) */
        memset(frzp, 0, sizeof frzp);
        frz_table_load();
        assert(!memcmp(&frzp[8], &keep, sizeof keep) && frz_phrase_frozen(9));
        assert(!strcmp(frzp[8].name, "T2_001.WAV") && !strcmp(frzd.d[frzp[8].dir], "ORANGECAT/UNTITLED") &&
               frzp[8].want == frzp[8].dir);             /* (0.6.1: its folder, the project's: no name, UNTITLED) */
        frzp[8].ver = 9;
        frzp_dirty = 1;
        frz_table_save();
        frz_table_load();
        assert(!frzp[8].take);
        frzp[8] = keep;
        assert(!ocs_write(OCS_FREEZE, 0, frzp, 100));   /* (a record of another size: nothing read) */
        frz_table_load();
        assert(!frzp[8].take);
        frzp[8] = keep;
        frzd = keepd;
        frzp_dirty = 1;
        frz_table_save();
        frz_table_load();
        assert(frz_phrase_frozen(9));
        ocp.hash[8] ^= 1u;                              /* (the phrase changed since: no longer frozen) */
        assert(!frz_phrase_frozen(9));
        ocp.hash[8] ^= 1u;
    }
    fz_play();                                          /* PLAY again: from its first frame, at pass 0 */
    bad = fz_check(1, 1, 200, 4096);
    printf("  again: %u wrong\n", bad);
    fflush(stdout);
    assert(!bad);
    trk[1].p[P_LEVEL] = 50;                             /* LEVEL against the level it was frozen at */
    fz_blocks(3);
    for (i = 0, bad = 0; i < 100u; i++) {              /* (it plays on: the next blocks at half) */
        uint32_t base = fz_abs, j;
        fz_block();
        for (j = 0; j < 64u; j++) {
            uint32_t p = (base + j - fz_start_abs) % fz_pl;
            if (t_mix_l[j] != ((fz_file(fz_val(1, p, 0)) * 2048) >> (12 - FZ_HEAD)) ||
                t_mix_r[j] != ((fz_file(fz_val(1, p, 1)) * 2048) >> (12 - FZ_HEAD))) bad++;
        }
    }
    printf("  LEVEL 50 of 100: %u wrong\n", bad);
    fflush(stdout);
    assert(!bad);
    trk[1].p[P_LEVEL] = 100;
    bank_pid[1][1] = 10;                                /* another phrase queued (not frozen): its pass plays its notes */
    trk[1].pattern_next = 1;
    fz_to_pass(2);
    printf("  another phrase: take %s, notes %s\n", (mix_frozen & 2u) ? "on" : "off", (seq_frz_quiet & 2u) ? "quiet" : "on");
    fflush(stdout);
    assert(!(mix_frozen & 2u) && !(seq_frz_quiet & 2u) && !cp[s].on && fzt[1].slot == CP_N && !strcmp(frz_word(1, 1), "") &&
           !(seq_frz_lock & 2u));
    trk[1].pattern_next = 0;                            /* the frozen one queued again: loaded ahead, back at its pass */
    fz_blocks(3);
    assert(fzt[1].phr == 9u && fzt[1].slot < CP_N && cp[fzt[1].slot].armed && !(mix_frozen & 2u));
    bad = fz_check(1, 1, 200, 4096);
    printf("  back to the frozen phrase: %u wrong\n", bad);
    fflush(stdout);
    assert(!bad);
    bank_pid[0][2] = 9;                                 /* track 1 on phrase 9 too: the take recorded on track 2 */
    trk[0].pattern_next = 2;
    assert(!frz_track_frozen(0));
    bad = fz_check(0, 1, 200, 8192);                    /* (both tracks play it: the mix holds it twice) */
    printf("  phrase 9 on track 1: %u wrong (slots %u and %u)\n", bad, fzt[0].slot, fzt[1].slot);
    fflush(stdout);
    assert(!bad && frz_track_frozen(0) && fzt[0].slot != fzt[1].slot && (mix_frozen & 3u) == 3u);
    frz_knob(1, -1);                                    /* KNOB 3 left: phrase 9 unfrozen, both tracks' notes at once */
    fz_blocks(3);
    printf("  unfrozen: %s, takes %x, notes quiet %x\n", frz_word(1, 1), mix_frozen, seq_frz_quiet);
    fflush(stdout);
    assert(!frzp[8].take && frzp_dirty && !(mix_frozen & 3u) && !(seq_frz_quiet & 3u) && fzt[0].slot == CP_N &&
           fzt[1].slot == CP_N && !frz_track_frozen(1) && f_stat("/ORANGECAT/UNTITLED/T2_001.WAV", &fi) == FR_OK);
    trk[0].pattern_next = 0;
    fz_stop();
    fz_blocks(5);
    frz_knob(2, 1);                                     /* track 3 frozen again stopped; then another tempo */
    for (b = 0; b < 600u && frz[2].st != FZ_ARMED; b++) fz_block();
    assert(frz[2].st == FZ_ARMED && !strcmp(ur.path, "/ORANGECAT/UNTITLED/T3_001.WAV") && frz[2].phrase == 17u);
    fz_play();
    for (b = 0; b < 3000u && frz[2].st != FZ_OFF; b++) fz_block();
    assert(frz_phrase_frozen(17) && f_stat("/ORANGECAT/UNTITLED/T3_001.WAV", &fi) == FR_OK && fi.fsize == 512u + fz_pl * 4u);
    bad = fz_check(2, 2, 100, 4096);
    assert(!bad);
    song.g[G_BPM] = 121;
    fz_to_pass(2);
    printf("  another tempo: %s, take %s\n", frz_word(2, 1), (mix_frozen & 4u) ? "on" : "off");
    fflush(stdout);
    assert(!strcmp(frz_word(2, 1), "Tempo") && !(mix_frozen & 4u) && !(seq_frz_quiet & 4u) && frz_state(2) == 2u);
    song.g[G_BPM] = 120;                                /* back: the take again */
    bad = fz_check(2, 2, 100, 4096);
    assert(!bad);
    frz_knob(2, 1);                                     /* KNOB 3 right on a frozen phrase: nothing new */
    fz_block();
    assert(frz[2].st == FZ_OFF && frz_phrase_frozen(17));
    frz_knob(3, 1);                                     /* playing: a bank with no phrase number cannot be frozen */
    fz_block();
    printf("  playing, no number: %s\n", frz_word(3, 1));
    fflush(stdout);
    assert(frz[3].st == FZ_OFF && !strcmp(frz_word(3, 1), "Save?"));
    bank_pid[3][0] = 25;                                /* nor one whose changes are not kept yet */
    ocp.hash[24] = 0x77u;
    frz_knob(3, 1);
    fz_block();
    assert(frz[3].st == FZ_OFF && !strcmp(frz_word(3, 1), "Save?"));
    fz_stop();
    fz_blocks(3);
    frz_knob(0, 1);                                     /* armed, then KNOB 3 left: cancelled, no file */
    for (b = 0; b < 600u && frz[0].st != FZ_ARMED; b++) fz_block();
    assert(frz[0].st == FZ_ARMED && !strcmp(ur.path, "/ORANGECAT/UNTITLED/T1_001.WAV"));
    frz_knob(0, -1);
    fz_block();
    assert(frz[0].st == FZ_OFF && !ur.on && f_stat("/ORANGECAT/UNTITLED/T1_001.WAV", &fi) == FR_NO_FILE && !frz_phrase_frozen(1));
    frz_knob(0, 1);                                     /* stopped mid-take: dropped, "STOP" */
    for (b = 0; b < 600u && frz[0].st != FZ_ARMED; b++) fz_block();
    fz_play();
    for (b = 0; b < 3000u && frz[0].st != FZ_REC; b++) fz_block();
    fz_blocks(20);
    fz_stop();
    fz_blocks(3);
    printf("  stopped mid-take: %s\n", frz_word(0, 0));
    fflush(stdout);
    assert(frz[0].st == FZ_OFF && !strcmp(frz_word(0, 0), "Stop") && f_stat("/ORANGECAT/UNTITLED/T1_001.WAV", &fi) == FR_NO_FILE &&
           !frz_phrase_frozen(1));
    {   /* (0.5.3) the card stops answering while a take plays: the phrase plays its notes, "Error"; still frozen */
        fz_pl = 3001u;
        frz_knob(3, -1);
        frz_knob(3, 1);                                 /* (stopped: phrase 25 kept, its CRC the bank's) */
        for (b = 0; b < 600u && frz[3].st != FZ_ARMED; b++) fz_block();
        assert(frz[3].st == FZ_ARMED && frz[3].phrase == 25u);
        fz_play();
        for (b = 0; b < 3000u && frz[3].st != FZ_OFF; b++) fz_block();
        assert(frz_phrase_frozen(25));
        for (b = 0; b < 2000u && !((mix_frozen >> 3) & 1u); b++) fz_block();
        assert((mix_frozen >> 3) & 1u);
        devs[0].deaf = 2;
        for (b = 0; b < 2000u && fzt[3].slot != CP_N; b++) fz_block();
        printf("  card gone while frozen: %s\n", frz_word(3, 1));
        fflush(stdout);
        assert(fzt[3].slot == CP_N && !strcmp(frz_word(3, 1), "Error") && !cs.run && frz_phrase_frozen(25) &&
               !(mix_frozen & 8u));
        devs[0].deaf = 0;
        fz_stop();
    }
    frz_drop_all("");
    memset(frzp, 0, sizeof frzp);
    memset(bank_pid, 0, sizeof bank_pid);
    fz_blocks(2);
    usbh_want(0);
    run_ms(2);
    return 1;
}
/* (0.6.1) a WAV of `frames` frames on the card: ch channels at rate, 16-bit (its samples: the frame number) */
static void tf_wav(const char *path, uint32_t ch, uint32_t rate, uint32_t frames)
{
    FIL fl;
    UINT bw;
    uint8_t h[44];
    uint32_t i, data = frames * 2u * ch;
    static const uint8_t H[16] = {'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E', 'f', 'm', 't', ' '};
    memcpy(h, H, 16);
    for (i = 0; i < 4u; i++) h[4 + i] = (uint8_t)((data + 36u) >> (8u * i));
    h[16] = 16; h[17] = h[18] = h[19] = 0; h[20] = 1; h[21] = 0; h[22] = (uint8_t)ch; h[23] = 0;
    for (i = 0; i < 4u; i++) h[24 + i] = (uint8_t)(rate >> (8u * i)), h[28 + i] = (uint8_t)((rate * 2u * ch) >> (8u * i));
    h[32] = (uint8_t)(2u * ch); h[33] = 0; h[34] = 16; h[35] = 0;
    memcpy(h + 36, "data", 4);
    for (i = 0; i < 4u; i++) h[40 + i] = (uint8_t)(data >> (8u * i));
    assert(f_open(&fl, path, FA_CREATE_ALWAYS | FA_WRITE) == FR_OK && f_write(&fl, h, 44, &bw) == FR_OK && bw == 44u);
    for (i = 0; i < frames * ch; i++) {
        int16_t v = (int16_t)(i / ch);
        assert(f_write(&fl, &v, 2, &bw) == FR_OK && bw == 2u);
    }
    f_close(&fl);
}
static int tf_is(const char *path) { FILINFO fi; return f_stat(path, &fi) == FR_OK; }
static void tf_settle(void)                             /* main-loop passes until no move is left (stopped) */
{
    for (uint32_t b = 0; b < 400u && frz_pending(); b++) fz_block();
    fz_blocks(3);
}
static void tf_entry(uint32_t id, uint32_t k, uint32_t take, uint32_t frames, uint32_t dir)   /* a frozen phrase as kept */
{
    frzp_t *f = &frzp[id - 1u];
    memset(f, 0, sizeof *f);
    f->take = (uint16_t)take; f->trk = (uint8_t)k; f->ver = FZP_VER; f->lvl = 100; f->bpm = 120;
    f->frames = frames; f->hash = ocp.hash[id - 1u] = 0x5000u + id; f->dir = f->want = (uint8_t)dir;
    frz_take_name(f->name, k, take);
}
/* (0.6.1) the takes' folders: named, renamed, into a folder that is there already; the browser */
static int t_folders(void)
{
    uint32_t b, i, un, jam;
    FILINFO fi;
    reset_world();
    add(D_DRIVE, 64, -1, 0);
    for (i = 0; i < 128u; i++) LEVEL_Q12[i] = (int32_t)i * 32;
    for (i = 0; i < NTRK; i++) trk[i].p[P_LEVEL] = 100, trk[i].p[P_MUTE] = 0, trk[i].pattern = 0, trk[i].pattern_next = 0xFFu;
    memset(bank_pid, 0, sizeof bank_pid);
    song.g[G_BPM] = 120;
    fz_stop();
    frz_init();
    frz_table_load();
    lines = 2;
    usbh_want(1);
    for (b = 0; b < 600u && !usbh.drive; b++) fz_block();
    assert(usbh.drive && uf_mount() == FR_OK);
    /* the three takes t_freeze left in UNTITLED, kept again as frozen phrases 9, 17, 25 */
    un = frzd_get("ORANGECAT/UNTITLED");
    tf_entry(9, 1, 1, 5003, un);
    tf_entry(17, 2, 1, 5003, un);
    tf_entry(25, 3, 1, 3001, un);
    t_slot_names[0][0] = (1u << 8) | (1u << 16);        /* (slot A's project names phrases 9 and 17, not 25) */
    /* 1. music with no name saved as JAM in slot A: its phrases' takes go to /ORANGECAT/JAM, 25's stays */
    frz_named("", "JAM", 0, 0);
    jam = frzd_find("ORANGECAT/JAM");
    assert(jam < FZD_N && frzp[8].want == jam && frzp[16].want == jam && frzp[24].want == un && frzp[8].dir == un);
    tf_settle();
    assert(tf_is("/ORANGECAT/JAM/T2_001.WAV") && tf_is("/ORANGECAT/JAM/T3_001.WAV") && tf_is("/ORANGECAT/UNTITLED/T4_001.WAV") &&
           !tf_is("/ORANGECAT/UNTITLED/T2_001.WAV") && frzp[8].dir == jam && frzp[16].dir == jam && frzp[24].dir == un);
    printf("  folders: no name -> JAM: phrase 9 and 17's takes moved, 25's stayed\n");
    /* 2. JAM renamed DUB 2: the folder renamed whole (a file no phrase names goes with it) */
    tf_wav("/ORANGECAT/JAM/NOTE.WAV", 2, 44100, 300);
    frz_named("JAM", "DUB 2", 0, 1);
    frz_table_save();                                   /* (kept with the move still to do: a power cut here) */
    frz_table_load();
    assert(frz_pending() && frzd.mv_from[0] == jam);
    tf_settle();
    i = frzd_find("ORANGECAT/DUB 2");
    assert(i < FZD_N && !tf_is("/ORANGECAT/JAM") && tf_is("/ORANGECAT/DUB 2/NOTE.WAV") && tf_is("/ORANGECAT/DUB 2/T2_001.WAV") &&
           frzp[8].dir == i && frzp[8].want == i && frzp[16].dir == i);
    printf("  folders: JAM renamed DUB 2, whole\n");
    /* 3. renamed TAKEN, a folder already there with a T2_001.WAV: each take moved in, the name taken renumbered */
    assert(f_mkdir("/ORANGECAT/TAKEN") == FR_OK);
    tf_wav("/ORANGECAT/TAKEN/T2_001.WAV", 2, 44100, 256);
    frz_named("DUB 2", "TAKEN", 0, 1);
    tf_settle();
    i = frzd_find("ORANGECAT/TAKEN");
    assert(tf_is("/ORANGECAT/TAKEN/T2_002.WAV") && tf_is("/ORANGECAT/TAKEN/T3_001.WAV") && tf_is("/ORANGECAT/DUB 2/NOTE.WAV") &&
           !strcmp(frzp[8].name, "T2_002.WAV") && frzp[8].dir == i && frzp[16].dir == i && f_stat("/ORANGECAT/TAKEN/T2_001.WAV", &fi) == FR_OK &&
           fi.fsize == 44u + 256u * 4u);
    printf("  folders: into TAKEN (there already): T2_001.WAV taken there, the take is T2_002.WAV\n");
    /* 4. the card out when named: the phrase still plays from where its take is; it moves when the card is back */
    {
        uint32_t pend;
        usbh.drive = 0;
        frz_named("TAKEN", "LATER", 0, 1);
        fz_blocks(5);
        pend = frz_pending();
        frz_entry_path(ur.path, &frzp[8]);
        assert(pend && !strcmp(ur.path, "/ORANGECAT/TAKEN/T2_002.WAV"));
        usbh.drive = 1;
        tf_settle();
        assert(tf_is("/ORANGECAT/LATER/T2_002.WAV") && tf_is("/ORANGECAT/LATER/T2_001.WAV") && !frz_pending());
        printf("  folders: named with the card out: moved once it was back\n");
    }
    /* 5. a 0.5.6 .. 0.6.0 record (16-byte entries): read, its takes in /ORANGECAT itself */
    {
        uint8_t v1[64 * 16];
        frzp_t keep[FZP_N];
        memcpy(keep, frzp, sizeof keep);
        memset(v1, 0, sizeof v1);
        memcpy(v1 + 8 * 16, &frzp[8], 16);
        v1[8 * 16 + 3] = 1;                             /* (ver 1) */
        v1[8 * 16 + 0] = 7;                             /* (take 7) */
        assert(!ocs_write(OCS_FREEZE, 0, v1, sizeof v1));
        frz_table_load();
        assert(frzp[8].take == 7u && frzp[8].dir == 0u && frzp[8].want == 0u && !strcmp(frzp[8].name, "T2_007.WAV") &&
               frzp[8].frames == 5003u && !frzp[16].take);
        frz_entry_path(ur.path, &frzp[8]);
        assert(!strcmp(ur.path, "/ORANGECAT/T2_007.WAV"));
        memcpy(frzp, keep, sizeof keep);
        printf("  folders: a 0.6.0 record read, its take in /ORANGECAT\n");
    }
    /* 6. the browser: it opens in the music's folder; up to the root; CLIPS; a mono WAV refused; LOOP.WAV given
     * to track 1's phrase, which then plays it */
    assert(f_mkdir("/CLIPS") == FR_OK);
    tf_wav("/CLIPS/LOOP.WAV", 2, 44100, 4000);
    tf_wav("/CLIPS/MONO.WAV", 1, 44100, 4000);
    tf_wav("/CLIPS/A FAR TOO LONG NAME.WAV", 2, 44100, 4000);
    str_cpy(t_music_name, "later", sizeof t_music_name);   /* (case aside: the folder LATER) */
    fb_open(0);
    for (b = 0; b < 200u && fb.st != FB_READY; b++) fz_block();
    printf("  browser: %u [%s] up %u n %u msg %s\n", fb.st, fb.path, fb.up, fb.n, fb.msg);
    fflush(stdout);
    assert(fb.st == FB_READY && frz_same_dir(fb.path, "/ORANGECAT/LATER") && fb.up && fb.n == 4u);   /* .. T2_001 T2_002 T3_001 */
    {
        char nm[44], v[16];
        assert(fb_row(1, nm, sizeof nm, v, sizeof v) && !strcmp(nm, "T2_001.WAV") && !strcmp(v, "0.0 s"));
        assert(fb_row(2, nm, sizeof nm, v, sizeof v) && !strcmp(nm, "T2_002.WAV") && !strcmp(v, "0.1 s"));
    }
    assert(!fb_back());
    fz_block();
    assert(fb.st == FB_READY && !strcmp(fb.path, "/ORANGECAT") && !strcmp(fb_where(), "ORANGECAT"));
    assert(!fb_back());
    fz_block();
    assert(fb.st == FB_READY && !strcmp(fb.path, "/") && !fb.up && !strcmp(fb_where(), "Card"));
    for (i = 0; i < fb.n && !(fb_ent(i) && !strcmp(fb_ent(i)->name, "CLIPS")); i++) ;
    assert(i < fb.n && fb_ent(i)->dir);
    fb.sel = (uint16_t)i;
    assert(!fb_ok());
    fz_block();
    assert(fb.st == FB_READY && !strcmp(fb.path, "/CLIPS") && fb.n == 4u && !strcmp(fb_ent(1)->name, "A FAR TOO LONG NAME.WAV") &&
           !strcmp(fb_ent(2)->name, "LOOP.WAV") && !strcmp(fb_ent(3)->name, "MONO.WAV"));
    t_bank_hash[0][0] = 0x1234u;
    fb.sel = 3;
    assert(!fb_ok() && fb.st == FB_FAIL && !strcmp(fb.msg, "Needs 44.1k 16b stereo") && fb.relist);
    assert(!fb_back());
    fz_block();
    fb.sel = 1;
    assert(!fb_ok() && fb.st == FB_FAIL && !strcmp(fb.msg, "Name too long"));
    assert(!fb_ok());
    fz_block();
    fb.sel = 2;
    assert(fb_ok() && fb.st == FB_OFF && !uf_browse);
    {
        uint32_t id = bank_pid[0][0];
        frzp_t *f = &frzp[id - 1u];
        assert(id && f->take && f->trk == 0u && f->frames == 4000u && f->bpm == 120 && f->lvl == 100 && f->hash == 0x1234u &&
               !strcmp(frzd.d[f->dir], "CLIPS") && f->want == f->dir && !strcmp(f->name, "LOOP.WAV") && frz_track_frozen(0) &&
               frzp_dirty);
        fz_pl = 4000u;
        fz_play();
        for (b = 0; b < 3000u && !(mix_frozen & 1u); b++) fz_block();
        assert(mix_frozen & 1u);
        fz_stop();
        fz_blocks(3);
        printf("  browser: CLIPS/LOOP.WAV given to track 1's phrase %u: it plays; MONO.WAV and a long name refused\n", id);
    }
    /* 7. (0.8.0) a .PAK in the browser: listed with its words; another size refused; no question about banks (0.9.1); installed in
     * order, the header read again last */
    {
        FIL pf;
        UINT bw;
        uint8_t blob[2048];
        uint32_t k, sum = 0;
        char nm[44], v[16];
        for (k = 0; k < sizeof blob; k++) blob[k] = (uint8_t)(k * 7u + 3u);
        memcpy(blob, "OCPK", 4);
        blob[12] = (uint8_t)(OC_PACK_LEN & 255u);
        blob[13] = (uint8_t)(OC_PACK_LEN >> 8);
        blob[14] = blob[15] = 0;
        for (k = 512; k < sizeof blob; k++) sum += blob[k];
        assert(f_mkdir("/PACKS") == FR_OK);
        assert(f_open(&pf, "/PACKS/OK.PAK", FA_WRITE | FA_CREATE_ALWAYS) == FR_OK && f_write(&pf, blob, sizeof blob, &bw) == FR_OK && bw == sizeof blob);
        f_close(&pf);
        assert(f_open(&pf, "/PACKS/SHORT.PAK", FA_WRITE | FA_CREATE_ALWAYS) == FR_OK && f_write(&pf, blob, 1024, &bw) == FR_OK);
        f_close(&pf);
        blob[0] = 'X';
        assert(f_open(&pf, "/PACKS/BAD.PAK", FA_WRITE | FA_CREATE_ALWAYS) == FR_OK && f_write(&pf, blob, sizeof blob, &bw) == FR_OK);
        f_close(&pf);
        blob[0] = 'O';
        fb_open(0);
        for (b = 0; b < 200u && fb.st != FB_READY; b++) fz_block();
        strcpy(fb.path, "/PACKS");
        fb.st = FB_LIST;
        fz_block();
        assert(fb.st == FB_READY && fb.n == 4u);          /* .. BAD.PAK OK.PAK SHORT.PAK */
        assert(fb_row(2, nm, sizeof nm, v, sizeof v) && !strcmp(nm, "OK.PAK") && !strcmp(v, "Pack"));
        fb.sel = 3;                                       /* the wrong size */
        assert(!fb_ok() && fb.st == FB_FAIL && !strcmp(fb.msg, "Not this build's pack") && !t_pk_begun);
        assert(!fb_ok());
        fz_block();
        fb.sel = 1;                                       /* the right size, the wrong magic */
        assert(!fb_ok() && fb.st == FB_FAIL && !strcmp(fb.msg, "Not this build's pack") && !t_pk_begun);
        assert(!fb_ok());
        fz_block();
        fb.sel = 2;
        assert(fb_ok() && fb.st == FB_OFF && !uf_browse && fb_pack_done());
        assert(t_pk_begun == 1u && t_pk_ended == 1u && t_pk_at == OC_PACK_LEN && t_pk_sum == sum &&
               !memcmp(t_pk_hdr, blob, 512));
        fb_open(0);
        assert(!fb_pack_done());
        fb_close();
        printf("  browser: .PAK listed; another size or build refused; no question about banks (0.9.1); the pack written in order, header last\n");
        /* (0.12.2, Part 2e) with the transport running (a count-in, or the song playing) the pack is refused whole: "Stop first", nothing begun, the row stays; stopped it goes on */
        fb_open(0);
        for (b = 0; b < 200u && fb.st != FB_READY; b++) fz_block();
        strcpy(fb.path, "/PACKS"); fb.st = FB_LIST; fz_block();
        fb.sel = 2; t_pk_begun = t_pk_ended = 0; t_busy = 1;
        assert(!fb_ok() && fb.st == FB_FAIL && !strcmp(fb.msg, "Stop first") && !t_pk_begun);
        assert(!fb_ok()); fz_block();                    /* (the message is acknowledged) */
        t_busy = 0; song.playing = 1; fb.sel = 2;
        assert(!fb_ok() && fb.st == FB_FAIL && !strcmp(fb.msg, "Stop first") && !t_pk_begun);
        assert(!fb_ok()); fz_block();
        song.playing = 0; fb.sel = 2;
        assert(fb_ok() && fb.st == FB_OFF && t_pk_begun == 1u && t_pk_ended == 1u);
        fb_open(0); assert(!fb_pack_done()); fb_close();
        printf("  browser: the pack is refused while the transport runs (a count-in or the song), installed once stopped\n");

    }
    /* 8. (0.10.0) a .OCM in the browser: listed ("Module"); a file that is not a package, a package for another firmware or with no image for the named place
     * refused with nothing erased; installed: the image for the place oc_mod_plan names, the code in order from byte 256, the header page last */
    {
        FIL pf;
        UINT bw;
        static uint8_t ocm[8192];
        uint32_t k, o, sum0 = 0, sum1 = 0, crc;
        const uint32_t base = 56u + 32u + 4u;
        char nm[44], v[16];
        #define W32(at, x) do { ocm[(at)] = (uint8_t)(x); ocm[(at) + 1] = (uint8_t)((x) >> 8); ocm[(at) + 2] = (uint8_t)((x) >> 16); ocm[(at) + 3] = (uint8_t)((x) >> 24); } while (0)
        #define MKOCM(iface_) do { memset(ocm, 0, sizeof ocm); W32(0, 0x464D434Fu); ocm[4] = 1; ocm[6] = 2; W32(8, (iface_)); W32(12, FW_API_ADDR); W32(16, 14u); \
            W32(20, 11136u); W32(24, OC_PACK_HASH); W32(44, FW_API_VERSION); W32(48, 60u); W32(52, 6u); \
            for (k = 0; k < 2u; k++) { W32(56 + 16 * k, 6u * k); W32(56 + 16 * k + 4, 1000u); W32(56 + 16 * k + 8, 0u); W32(56 + 16 * k + 12, base + 1000u * k); \
                for (o = 0; o < 1000u; o++) ocm[base + 1000u * k + o] = (uint8_t)(o * 5u + k * 11u + 1u); } \
            crc = ocs_crc32(ocm, 88); W32(88, crc); } while (0)
        MKOCM(FW_API_IFACE);
        for (o = 0x100; o < 1000u; o++) { sum0 += ocm[base + o]; sum1 += ocm[base + 1000u + o]; }
        assert(f_mkdir("/MODS") == FR_OK);
        assert(f_open(&pf, "/MODS/GOOD.OCM", FA_WRITE | FA_CREATE_ALWAYS) == FR_OK && f_write(&pf, ocm, base + 2000u, &bw) == FR_OK && bw == base + 2000u);
        f_close(&pf);
        MKOCM(FW_API_IFACE ^ 1u);
        assert(f_open(&pf, "/MODS/OTHER.OCM", FA_WRITE | FA_CREATE_ALWAYS) == FR_OK && f_write(&pf, ocm, base + 2000u, &bw) == FR_OK);
        f_close(&pf);
        MKOCM(FW_API_IFACE);
        ocm[60] ^= 1;/* a bad table CRC */
        assert(f_open(&pf, "/MODS/DAMAGED.OCM", FA_WRITE | FA_CREATE_ALWAYS) == FR_OK && f_write(&pf, ocm, base + 2000u, &bw) == FR_OK);
        f_close(&pf);
        assert(f_open(&pf, "/MODS/JUNK.OCM", FA_WRITE | FA_CREATE_ALWAYS) == FR_OK && f_write(&pf, "not a package at all, only text, long enough to read 64 bytes of it....", 70, &bw) == FR_OK);
        f_close(&pf);
        fb_open(0);
        for (b = 0; b < 200u && fb.st != FB_READY; b++) fz_block();
        strcpy(fb.path, "/MODS");
        fb.st = FB_LIST;
        fz_block();
        assert(fb.st == FB_READY && fb.n == 5u);          /* .. DAMAGED GOOD JUNK OTHER */
        assert(fb_row(2, nm, sizeof nm, v, sizeof v) && !strcmp(nm, "GOOD.OCM") && !strcmp(v, "Module"));
        fb.sel = 1;                                       /* the damaged table */
        assert(!fb_ok() && fb.st == FB_FAIL && !strcmp(fb.msg, "Not a module") && !t_md_begun);
        assert(!fb_ok()); fz_block();
        fb.sel = 3;                                       /* not a package */
        assert(!fb_ok() && fb.st == FB_FAIL && !strcmp(fb.msg, "Not a module") && !t_md_begun);
        assert(!fb_ok()); fz_block();
        fb.sel = 4;                                       /* another firmware's identifier */
        assert(!fb_ok() && fb.st == FB_FAIL && !strcmp(fb.msg, "Not for this firmware") && !t_md_begun);
        assert(!fb_ok()); fz_block();
        t_plan = -1;
        fb.sel = 2;                                       /* a good one, no place free */
        assert(!fb_ok() && fb.st == FB_FAIL && !strcmp(fb.msg, "No place free") && !t_md_begun);
        assert(!fb_ok()); fz_block();
        t_blocked = 2;                                    /* (0.12.1) no place free because the Prophet's module stands at start 2: it is named, never erased */
        fb.sel = 2;
        assert(!fb_ok() && fb.st == FB_FAIL && !strcmp(fb.msg, "PROPHET at 2: remove it") && !t_md_begun && !t_md_ended);
        assert(!fb_ok()); fz_block();
        t_blocked = 0;                                    /* SHAPES at start 0 */
        oc_mod.engine[0] = 14u;
        fb.sel = 2;
        assert(!fb_ok() && fb.st == FB_FAIL && !strcmp(fb.msg, "SHAPES at 0: remove it") && !t_md_begun && !t_md_ended);
        assert(!fb_ok()); fz_block();
        t_blocked = -1;
        t_plan = 5;                                       /* a place the package has no image for */
        fb.sel = 2;
        assert(!fb_ok() && fb.st == FB_FAIL && !strcmp(fb.msg, "No image for it") && !t_md_begun);
        assert(!fb_ok()); fz_block();
        t_md_fail = 1;                                    /* begin fails (a flash error) */
        t_plan = 1;
        fb.sel = 2;
        assert(!fb_ok() && fb.st == FB_FAIL && !strcmp(fb.msg, "Install failed") && t_md_begun == 1u && !t_md_ended);
        assert(!fb_ok()); fz_block();
        t_md_fail = 2;                                    /* end refuses */
        t_md_begun = 0;
        fb.sel = 2;
        assert(!fb_ok() && fb.st == FB_FAIL && !strcmp(fb.msg, "Install failed") && t_md_begun == 1u && !t_md_ended);
        assert(!fb_ok()); fz_block();
        t_md_fail = 0; t_md_begun = 0;
        t_plan = 1;
        t_inplace = 1;                                    /* (0.10.1) no other place free: the first OCT+ only says the old module is erased first */
        fb.sel = 2;
        assert(!fb_ok() && fb.st == FB_FAIL && !strcmp(fb.msg, "Erases old first. Again") && !t_md_begun && fb.relist == 2u);
        assert(!fb_ok() && fb.st == FB_READY && fb.sel == 2);        /* the word gone, the rows and the row kept */
        fb.sel = 1;
        assert(!fb_ok() && fb.st == FB_FAIL && !strcmp(fb.msg, "Not a module"));   /* another row pressed: the question is dropped */
        assert(!fb_ok()); fz_block();
        fb.sel = 2;
        assert(!fb_ok() && fb.st == FB_FAIL && !strcmp(fb.msg, "Erases old first. Again") && !t_md_begun);   /* asked again for this row */
        assert(!fb_ok() && fb.st == FB_READY);
        assert(fb_ok() && fb.st == FB_OFF && !uf_browse && fb_pack_done() == 2 && t_md_begun == 1u);          /* the second OCT+ on the row goes on */
        t_inplace = 0; t_md_begun = t_md_ended = 0;
        fb_open(0);
        for (b = 0; b < 200u && fb.st != FB_READY; b++) fz_block();
        strcpy(fb.path, "/MODS"); fb.st = FB_LIST; fz_block();
        fb.sel = 2;
        assert(fb_ok() && fb.st == FB_OFF && !uf_browse && fb_pack_done() == 2);   /* beside: no question */
        assert(t_md_begun == 1u && t_md_ended == 1u && t_md_engine == 14u && t_md_start == 1u && t_md_len == 1000u && t_md_at == 1000u && t_md_sum == sum1 &&
               !memcmp(t_md_page, ocm + base + 1000u, 256));
        t_md_begun = t_md_ended = 0; t_plan = 0;
        fb_open(0);
        for (b = 0; b < 200u && fb.st != FB_READY; b++) fz_block();
        strcpy(fb.path, "/MODS"); fb.st = FB_LIST; fz_block();
        fb.sel = 2;
        assert(fb_ok() && fb_pack_done() == 2 && t_md_start == 0u && t_md_sum == sum0 && !memcmp(t_md_page, ocm + base, 256));
        fb_open(0);
        assert(!fb_pack_done());
        fb_close();
        /* (0.12.2, Part 2e) the module is refused while the transport runs (a count-in, or the song): "Stop first", nothing begun */
        t_md_begun = t_md_ended = 0;
        fb_open(0);
        for (b = 0; b < 200u && fb.st != FB_READY; b++) fz_block();
        strcpy(fb.path, "/MODS"); fb.st = FB_LIST; fz_block();
        fb.sel = 2; t_busy = 1;
        assert(!fb_ok() && fb.st == FB_FAIL && !strcmp(fb.msg, "Stop first") && !t_md_begun);
        assert(!fb_ok()); fz_block();
        t_busy = 0; song.playing = 1; fb.sel = 2;
        assert(!fb_ok() && fb.st == FB_FAIL && !strcmp(fb.msg, "Stop first") && !t_md_begun);
        assert(!fb_ok()); fz_block();
        song.playing = 0; fb.sel = 2;
        assert(fb_ok() && !t_md_begun == 0 && t_md_ended == 1u);
        fb_open(0); assert(!fb_pack_done()); fb_close();
        t_md_begun = t_md_ended = 0;
        printf("  browser: the module is refused while the transport runs, installed once stopped\n");
        /* (0.12.1) the Prophet's package: engine 19, two variants for the sectors 0 and 4 (starts 0 and 2); the start the unit names picks the variant by its sector */
        {   uint32_t sumP[2] = { 0, 0 };
            MKOCM(FW_API_IFACE);
            W32(16, 19u); W32(56, 0u); W32(56 + 16, 4u);
            crc = ocs_crc32(ocm, 88); W32(88, crc);
            for (o = 0x100; o < 1000u; o++) { sumP[0] += ocm[base + o]; sumP[1] += ocm[base + 1000u + o]; }
            assert(f_mkdir("/MODP") == FR_OK);
            assert(f_open(&pf, "/MODP/PROPHET.OCM", FA_WRITE | FA_CREATE_ALWAYS) == FR_OK && f_write(&pf, ocm, base + 2000u, &bw) == FR_OK && bw == base + 2000u);
            f_close(&pf);
            t_md_begun = t_md_ended = 0; t_plan = 2;
            fb_open(0);
            for (b = 0; b < 200u && fb.st != FB_READY; b++) fz_block();
            strcpy(fb.path, "/MODP"); fb.st = FB_LIST; fz_block();
            fb.sel = 1;
            assert(fb.st == FB_READY && fb.n == 2u && fb_ok() && fb_pack_done() == 2);
            assert(t_md_begun == 1u && t_md_engine == 19u && t_md_start == 2u && t_md_sum == sumP[1] && !memcmp(t_md_page, ocm + base + 1000u, 256));   /* start 2 is sector 4: the second variant */
            t_md_begun = t_md_ended = 0; t_plan = 0;
            fb_open(0);
            for (b = 0; b < 200u && fb.st != FB_READY; b++) fz_block();
            strcpy(fb.path, "/MODP"); fb.st = FB_LIST; fz_block();
            fb.sel = 1;
            assert(fb_ok() && fb_pack_done() == 2 && t_md_start == 0u && t_md_sum == sumP[0] && !memcmp(t_md_page, ocm + base, 256));
            t_plan = -1; t_blocked = 0; oc_mod.engine[0] = 14u; t_md_begun = 0;
            fb_open(0);
            for (b = 0; b < 200u && fb.st != FB_READY; b++) fz_block();
            strcpy(fb.path, "/MODP"); fb.st = FB_LIST; fz_block();
            fb.sel = 1;
            assert(!fb_ok() && fb.st == FB_FAIL && !strcmp(fb.msg, "SHAPES at 0: remove it") && !t_md_begun);      /* the Prophet's package, SHAPES' module in the way */
            assert(!fb_ok()); fz_block();
            t_blocked = -1; t_plan = 0;
            fb_open(0);
            assert(!fb_pack_done());
            fb_close();
        }
        (void)sum0;
        printf("  browser: .OCM listed; a damaged table, a stranger, another firmware's package, no place (and a module of another engine in the way, named) and no image refused; the code written in order, the header page last; the Prophet package picks its variant by sector\n");
    }
    dump_card("build/host/rec_card.img");               /* (tests/check_rec_card.py reads it: the takes in their folders) */
    frz_drop_all("");
    memset(frzp, 0, sizeof frzp);
    memset(bank_pid, 0, sizeof bank_pid);
    fz_blocks(2);
    usbh_want(0);
    run_ms(2);
    fflush(stdout);
    return 1;
}
int main(void)
{
    card_load();
    int ok = t_drive(64) && t_drive(8) && t_hub() && t_hub_no_drive() && t_reader_stick() && t_reader_deaf() && t_hub_empty() && t_fs() && t_stream() && t_keys() && t_not_ready() && t_no_answer() && t_silent_device() && t_freeze() && t_folders();
    printf("usb_host: %s\n", ok ? "all passed" : "FAILED");
    return !ok;
}
