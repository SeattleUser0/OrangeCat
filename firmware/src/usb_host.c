/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat: the USB flash drive test (build doc phase 2). Menu > USB DRIVE TEST turns the USB port into a host,
 * waits for something to be plugged in, and shows what it found: the speed, the maker and model (the device
 * descriptor and its strings), the class. A mass storage drive is then read: its own name (SCSI INQUIRY), its size
 * (READ CAPACITY), sector 0 (the partition table or the boot sector: FAT32, exFAT ..) and a read speed (256 KB).
 * Hubs are walked, up to three deep, every port with something on it, until a drive reads (a powered hub is one
 * way to power a drive: the FM-1 gives its port no 5 V). If none reads, the page lists what was found. Leaving the page (OCT-) turns the port back into the normal device: USB MIDI,
 * USB audio and the web editor come back.
 *
 * Nothing here was known to work on a unit when it was written. The controller is a MUSB-style core (src/usb.c);
 * the host bits are in hal/fm1_usb.h (fm1_usb_host_attach). Every step records what it saw (usbh.step, the last
 * CSR, DEVCTL, the line levels), and the page shows that diagnostic line, so a photo of the screen says where it
 * stopped. All of it runs in the main loop: usb.up = 0 and usb.detached = 1 keep usb_poll, ua_service and usb_retry
 * (TIMER5, main loop) off the SIE while the host owns it. A probe blocks the main loop for a few seconds at most
 * (the audio runs on; the watchdog is fed).
 *
 * The SIE registers (MUSB byte map, JieLi's usb_phy.h): DEVCTL 0x0F (bit 0 Session, bit 2 HostMode, bit 5 LSDev,
 * bit 6 FSDev), TXTYPE 0x18, TXINTERVAL 0x19, RXTYPE 0x1A, RXINTERVAL 0x1B; host CSR0: RxPktRdy 0x01, TxPktRdy
 * 0x02, RxStall 0x04, SetupPkt 0x08, Error 0x10, ReqPkt 0x20, StatusPkt 0x40, NAK timeout 0x80; host TXCSR1:
 * TxPktRdy 0x01, Error 0x04, RxStall 0x20, ClrDataTog 0x40, NAK timeout 0x80; host RXCSR1: RxPktRdy 0x01, Error
 * 0x04, DataError / NAK timeout 0x08, FlushFIFO 0x10, ReqPkt 0x20, RxStall 0x40, ClrDataTog 0x80.
 * The buffers are usb.c's (ep0buf, ep1tx, ep1rx): device mode is off while they are used here.
 *
 * Revisions:
 *   0.4.7: first version.
 *   0.4.8: the first test on a unit (RCA C109 hub on PD power) got through the hub to a second hub inside it
 *          (1A40:0801) and stopped: one hub was all 0.4.7 followed. Now the whole tree is walked (uh_visit), a
 *          storage device that does not read (a card reader with no card) is passed over for the next, and with no
 *          drive read the page lists every device found with its place ("1.3": port 3 of the hub on port 1).
 *          Waits feed the watchdog (uh_wait): a walk can take several seconds.
 *   0.4.9 (after the second unit test): the SD card in the hub's slot read (29.7 GB, FAT32, 914 KB/s: about what full speed
 *          USB carries). A card reader stick in the hub's port 3 failed its first command (INQUIRY) and the walk
 *          went on, so the error code was lost. Now: GET_MAX_LUN first (a reader may have a slot for each card
 *          type; each is tried, "slot 2" on the page when it is not the first), 300 ms before the first command (Linux
 *          waits a second), one retry after a Bulk-Only reset and 1 s, the error kept in the reason ("3: no answer,
 *          stall"), and the list keeps hubs and disks only (a USB-C billboard device took a line).
 *   0.4.10: the drive's files (usb_files.c). usbh.drive says a drive was read and stays until it is unplugged; the
 *          page's file list lives here (view, cmd, path, ent, info) so the UI and its tests need no file system.
 *          OCT+ on a test that found no drive looks again (usbh_reprobe).
 *   0.4.14: the foot's lines hold 48 characters (UH_FOOT) and there are three (info3: the players' gaps and speed);
 *          the list shows five rows to make room. (0.4.13's foot ran past its 30 and wrote over the first row's
 *          name: the "8" on the device was the end of the gap count.)
 *   0.4.16: WRITE(10): uh_scsi_w (the recorder's, through FatFs: usb_files.c disk_write).
 */
#ifndef UH_LINES_READ
#define UH_LINES_READ() fm1_usb_lines()                 /* (host tests replace the hardware read) */
#endif
enum { S_DEVCTL = 15, S_TXTYPE = 24, S_TXINTERVAL = 25, S_RXTYPE = 26, S_RXINTERVAL = 27 };
enum { UH_OFF, UH_WAIT, UH_SEEN, UH_DONE };
/* (0.4.10) what the page asks the main loop for (usbh.cmd; usb_files.c usbf_task does it) */
enum { UC_NONE, UC_LIST, UC_WINDOW, UC_ENTER, UC_UP, UC_PROBE, UC_REC };   /* (0.4.16: UC_REC, REC on the files) */
#define UH_ROWS 5u                                      /* the file list's rows on the page (0.4.14: was 6) */
#define UH_NAME 32u
/* errors (negative): the page names them */
enum { UE_TIME = 1, UE_STALL, UE_ERR, UE_NAK, UE_SHORT, UE_CSW, UE_CHECK };
#define UH_LINES 8u
#define UH_LINE 30u
#if MELODEE_USB_AUDIO
#define UH_SRC_MAX 4u                                   /* (0.4.17) REC: the mix or tracks 1..4 (mix.c's USB stems) */
#else
#define UH_SRC_MAX 0u                                   /* (no stems: the mix only) */
#endif
#define UH_FOOT 48u                                     /* (0.4.14) the foot's lines: longer than the page shows */
static struct {
    uint8_t st;                 /* UH_* */
    uint8_t step;               /* where a probe got to (the diagnostic line) */
    uint8_t addr, mps0;         /* the device being talked to, its EP0 packet size */
    uint8_t ep_in, ep_out;      /* bulk endpoints (mass storage) */
    uint8_t csr, devctl;        /* the last CSR seen, DEVCTL after the session started */
    uint8_t sie;                /* 1: the controller answered when attached as a host */
    uint8_t want;               /* the page asks (ui_menu.c); the main loop opens and closes (usbh_task) */
    uint8_t lun, ifc;           /* (0.4.9) the drive's slot (SCSI LUN) commands go to; its interface number */
    int8_t err;                 /* the last error (UE_*), 0 none */
    uint32_t t_ms;              /* when the lines changed */
    uint32_t gen;               /* the page redraws when this changes */
    uint32_t tag;               /* mass storage command tag */
    char line[UH_LINES][UH_LINE];
    /* (0.4.10) the files: drive 1 = a drive read (its address, slot and endpoints set), until it is unplugged; view
     * 0 the probe's lines, 1 the file list (the page sets it); the folder (path, "" the top), how many in it (count),
     * the row picked (sel) and the first shown (top), the rows as read (ent, nent, for ent_top), and the foot */
    uint8_t drive, view, cmd, nent;
    uint16_t count, sel, top, ent_top;
    char path[96];
    char info[UH_FOOT], info2[UH_FOOT], info3[UH_FOOT];   /* (0.4.14: info3, and 48 each) */
    uint8_t rec;                                        /* (0.4.16) the recorder runs (usb_files.c; the REC light) */
    uint8_t src;                                        /* (0.4.17) what REC records: 0 the mix, 1..4 that track (KNOB 3) */
    struct { char name[UH_NAME]; uint32_t size; uint8_t dir, play; } ent[UH_ROWS];   /* (0.4.13: play, a WAV playing) */
} usbh;
static uint8_t uh_sec[512];     /* sector 0 */
/* (0.4.8) the walk through hubs: the ports from the first device down (path), what was found (log), why the last
 * storage device did not read (why) */
#define UH_DEPTH 3u
#define UH_LOG 5u
static struct {
    uint8_t next;               /* the address the next device gets */
    uint8_t depth, path[UH_DEPTH];
    uint8_t n;                  /* log entries */
    char log[UH_LOG][UH_LINE];
    char why[UH_LINE];
} uh_walk;

static void uh_feed(void) { fm1_wdt_feed(); }
static void uh_wait(uint32_t ms)                        /* (0.4.8) a wait that feeds the watchdog */
{
    while (ms) {
        uint32_t k = ms > 20u ? 20u : ms;
        fm1_delay_ms(k);
        uh_feed();
        ms -= k;
    }
}

/* ---- the page's lines ---- */
static void uh_set(uint32_t i, const char *s)
{
    if (i < UH_LINES) {
        str_cpy(usbh.line[i], s, UH_LINE);
        usbh.gen++;
    }
}
static void uh_clear(void)
{
    uint32_t i;
    for (i = 0; i < UH_LINES; i++) usbh.line[i][0] = 0;
    usbh.gen++;
}
static char *uh_hex(char *b, uint32_t v, uint32_t digits)   /* writes, returns the end */
{
    while (digits--) {
        uint32_t d = (v >> (4u * digits)) & 15u;
        *b++ = (char)(d < 10u ? '0' + d : 'A' + d - 10u);
    }
    *b = 0;
    return b;
}
static char *uh_dec(char *b, uint32_t v)
{
    char t[11];
    uint32_t n = 0;
    do { t[n++] = (char)('0' + v % 10u); v /= 10u; } while (v);
    while (n) *b++ = t[--n];
    *b = 0;
    return b;
}
static char *uh_cat(char *b, const char *s) { while (*s) *b++ = *s++; *b = 0; return b; }
/* (0.4.10) "100 B", "64 KB", "1.2 MB", "3.4 GB" */
static char *uh_bytes(char *b, uint32_t n)
{
    static const char *const U[] = {" B", " KB", " MB", " GB"};
    uint32_t u = 0, t = 0;
    while (n >= 1024u && u < 3u) { t = n % 1024u * 10u / 1024u; n /= 1024u; u++; }
    b = uh_dec(b, n);
    if (u && n < 10u) { *b++ = '.'; b = uh_dec(b, t); }
    return uh_cat(b, U[u]);
}

/* ---- the SIE ---- */
static int uh_fail(int e) { usbh.err = (int8_t)e; return -e; }
static uint32_t uh_late(uint32_t t0, uint32_t ms, uint32_t *spins)
{
    if (!(++*spins & 1023u)) uh_feed();
    return fm1_ms - t0 > ms || *spins > 4000000u;       /* (spins: a clock that stopped cannot hang it) */
}
/* CSR0 = start, then wait: want != 0 for one of those bits, else for the busy bits to clear */
static int uh_csr0(uint32_t start, uint32_t busy, uint32_t want, uint32_t ms)
{
    uint32_t t0 = fm1_ms, spins = 0, c;
    sie_wr(S_INDEX, 0);
    sie_wr(S_CSR0, start);
    for (;;) {
        c = sie_rd(S_CSR0);
        usbh.csr = (uint8_t)c;
        if (c & 0x04u) { sie_wr(S_CSR0, 0); return uh_fail(UE_STALL); }
        if (c & 0x10u) { sie_wr(S_CSR0, 0); return uh_fail(UE_ERR); }
        if (c & 0x80u) { sie_wr(S_CSR0, 0); return uh_fail(UE_NAK); }
        if (want ? (c & want) != 0 : !(c & busy)) return (int)c;
        if (uh_late(t0, ms, &spins)) { sie_wr(S_CSR0, 0); return uh_fail(UE_TIME); }
    }
}
/* one control transfer: SETUP, the IN data (into d, at most len; *got the count), the status stage.
 * No request here sends OUT data. 0 done, < 0 an error. */
static int uh_ctrl(uint8_t rt, uint8_t rq, uint16_t val, uint16_t idx, uint16_t len, uint8_t *d, uint32_t *got)
{
    uint32_t n = 0, k, i;
    int r;
    sie_wr(S_FADDR, usbh.addr);
    ep0buf[0] = rt; ep0buf[1] = rq;
    ep0buf[2] = (uint8_t)val; ep0buf[3] = (uint8_t)(val >> 8);
    ep0buf[4] = (uint8_t)idx; ep0buf[5] = (uint8_t)(idx >> 8);
    ep0buf[6] = (uint8_t)len; ep0buf[7] = (uint8_t)(len >> 8);
    fm1_usb_ep0_send(ep0buf, 8);
    if ((r = uh_csr0(0x0Au, 0x02u, 0, 500)) < 0)        /* SetupPkt | TxPktRdy */
        return r;
    if ((rt & 0x80u) && len) {
        for (;;) {
            if ((r = uh_csr0(0x20u, 0, 0x01u, 500)) < 0)  /* ReqPkt, then RxPktRdy */
                return r;
            k = sie_rd(S_COUNT0);
            if (k > 64u) k = 64u;
            fm1_usb_rx_sync();
            for (i = 0; i < k && n < len; i++)
                d[n++] = ep0buf[i];
            sie_wr(S_CSR0, 0);                          /* RxPktRdy cleared */
            if (n >= len || k < usbh.mps0)
                break;
        }
        fm1_usb_ep0_send(ep0buf, 0);
        r = uh_csr0(0x42u, 0x02u, 0, 500);              /* StatusPkt | TxPktRdy: the OUT status */
    } else {
        r = uh_csr0(0x60u, 0, 0x01u, 500);              /* StatusPkt | ReqPkt: the IN status */
        if (r >= 0) sie_wr(S_CSR0, 0);
    }
    if (got) *got = n;
    return r < 0 ? r : 0;
}
static int uh_get_desc(uint8_t type, uint8_t i, uint16_t lang, uint16_t len, uint8_t *d, uint32_t *got)
{
    return uh_ctrl(0x80u, 6u, (uint16_t)(type << 8 | i), lang, len, d, got);
}

/* ---- bulk transfers on host EP1 (TX: OUT to the drive, RX: IN from it) ---- */
static void uh_bulk_setup(void)
{
    fm1_usb_ep_txbuf(1, ep1tx);
    fm1_usb_ep_rxbuf(1, ep1rx);
    sie_wr(S_INDEX, 1);
    sie_wr(S_TXTYPE, 0x20u | (usbh.ep_out & 15u));      /* bulk, the drive's endpoint number */
    sie_wr(S_TXINTERVAL, 0);                            /* no NAK limit: the timeouts here end a wait */
    sie_wr(S_TXMAXP, 0xFF);                             /* (as usb.c's device endpoints) */
    sie_wr(S_TXCSR1, 0x48);                             /* ClrDataTog | FlushFIFO */
    sie_wr(S_TXCSR2, 0);
    sie_wr(S_RXTYPE, 0x20u | (usbh.ep_in & 15u));
    sie_wr(S_RXINTERVAL, 0);
    sie_wr(S_RXMAXP, 0xFF);
    sie_wr(S_RXCSR1, 0x90);                             /* ClrDataTog | FlushFIFO */
    sie_wr(S_RXCSR2, 0);
    fm1_usb_ep_enable(1u << 1);
}
static int uh_out(const uint8_t *p, uint32_t n)
{
    while (n) {
        uint32_t k = n > 64u ? 64u : n, i, c, t0 = fm1_ms, spins = 0;
        for (i = 0; i < k; i++) ep1tx[i] = p[i];
        fm1_usb_ep_send(1, ep1tx, k);
        sie_wr(S_INDEX, 1);
        sie_wr(S_TXCSR1, 0x01u);                        /* TxPktRdy */
        for (;;) {
            c = sie_rd(S_TXCSR1);
            usbh.csr = (uint8_t)c;
            if (c & 0x20u) { sie_wr(S_TXCSR1, c & ~0x21u); return uh_fail(UE_STALL); }
            if (c & 0x04u) { sie_wr(S_TXCSR1, c & ~0x05u); return uh_fail(UE_ERR); }
            if (c & 0x80u) { sie_wr(S_TXCSR1, c & ~0x81u); return uh_fail(UE_NAK); }
            if (!(c & 0x01u)) break;
            if (uh_late(t0, 1000, &spins)) { sie_wr(S_TXCSR1, 0x08u); return uh_fail(UE_TIME); }
        }
        p += k;
        n -= k;
    }
    return 0;
}
/* up to n bytes into d (0: dropped); a short packet ends it. The count read, or < 0. */
static int uh_in(uint8_t *d, uint32_t n)
{
    uint32_t got = 0;
    while (got < n) {
        uint32_t c, k, i, t0 = fm1_ms, spins = 0;
        sie_wr(S_INDEX, 1);
        sie_wr(S_RXCSR1, 0x20u);                        /* ReqPkt */
        for (;;) {
            c = sie_rd(S_RXCSR1) | sie_rd(S_RXCSR2) << 8;
            usbh.csr = (uint8_t)c;
            if (c & 0x40u) { sie_wr(S_RXCSR1, c & ~0x60u & 0xFFu); return uh_fail(UE_STALL); }
            if (c & 0x04u) { sie_wr(S_RXCSR1, c & ~0x24u & 0xFFu); return uh_fail(UE_ERR); }
            if (c & 0x08u) { sie_wr(S_RXCSR1, c & ~0x28u & 0xFFu); return uh_fail(UE_NAK); }
            if (c & 0x01u) break;
            if (uh_late(t0, 1000, &spins)) { sie_wr(S_RXCSR1, 0); return uh_fail(UE_TIME); }
        }
        k = sie_rd(S_RXCOUNT1) | sie_rd(S_RXCOUNT2) << 8;
        if (k > 64u) k = 64u;
        fm1_usb_rx_sync();
        for (i = 0; i < k && got < n; i++, got++)
            if (d) d[got] = ep1rx[i];
        c = (c & ~0x164u) | 0x10u;                      /* the packet taken (as usb.c ep1_rx) */
        sie_wr(S_RXCSR1, c & 0xFFu);
        sie_wr(S_RXCSR2, c >> 8);
        if (k < 64u) break;
    }
    return (int)got;
}
/* CLEAR_FEATURE(ENDPOINT_HALT) and our side's data toggle back to DATA0 */
static void uh_unstall(uint8_t ep)
{
    usbh.err = 0;
    uh_ctrl(0x02u, 1u, 0, ep, 0, 0, 0);
    sie_wr(S_INDEX, 1);
    if (ep & 0x80u) sie_wr(S_RXCSR1, 0x80u);
    else sie_wr(S_TXCSR1, 0x40u);
}

/* ---- mass storage, bulk-only transport ---- */
/* one SCSI command: cb (len cl), n bytes IN into d (n 0: no data). 0 done and passed, < 0 an error */
static int uh_scsi(const uint8_t *cb, uint32_t cl, uint8_t *d, uint32_t n)
{
    uint8_t w[31], s[13];
    uint32_t i;
    int r, got = 0;
    for (i = 0; i < 31u; i++) w[i] = 0;
    w[0] = 'U'; w[1] = 'S'; w[2] = 'B'; w[3] = 'C';
    usbh.tag++;
    for (i = 0; i < 4u; i++) {
        w[4 + i] = (uint8_t)(usbh.tag >> (8u * i));
        w[8 + i] = (uint8_t)(n >> (8u * i));
    }
    w[12] = n ? 0x80u : 0;                              /* data IN */
    w[13] = usbh.lun;                                   /* (0.4.9) the slot */
    w[14] = (uint8_t)cl;
    for (i = 0; i < cl && i < 16u; i++) w[15 + i] = cb[i];
    if ((r = uh_out(w, 31)) < 0) {
        if (r == -UE_STALL) uh_unstall(usbh.ep_out);
        return r;
    }
    if (n) {
        got = uh_in(d, n);
        if (got == -UE_STALL) uh_unstall((uint8_t)(usbh.ep_in | 0x80u));
        else if (got < 0) return got;
    }
    r = uh_in(s, 13);
    if (r == -UE_STALL) {                               /* the status phase stalled once: clear, then again */
        uh_unstall((uint8_t)(usbh.ep_in | 0x80u));
        r = uh_in(s, 13);
    }
    if (r < 0) return r;
    if (r != 13 || s[0] != 'U' || s[1] != 'S' || s[2] != 'B' || s[3] != 'S')
        return uh_fail(UE_CSW);
    if (s[12]) return uh_fail(UE_CHECK);                /* command failed (the drive has sense data) */
    return got < 0 ? got : 0;
}
/* (0.4.16) one SCSI command with n bytes OUT from d (WRITE(10)): the CBW, the data in 64-byte packets, the status.
 * The drive may hold off the data while the card is busy writing (it NAKs; uh_out waits up to 1 s a packet) and
 * the status until the card is done (uh_in waits up to 1 s). 0 done and passed, < 0 an error */
static int uh_scsi_w(const uint8_t *cb, uint32_t cl, const uint8_t *d, uint32_t n)
{
    uint8_t w[31], s[13];
    uint32_t i;
    int r;
    for (i = 0; i < 31u; i++) w[i] = 0;
    w[0] = 'U'; w[1] = 'S'; w[2] = 'B'; w[3] = 'C';
    usbh.tag++;
    for (i = 0; i < 4u; i++) {
        w[4 + i] = (uint8_t)(usbh.tag >> (8u * i));
        w[8 + i] = (uint8_t)(n >> (8u * i));
    }
    w[12] = 0;                                          /* data OUT */
    w[13] = usbh.lun;
    w[14] = (uint8_t)cl;
    for (i = 0; i < cl && i < 16u; i++) w[15 + i] = cb[i];
    if ((r = uh_out(w, 31)) < 0) {
        if (r == -UE_STALL) uh_unstall(usbh.ep_out);
        return r;
    }
    if (n && (r = uh_out(d, n)) < 0) {
        if (r != -UE_STALL) return r;
        uh_unstall(usbh.ep_out);                        /* (the drive refused the data: its status says why) */
    }
    r = uh_in(s, 13);
    if (r == -UE_STALL) {
        uh_unstall((uint8_t)(usbh.ep_in | 0x80u));
        r = uh_in(s, 13);
    }
    if (r < 0) return r;
    if (r != 13 || s[0] != 'U' || s[1] != 'S' || s[2] != 'B' || s[3] != 'S')
        return uh_fail(UE_CSW);
    if (s[12]) return uh_fail(UE_CHECK);                /* (a locked card, or past its end) */
    return 0;
}
static uint32_t uh_be32(const uint8_t *p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }

/* "0.1 GB", "14.3 GB", "931 GB": blocks x size (GB of 1024 MB, as a computer shows it) */
static char *uh_size(char *b, uint32_t blocks, uint32_t bsize)
{
    uint32_t mb = (uint32_t)(((uint64_t)blocks * bsize) >> 20), t;
    if (mb >= 102400u) return uh_cat(uh_dec(b, mb >> 10), " GB");
    t = mb * 10u / 1024u;                                /* tenths of a GB */
    b = uh_dec(b, t / 10u);
    *b++ = '.';
    return uh_cat(uh_dec(b, t % 10u), " GB");
}

/* what sector 0 is: a partition table (the first entry's type) or a boot sector with no table */
static const char *uh_sector0_kind(const uint8_t *s)
{
    uint8_t t = s[0x1C2];
    if (s[510] != 0x55u || s[511] != 0xAAu) return "no signature";
    if (s[3] == 'E' && s[4] == 'X' && s[5] == 'F' && s[6] == 'A' && s[7] == 'T') return "exFAT, no table";
    if (s[0x52] == 'F' && s[0x53] == 'A' && s[0x54] == 'T' && s[0x55] == '3' && s[0x56] == '2') return "FAT32, no table";
    if (s[0x36] == 'F' && s[0x37] == 'A' && s[0x38] == 'T') return "FAT, no table";
    if (t == 0x0Bu || t == 0x0Cu) return "FAT32";
    if (t == 0x07u) return "exFAT or NTFS";
    if (t == 0x01u || t == 0x04u || t == 0x06u || t == 0x0Eu) return "FAT16";
    if (t == 0xEEu) return "GPT";
    if (t == 0xAFu) return "Mac (HFS+ or APFS)";
    return "other";
}

static char *uh_path(char *b);
static const char *uh_err_name(int e);
/* a storage device that did not read: the page's line, and the reason kept for the list ("1.2: no card?", "3: no
 * answer, stall": (0.4.9) the error too, since the walk goes on and the diagnostic line moves with it) */
static int uh_msc_fail(uint32_t line, const char *s, const char *brief)
{
    char *e = uh_path(uh_walk.why), b[UH_LINE];
    uh_set(line, s);
    e = uh_cat(e, ": ");
    str_cpy(e, brief, (unsigned)(UH_LINE - (uint32_t)(e - uh_walk.why)));
    if (usbh.err && usbh.err != UE_CHECK && str_len(uh_walk.why) + 2u + str_len(uh_err_name(usbh.err)) < UH_LINE) {
        uh_cat(uh_cat(b, ", "), uh_err_name(usbh.err));
        uh_cat(uh_walk.why + str_len(uh_walk.why), b);
    }
    return 0;
}
/* (0.4.9) Bulk-Only Mass Storage Reset, then both endpoints' halts cleared */
static void uh_bot_reset(void)
{
    uh_ctrl(0x21u, 0xFFu, 0, usbh.ifc, 0, 0, 0);
    uh_unstall((uint8_t)(usbh.ep_in | 0x80u));
    uh_unstall(usbh.ep_out);
}
/* INQUIRY, TEST UNIT READY (the drive may need a moment), READ CAPACITY, sector 0, 256 KB timed. 1 read */
static int uh_msc(void)
{
    static const uint8_t INQ[6] = {0x12, 0, 0, 0, 36, 0}, TUR[6] = {0}, SENSE[6] = {0x03, 0, 0, 0, 18, 0};
    static const uint8_t CAP[10] = {0x25};
    uint8_t cb[10], *d = uh_sec;
    char b[UH_LINE], *e;
    uint32_t i, tries, blocks, bsize, t0, got = 0, lun, maxlun;
    int r, ready = 0;
    uint8_t m = 0;
    usbh.step = 30;
    for (i = 4; i < UH_LINES; i++) usbh.line[i][0] = 0;   /* (0.4.8: nothing left from a device before) */
    usbh.lun = 0;
    uh_bulk_setup();
    if (uh_ctrl(0xA1u, 0xFEu, 0, usbh.ifc, 1, &m, &got) < 0 || got < 1u)   /* GET_MAX_LUN (a stall: one slot) */
        m = 0;
    maxlun = m > 3u ? 3u : m;
    uh_wait(300);                                       /* (0.4.9) a moment before the first command */
    usbh.err = 0;
    if (uh_scsi(INQ, 6, d, 36) < 0) {                   /* (0.4.9) once more, after a reset and a second */
        uh_bot_reset();
        uh_wait(1000);
        if (uh_scsi(INQ, 6, d, 36) < 0)
            return uh_msc_fail(5, "Drive did not answer", "no answer");
    }
    for (i = 8, e = b; i < 32u && e < b + UH_LINE - 1; i++)  /* vendor 8..15, product 16..31 */
        if (d[i] >= 32u && d[i] < 127u && !(d[i] == ' ' && (e == b || e[-1] == ' ')))
            *e++ = (char)d[i];
    while (e > b && e[-1] == ' ') e--;
    *e = 0;
    if (str_len(b) != str_len(usbh.line[3]) || memcmp(b, usbh.line[3], str_len(b)))
        uh_set(4, b);                                   /* (the drive's own name, when it differs from line 3) */
    for (lun = 0; lun <= maxlun; lun++) {             /* (0.4.9) each slot until one has a card that reads */
        usbh.lun = (uint8_t)lun;
        usbh.step = 31;
        ready = 0;
        for (tries = 0; tries < (maxlun ? 10u : 20u); tries++) {
            if (uh_scsi(TUR, 6, 0, 0) == 0) { ready = 1; break; }
            uh_scsi(SENSE, 6, d, 18);
            uh_wait(100);
        }
        usbh.step = 32;
        if (uh_scsi(CAP, 10, d, 8) == 0)
            break;
    }
    if (lun > maxlun) {
        usbh.lun = 0;
        return uh_msc_fail(5, ready ? "Size did not read" : "Not ready (no card?)", ready ? "no size" : "no card?");
    }
    blocks = uh_be32(d) + 1u;
    bsize = uh_be32(d + 4);
    e = uh_size(uh_cat(b, "Drive "), blocks, bsize);
    if (lun) uh_dec(uh_cat(e, ", slot "), lun + 1u);     /* (0.4.9) not the reader's first slot */
    uh_set(5, b);
    usbh.step = 33;
    for (i = 0; i < 10u; i++) cb[i] = 0;
    cb[0] = 0x28; cb[8] = 1;                            /* READ(10), LBA 0, one block */
    if (bsize != 512u || (r = uh_scsi(cb, 10, uh_sec, 512)) < 0)
        return uh_msc_fail(6, bsize != 512u ? "Blocks not 512 bytes" : "Sector 0 not read",
                           bsize != 512u ? "not 512" : "no sector 0");
    e = uh_cat(b, "Disk: ");
    uh_cat(e, uh_sector0_kind(uh_sec));
    uh_set(6, b);
    usbh.step = 34;
    t0 = fm1_ms;
    for (i = 0; i < 8u; i++) {                          /* 8 x 64 blocks from LBA 0 = 256 KB */
        uint32_t lba = i * 64u;
        cb[2] = (uint8_t)(lba >> 24); cb[3] = (uint8_t)(lba >> 16); cb[4] = (uint8_t)(lba >> 8); cb[5] = (uint8_t)lba;
        cb[7] = 0; cb[8] = 64;
        if (uh_scsi(cb, 10, 0, 64u * 512u) < 0) break;
        uh_feed();
    }
    if (i == 8u) {
        uint32_t ms = fm1_ms - t0;
        e = uh_cat(b, "Read ");
        e = uh_dec(e, 256u * 1000u / (ms ? ms : 1u));
        uh_cat(e, " KB/s");
        uh_set(7, b);
    }
    usbh.step = 35;
    return 1;
}

/* ---- enumeration ---- */
/* a string descriptor as ASCII (others '?'), at most UH_LINE - 1 */
static void uh_string(uint8_t i, uint16_t lang, char *b)
{
    uint8_t d[64];
    uint32_t got = 0, k, n = 0;
    b[0] = 0;
    if (!i || uh_get_desc(3, i, lang, sizeof d, d, &got) < 0 || got < 4u) return;
    for (k = 2; k + 1u < got && k < d[0] && n < UH_LINE - 1u; k += 2) {
        uint32_t c = d[k] | (uint32_t)d[k + 1] << 8;
        if (c == ' ' && (!n || b[n - 1] == ' ')) continue;
        b[n++] = (char)(c >= 32u && c < 127u ? c : '?');
    }
    while (n && b[n - 1] == ' ') n--;
    b[n] = 0;
}

static struct { uint8_t cls, sub, proto, cfg, nports; uint16_t vid, pid; } uh_dev;
/* the device at address 0 (just reset) to address a: its descriptors, strings, configuration */
static int uh_enumerate(uint8_t a)
{
    uint8_t d[64];
    uint32_t got = 0, total, i, k;
    uint16_t lang = 0x0409;
    char b[UH_LINE], *e;
    int r;
    usbh.addr = 0;
    usbh.mps0 = 8;
    usbh.step = 11;                                     /* (11 .. 16; which device: the diagnostic line's A) */
    if ((r = uh_get_desc(1, 0, 0, 18, d, &got)) < 0 || got < 8u) return r < 0 ? r : uh_fail(UE_SHORT);
    usbh.mps0 = d[7] >= 8u ? d[7] : 8u;
    usbh.step++;
    if ((r = uh_ctrl(0x00u, 5u, a, 0, 0, 0, 0)) < 0) return r;     /* SET_ADDRESS */
    uh_wait(10);
    usbh.addr = a;
    usbh.step++;
    if ((r = uh_get_desc(1, 0, 0, 18, d, &got)) < 0 || got < 18u) return r < 0 ? r : uh_fail(UE_SHORT);
    uh_dev.vid = (uint16_t)(d[8] | d[9] << 8);
    uh_dev.pid = (uint16_t)(d[10] | d[11] << 8);
    uh_dev.cls = d[4];
    {
        uint8_t im = d[14], ip = d[15];
        uint8_t l[4];
        uint32_t lg = 0;
        e = uh_cat(b, "ID ");
        e = uh_hex(e, uh_dev.vid, 4);
        *e++ = ':';
        uh_hex(e, uh_dev.pid, 4);
        uh_set(2, b);
        usbh.step++;
        if ((im || ip) && uh_get_desc(3, 0, 0, 4, l, &lg) == 0 && lg >= 4u)
            lang = (uint16_t)(l[2] | l[3] << 8);
        uh_string(im, lang, b);
        e = b + str_len(b);
        if (ip && e < b + UH_LINE - 2) {
            if (e > b) *e++ = ' ';
            uh_string(ip, lang, e);
        }
        if (b[0]) uh_set(3, b);
    }
    usbh.step++;
    if ((r = uh_get_desc(2, 0, 0, 9, d, &got)) < 0 || got < 9u) return r < 0 ? r : uh_fail(UE_SHORT);
    total = d[2] | (uint32_t)d[3] << 8;
    if (total > sizeof d) total = sizeof d;
    if ((r = uh_get_desc(2, 0, 0, (uint16_t)total, d, &got)) < 0) return r;
    uh_dev.cfg = d[5];
    uh_dev.sub = uh_dev.proto = 0;
    usbh.ep_in = usbh.ep_out = 0;
    for (i = 0, k = 0; i + 2u <= got && d[i] >= 2u; i += d[i]) {   /* the first interface, the bulk endpoints */
        if (d[i + 1] == 4u && i + 8u <= got && !k) {   /* its class says more than the device's (often 0) */
            k = 1;
            uh_dev.cls = d[i + 5];
            uh_dev.sub = d[i + 6];
            uh_dev.proto = d[i + 7];
            usbh.ifc = d[i + 2];
        }
        if (d[i + 1] == 5u && i + 7u <= got && (d[i + 3] & 3u) == 2u) {
            if (d[i + 2] & 0x80u) { if (!usbh.ep_in) usbh.ep_in = d[i + 2] & 15u; }
            else if (!usbh.ep_out) usbh.ep_out = d[i + 2] & 15u;
        }
    }
    usbh.step++;
    if ((r = uh_ctrl(0x00u, 9u, uh_dev.cfg, 0, 0, 0, 0)) < 0) return r;     /* SET_CONFIGURATION */
    return 0;
}

/* ---- hubs (0.4.8: the whole tree) ---- */
/* "1.3": the ports from the first device down; "top" for the first device itself */
static char *uh_path(char *b)
{
    uint32_t i;
    char *e = b;
    if (!uh_walk.depth) return uh_cat(b, "top");
    for (i = 0; i < uh_walk.depth; i++) {
        if (i) *e++ = '.';
        e = uh_dec(e, uh_walk.path[i]);
    }
    return e;
}
/* one line of the list: "1.3 05E3:0749 disk", "top 1A40:0801 hub", "1.2 no answer" ((0.4.9) other devices left out:
 * a keyboard, a USB-C billboard (c11) took lines a drive's place needed) */
static void uh_log(int answered)
{
    char *e;
    if (uh_walk.n >= UH_LOG) return;
    if (answered && uh_dev.cls != 8u && uh_dev.cls != 9u) return;   /* (0.4.9) hubs and disks only */
    e = uh_path(uh_walk.log[uh_walk.n]);
    *e++ = ' ';
    if (!answered) {
        uh_cat(e, "no answer");
    } else {
        e = uh_hex(e, uh_dev.vid, 4);
        *e++ = ':';
        e = uh_hex(e, uh_dev.pid, 4);
        *e++ = ' ';
        if (uh_dev.cls == 9u) uh_cat(e, "hub");
        else if (uh_dev.cls == 8u) uh_cat(e, "disk");
        else uh_hex(uh_cat(e, "c"), uh_dev.cls, 2);   /* the USB class: c03 a keyboard or mouse */
    }
    uh_walk.n++;
}
/* the hub at usbh.addr: its ports powered, then which have something on them, a bit per port (1..7). Devices take
 * a moment after power: watch up to 2 s, and stop once the answer has held for 300 ms */
static uint32_t uh_hub_ports(void)
{
    uint8_t d[16];
    uint32_t got = 0, p, n, mask = 0, last = 0, same = 0, t0;
    usbh.step = 40;
    if (uh_ctrl(0xA0u, 6u, 0x2900u, 0, 9, d, &got) < 0 || got < 3u) return 0;   /* the hub descriptor */
    n = d[2] > 7u ? 7u : d[2];
    for (p = 1; p <= n; p++)
        uh_ctrl(0x23u, 3u, 8u, (uint16_t)p, 0, 0, 0);   /* SET_FEATURE(PORT_POWER) */
    uh_wait(2u * d[5] + 100u);                          /* bPwrOn2PwrGood (2 ms units) and a margin */
    usbh.step = 41;
    for (t0 = fm1_ms; fm1_ms - t0 < 2000u;) {
        mask = 0;
        for (p = 1; p <= n; p++)
            if (uh_ctrl(0xA3u, 0u, 0, (uint16_t)p, 4, d, &got) == 0 && got >= 4u && (d[0] & 1u))
                mask |= 1u << p;
        same = mask == last ? same + 1u : 0u;
        last = mask;
        if (mask && same >= 3u) break;
        uh_wait(100);
    }
    return mask;
}
/* reset port p of the hub at usbh.addr: 0 a full speed device now at address 0; < 0 not (low speed, or the reset
 * did not end) */
static int uh_port_reset(uint32_t p)
{
    uint8_t d[4];
    uint32_t got = 0, i, done = 0;
    usbh.step = 42;
    uh_ctrl(0x23u, 3u, 4u, (uint16_t)p, 0, 0, 0);       /* SET_FEATURE(PORT_RESET) */
    for (i = 0; i < 25u && !done; i++) {
        uh_wait(20);
        done = uh_ctrl(0xA3u, 0u, 0, (uint16_t)p, 4, d, &got) == 0 && got >= 4u && (d[2] & 0x10u);
    }
    uh_ctrl(0x23u, 1u, 20u, (uint16_t)p, 0, 0, 0);    /* CLEAR_FEATURE(C_PORT_RESET) */
    uh_ctrl(0x23u, 1u, 16u, (uint16_t)p, 0, 0, 0);    /* CLEAR_FEATURE(C_PORT_CONNECTION) */
    if (!done || !(d[0] & 2u)) return -1;               /* not enabled */
    if (d[1] & 0x02u) {                                 /* low speed: needs PRE packets, not done here */
        uh_ctrl(0x23u, 1u, 1u, (uint16_t)p, 0, 0, 0);   /* CLEAR_FEATURE(PORT_ENABLE) */
        return -2;
    }
    uh_wait(20);
    return 0;
}
/* the device just enumerated (uh_dev, usbh.addr): a drive is read; a hub's ports are each reset, enumerated and
 * visited in turn. 1 = a drive read (its lines stay on the page) */
static int uh_visit(uint32_t depth)
{
    uint32_t mask, p;
    uint8_t hub;
    uh_log(1);
    if (uh_dev.cls == 8u && usbh.ep_in && usbh.ep_out && uh_dev.proto == 0x50u)
        return uh_msc();
    if (uh_dev.cls != 9u || depth >= UH_DEPTH)
        return 0;
    hub = usbh.addr;
    mask = uh_hub_ports();
    for (p = 1; p <= 7u; p++) {
        if (!(mask & (1u << p)) || uh_walk.next >= 30u)
            continue;
        usbh.addr = hub;
        uh_walk.path[depth] = (uint8_t)p;
        uh_walk.depth = (uint8_t)(depth + 1u);
        if (uh_port_reset(p) < 0) {
            uh_walk.depth = (uint8_t)depth;
            continue;
        }
        usbh.err = 0;
        if (uh_enumerate(++uh_walk.next) < 0) {         /* off the bus: it must not answer at address 0 later */
            uh_log(0);
            usbh.addr = hub;
            uh_ctrl(0x23u, 1u, 1u, (uint16_t)p, 0, 0, 0);   /* CLEAR_FEATURE(PORT_ENABLE) */
            uh_walk.depth = (uint8_t)depth;
            continue;
        }
        if (uh_visit(depth + 1u))
            return 1;
        uh_walk.depth = (uint8_t)depth;
    }
    return 0;
}

static const char *uh_err_name(int e)
{
    static const char *const N[] = {"", "timeout", "stall", "error", "no answer", "short", "bad status", "failed"};
    return e > 0 && e < 8 ? N[e] : "";
}
static void uh_failed(const char *what)
{
    char b[UH_LINE], *e = uh_cat(b, what);
    if (usbh.err) { e = uh_cat(e, ": "); uh_cat(e, uh_err_name(usbh.err)); }
    uh_set(5, b);
}

/* the bus reset, then the walk from the device on the port */
static void uh_probe(void)
{
    uint32_t lines = UH_LINES_READ(), i;
    char b[UH_LINE], *e;
    usbh.err = 0;
    usbh.tag = 0;
    usbh.drive = 0;
    usbh.view = 0;
    uh_walk.next = 1;
    uh_walk.depth = 0;
    uh_walk.n = 0;
    uh_walk.why[0] = 0;
    uh_set(1, lines & 1u && !(lines & 2u) ? "Found: low speed" : "Found: full speed");
    usbh.step = 1;
    sie_wr(S_DEVCTL, 0x01);                             /* Session (again: some cores want it after connect) */
    sie_wr(S_POWER, 0x08);                              /* bus reset */
    uh_wait(50);
    sie_wr(S_POWER, 0x00);
    uh_wait(20);
    usbh.devctl = (uint8_t)sie_rd(S_DEVCTL);
    if (uh_enumerate(1) < 0) {
        uh_failed("Did not answer");
        return;
    }
    if (uh_visit(0)) {                                  /* a drive read: its lines are on the page */
        usbh.drive = 1;                                 /* (0.4.10) its files can be opened */
        if (uh_walk.depth) {
            e = uh_path(uh_cat(b, "Through hubs: "));
            (void)e;
            uh_set(1, b);
        }
        return;
    }
    if (uh_walk.next == 1u && !uh_walk.why[0]) {        /* one device, not a drive: its lines say what it is */
        e = uh_cat(b, uh_dev.cls == 8u ? "Storage, not bulk-only" : uh_dev.cls == 9u ? "Hub, nothing on it" :
                   "Not a drive, class ");
        if (uh_dev.cls != 8u && uh_dev.cls != 9u) uh_hex(e, uh_dev.cls, 2);
        uh_set(5, b);
        return;
    }
    for (i = 1; i < UH_LINES; i++) usbh.line[i][0] = 0;   /* no drive read: what was found, and why */
    uh_set(1, "No drive read");
    for (i = 0; i < uh_walk.n; i++) uh_set(2u + i, uh_walk.log[i]);
    if (uh_walk.why[0]) uh_set(7, uh_walk.why);
}

/* ---- the page ---- */
/* the page's side: only a wish (the UI never touches the controller; the host tests run it without one) */
static void usbh_want(int on) { usbh.want = (uint8_t)!!on; usbh.gen++; }
/* the controller as a host. Melodee found that it does not answer at power-on without a cable (usb.c usb_retry):
 * it may need 5 V on the port. usbh.sie says whether it answered; line 0 says it in words. */
static void uh_attach(void)
{
    fm1_usb_off();
    fm1_usb_reset();
    fm1_delay_ms(25);
    fm1_usb_host_attach(ep0buf);
    usb.timeouts = 0;
    fm1_usb_ep_enable(0x1Fu);
    sie_wr(S_INTRUSBE, 0);
    sie_wr(S_INTRTX1E, 0);
    sie_wr(S_INTRRX1E, 0);
    sie_wr(S_DEVCTL, 0x01);                             /* Session */
    fm1_delay_ms(5);
    usbh.devctl = (uint8_t)sie_rd(S_DEVCTL);
    usbh.sie = usb.timeouts == 0;
    uh_set(0, !usbh.sie ? "USB chip silent" : usbh.devctl & 0x04u ? "Host mode on" : "Host mode unsure");
}
static void usbh_open(void)
{
    if (usbh.st != UH_OFF) return;
    usb.up = 0;                                         /* TIMER5 leaves the SIE alone, usb_retry too */
    usb.config = 0;
    usb.suspended = 0;
    usb.detached = 1;
    uh_clear();
    uh_attach();
    uh_set(1, "Waiting for a drive");
    usbh.step = 0;
    usbh.err = 0;
    usbh.st = UH_WAIT;
    usbh.t_ms = fm1_ms;
}
static void usbh_close(void)
{
    if (usbh.st == UH_OFF) return;
    usbh.st = UH_OFF;
    usbh.drive = usbh.view = usbh.cmd = 0;
    fm1_usb_off();
    usb.detached = 0;
    usb.retry_ms = fm1_ms;
    usb_start();                                        /* the device again (usb_retry, if the SIE does not answer) */
    uh_clear();
}
/* main loop: open or close as the page wants; watch the lines; a device seen for 150 ms is probed; gone for 300 ms,
 * wait again */
static void usbh_task(void)
{
    uint32_t lines;
    if (usbh.want && usbh.st == UH_OFF) usbh_open();
    if (!usbh.want && usbh.st != UH_OFF) usbh_close();
    if (usbh.st == UH_OFF) return;
    lines = UH_LINES_READ();
    switch (usbh.st) {
    case UH_WAIT:
        if (lines) { usbh.st = UH_SEEN; usbh.t_ms = fm1_ms; }
        else if (!usbh.sie && fm1_ms - usbh.t_ms >= 1000u) {   /* not answering: try again each second */
            uh_attach();                                /* (5 V may arrive with what is plugged in) */
            usbh.t_ms = fm1_ms;
        }
        break;
    case UH_SEEN:
        if (!lines) { usbh.st = UH_WAIT; break; }
        if (fm1_ms - usbh.t_ms >= 150u) {
            if (!usbh.sie) uh_attach();
            uh_probe();
            usbh.st = UH_DONE;
            usbh.t_ms = fm1_ms;
        }
        break;
    case UH_DONE:
        if (lines) usbh.t_ms = fm1_ms;
        else if (fm1_ms - usbh.t_ms >= 300u) {          /* unplugged: clear and wait for the next one */
            char keep[UH_LINE];
            str_cpy(keep, usbh.line[0], UH_LINE);
            uh_clear();
            uh_set(0, keep);
            uh_set(1, "Waiting for a drive");
            usbh.step = 0;
            usbh.err = 0;
            usbh.drive = usbh.view = 0;
            usbh.st = UH_WAIT;
        }
        break;
    }
}
/* (0.4.10) OCT+ on a test that read no drive: look again, with what is plugged in now */
static void usbh_reprobe(void)
{
    uint32_t i;
    if (usbh.st != UH_DONE) return;
    for (i = 1; i < UH_LINES; i++) usbh.line[i][0] = 0;
    usbh.gen++;
    usbh.st = UH_SEEN;
    usbh.t_ms = fm1_ms - 150u;
}
/* the diagnostic line: "L2 A1 S35 C01 D5D E0" (lines, the device's address (0.4.8), step, last CSR, DEVCTL, last
 * error) */
static void usbh_diag(char *b)
{
    char *e = uh_cat(b, "L");
    e = uh_dec(e, UH_LINES_READ());
    e = uh_cat(e, " A");
    e = uh_dec(e, usbh.addr);
    e = uh_cat(e, " S");
    e = uh_dec(e, usbh.step);
    e = uh_cat(e, " C");
    e = uh_hex(e, usbh.csr, 2);
    e = uh_cat(e, " D");
    e = uh_hex(e, usbh.devctl, 2);
    e = uh_cat(e, " E");
    uh_dec(e, (uint32_t)(usbh.err < 0 ? 0 : usbh.err));
}
