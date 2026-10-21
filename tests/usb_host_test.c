/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat 0.4.7: the USB drive test (firmware/src/usb_host.c) against a model of the controller in host mode
 * and of the devices on the other end: a mass storage flash drive (EP0 packets of 64 or 8 bytes), a hub with the
 * drive on port 3, a keyboard (not a drive), a drive that is not ready at first, and a controller that does not
 * answer. It tests the probe's logic (the requests, their order, the addresses, the bulk-only transport and what
 * the page says), not the real controller: the bits it writes are the ones JieLi documents for MUSB host mode. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
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

/* ------------------------------------------------------------------ the devices --- */
enum { D_DRIVE, D_HUB, D_KEYS };
typedef struct {
    int kind, addr, mps0, configured, reset_ok;
    uint8_t in[512 + 64];                               /* the control IN data queued */
    uint32_t in_n, in_at;
    int stall, status_stall;
} dev_t;
static dev_t devs[2];
static int ndev, hub_port_reset, hub_drive_on;          /* the hub: port 3 reset, the drive behind it addressable */
static int drive_not_ready, scsi_tur_count;
static uint32_t setups, faddr_bad;
static dev_t *cur;

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
static dev_t *route(void)
{
    int i;
    for (i = 0; i < ndev; i++)
        if (devs[i].addr == common[S_FADDR] && (i == 0 || hub_drive_on)) {
            return &devs[i];                            /* (behind the hub: once its port is reset) */
        }
    faddr_bad++;
    return 0;
}
static int dev_setup(const uint8_t *s)
{
    uint8_t rt = s[0], rq = s[1];
    uint16_t val = (uint16_t)(s[2] | s[3] << 8), idx = (uint16_t)(s[4] | s[5] << 8), len = (uint16_t)(s[6] | s[7] << 8);
    setups++;
    cur = route();
    if (!cur) return 0;
    cur->in_n = cur->in_at = 0;
    cur->stall = cur->status_stall = 0;
    if (rt == 0x80 && rq == 6) {                        /* GET_DESCRIPTOR */
        uint8_t t = (uint8_t)(val >> 8), i = (uint8_t)val;
        const uint8_t *dd = cur->kind == D_DRIVE ? DRIVE_DEV : cur->kind == D_HUB ? HUB_DEV : KEYS_DEV;
        if (t == 1) { uint8_t d[18]; memcpy(d, dd, 18); d[7] = (uint8_t)cur->mps0; q(d, 18, len); }
        else if (t == 2) {
            if (cur->kind == D_DRIVE) q(DRIVE_CFG, sizeof DRIVE_CFG, len);
            else if (cur->kind == D_HUB) q(HUB_CFG, sizeof HUB_CFG, len);
            else q(KEYS_CFG, sizeof KEYS_CFG, len);
        } else if (t == 3) {
            static const uint8_t LANG[4] = {4, 3, 0x09, 0x04};
            if (i == 0) q(LANG, 4, len);
            else if (cur->kind == D_DRIVE) q_string(i == 1 ? " SanDisk" : i == 2 ? "Cruzer  Blade " : "4C530001", len);
            else if (cur->kind == D_KEYS) q_string(i == 1 ? "Logitech" : "USB Keyboard", len);
            else q_string("Generic Hub", len);
        } else cur->stall = 1;
    } else if (rt == 0x00 && rq == 5) {                 /* SET_ADDRESS (takes effect after the status stage) */
        cur->configured = -(int)val - 1;
    } else if (rt == 0x00 && rq == 9) {
        cur->configured = 1;
    } else if (rt == 0xA0 && rq == 6 && cur->kind == D_HUB) {    /* the hub descriptor: 4 ports, 100 ms power good */
        static const uint8_t HD[9] = {9, 0x29, 4, 0, 0, 50, 100, 0, 0xFF};
        q(HD, 9, len);
    } else if (rt == 0x23 && rq == 3 && cur->kind == D_HUB) {    /* SET_FEATURE */
        if (val == 4 && idx == 3) { hub_port_reset = 1; hub_drive_on = 1; devs[1].addr = 0; }
    } else if (rt == 0x23 && rq == 1 && cur->kind == D_HUB) {    /* CLEAR_FEATURE */
        if (val == 20) hub_port_reset = 2;
    } else if (rt == 0xA3 && rq == 0 && cur->kind == D_HUB) {    /* GET_STATUS(port) */
        uint8_t st[4] = {0, 0, 0, 0};
        if (idx == 3) { st[0] = (uint8_t)(1 | (hub_port_reset ? 2 : 0)); st[1] = 1; st[2] = hub_port_reset == 1 ? 0x10 : 0; }
        q(st, 4, len);
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
    dev_t *d = route();
    uint32_t len;
    assert(d && d->kind == D_DRIVE && d->configured == 1);
    assert(n == 31 && !memcmp(p, "USBC", 4));          /* every CBW in one packet */
    memcpy(&last_tag, p + 4, 4);
    memcpy(&len, p + 8, 4);
    bin_n = bin_at = 0;
    switch (p[15]) {
    case 0x12: {
        uint8_t r[36] = {0, 0x80, 4, 2, 31};
        memcpy(r + 8, "SanDisk Cruzer Blade    1.00", 28);
        memcpy(bin, r, 36); bin_n = 36; csw(0, 0);
        break;
    }
    case 0x00:
        scsi_tur_count++;
        if (drive_not_ready) { drive_not_ready--; sense_key = 2; csw(0, 1); }
        else csw(0, 0);
        break;
    case 0x03: {
        uint8_t r[18] = {0x70, 0, sense_key};
        memcpy(bin, r, 18); bin_n = 18; csw(0, 0);
        break;
    }
    case 0x25: {
        uint8_t r[8] = {0x01, 0xCA, 0x3F, 0xFF, 0, 0, 2, 0};     /* 30031872 blocks of 512 */
        memcpy(bin, r, 8); bin_n = 8; csw(0, 0);
        break;
    }
    case 0x28: {
        uint32_t lba = (uint32_t)p[17] << 24 | (uint32_t)p[18] << 16 | (uint32_t)p[19] << 8 | p[20];
        uint32_t cnt = (uint32_t)p[22] << 8 | p[23];
        assert(len == cnt * 512u && cnt <= 64u);
        memset(bin, 0, cnt * 512u);
        if (lba == 0) { bin[0x1C2] = 0x0C; bin[510] = 0x55; bin[511] = 0xAA; }
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
    ndev = hub_port_reset = hub_drive_on = drive_not_ready = scsi_tur_count = 0;
    setups = faddr_bad = 0;
    lines = 0;
    sie_alive = 1;
    attached_host = attached_device = 0;
    usb.up = 1;
    usb.detached = 0;
}
static void add(int kind, int mps0)
{
    devs[ndev].kind = kind;
    devs[ndev].mps0 = mps0;
    devs[ndev].addr = 0;
    ndev++;
}
static void run_ms(uint32_t ms)
{
    uint32_t end = fm1_ms + ms;
    while ((int32_t)(fm1_ms - end) < 0) {
        usbh_task();
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
    add(D_DRIVE, mps0);
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
static int t_hub(void)
{
    reset_world();
    add(D_HUB, 64);
    add(D_DRIVE, 64);
    devs[1].addr = 99;                                  /* not on the bus until its port is reset */
    usbh_want(1);
    run_ms(5);
    lines = 2;
    run_ms(2000);
    show("hub, drive on port 3");
    assert(usbh.st == UH_DONE && devs[0].addr == 1 && devs[0].configured == 1);
    assert(hub_port_reset == 2 && devs[1].addr == 2 && devs[1].configured == 1);
    assert(line_is(1, "A hub, port 3") && line_is(2, "ID 0781:5567") && line_is(6, "Disk: FAT32"));
    usbh_want(0);
    run_ms(2);
    return 1;
}
static int t_keys(void)
{
    reset_world();
    add(D_KEYS, 8);
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
    add(D_DRIVE, 64);
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
    add(D_DRIVE, 64);
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
    int ok = t_drive(64) && t_drive(8) && t_hub() && t_keys() && t_not_ready() && t_no_answer() && t_silent_device();
    printf("usb_host: %s\n", ok ? "all passed" : "FAILED");
    return !ok;
}
