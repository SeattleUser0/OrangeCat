/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat: the USB port as a host, for the card frozen tracks are recorded to (build doc phase 2; spec 10.6). While
 * a track is frozen or freezing (frz[] below: the MIXER page's KNOB 3) the port is a host: it waits for something to be
 * plugged in, walks hubs (up to three deep, every port with something on it) until a mass storage drive answers and
 * reads (a powered hub is how a drive gets power: the FM-1 gives its port no 5 V), and sets usbh.drive. usb_files.c
 * then mounts it (FatFs) and writes and reads the takes. When no track is frozen any more, the port goes back to the
 * normal device: USB MIDI, USB audio and the web editor come back.
 *
 * The controller is a MUSB-style core (src/usb.c); the host bits are in hal/fm1_usb.h (fm1_usb_host_attach). All of
 * it runs in the main loop: usb.up = 0 and usb.detached = 1 keep usb_poll, ua_service and usb_retry (TIMER5, main
 * loop) off the SIE while the host owns it. A probe blocks the main loop for a few seconds at most (the audio runs
 * on; the watchdog is fed).
 *
 * The SIE registers (MUSB byte map, JieLi's usb_phy.h): DEVCTL 0x0F (bit 0 Session, bit 2 HostMode, bit 5 LSDev,
 * bit 6 FSDev), TXTYPE 0x18, TXINTERVAL 0x19, RXTYPE 0x1A, RXINTERVAL 0x1B; host CSR0: RxPktRdy 0x01, TxPktRdy
 * 0x02, RxStall 0x04, SetupPkt 0x08, Error 0x10, ReqPkt 0x20, StatusPkt 0x40, NAK timeout 0x80; host TXCSR1:
 * TxPktRdy 0x01, Error 0x04, RxStall 0x20, ClrDataTog 0x40, NAK timeout 0x80; host RXCSR1: RxPktRdy 0x01, Error
 * 0x04, DataError / NAK timeout 0x08, FlushFIFO 0x10, ReqPkt 0x20, RxStall 0x40, ClrDataTog 0x80.
 * The buffers are usb.c's (ep0buf, ep1tx, ep1rx): device mode is off while they are used here.
 *
 * Revisions:
 *   0.4.7: first version: Menu > USB DRIVE TEST, a page of what the probe found.
 *   0.4.8: the first test on a unit (RCA C109 hub on PD power) got through the hub to a second hub inside it
 *          (1A40:0801) and stopped: one hub was all 0.4.7 followed. Now the whole tree is walked (uh_visit), a
 *          storage device that does not read (a card reader with no card) is passed over for the next. Waits feed
 *          the watchdog (uh_wait): a walk can take several seconds.
 *   0.4.9 (after the second unit test): the SD card in the hub's slot read (29.7 GB, FAT32, 914 KB/s: about what full
 *          speed USB carries). A card reader stick in the hub's port 3 failed its first command (INQUIRY). Now:
 *          GET_MAX_LUN first (a reader may have a slot for each card type; each is tried), 300 ms before the first
 *          command (Linux waits a second), one retry after a Bulk-Only reset and 1 s.
 *   0.4.10: the drive's files (usb_files.c): usbh.drive says a drive was read and stays until it is unplugged.
 *   0.4.16: WRITE(10): uh_scsi_w (through FatFs: usb_files.c disk_write).
 *   0.4.20: frozen tracks' state (frz[], below: the MIXER page's KNOB 3 asks, usb_files.c does it, card_freeze.c plays
 *           and records); the port stays a host while any track is frozen or freezing.
 *   0.5.6: a freeze belongs to a phrase (card_freeze.c frzp[]); frz[] is a freeze being made; frz_need; frz_word moved
 *          to card_freeze.c (it reads the phrases).
 *   0.5.3: the drive test is gone (it was for finding out whether the FM-1 could use a drive at all, and it can):
 *          no page, no lines of what was found (uh_set, the device's strings, the list of hubs and disks, the
 *          diagnostic line), no sector 0 look, no read speed test. What freezing needs stays: the host, the walk
 *          through hubs, the drive made ready (INQUIRY, the slots, TEST UNIT READY, READ CAPACITY: 512-byte blocks).
 */
#ifndef UH_LINES_READ
#define UH_LINES_READ() fm1_usb_lines()                 /* (host tests replace the hardware read) */
#endif
enum { S_DEVCTL = 15, S_TXTYPE = 24, S_TXINTERVAL = 25, S_RXTYPE = 26, S_RXINTERVAL = 27 };
enum { UH_OFF, UH_WAIT, UH_SEEN, UH_DONE };
/* errors (negative) */
enum { UE_TIME = 1, UE_STALL, UE_ERR, UE_NAK, UE_SHORT, UE_CSW, UE_CHECK };
static struct {
    uint8_t st;                 /* UH_* */
    uint8_t addr, mps0;         /* the device being talked to, its EP0 packet size */
    uint8_t ep_in, ep_out;      /* bulk endpoints (mass storage) */
    uint8_t csr;                /* the last CSR seen */
    uint8_t sie;                /* 1: the controller answered when attached as a host */
    uint8_t want;               /* frozen tracks ask (frz_task); the main loop opens and closes (usbh_task) */
    uint8_t lun, ifc;           /* (0.4.9) the drive's slot (SCSI LUN) commands go to; its interface number */
    int8_t err;                 /* the last error (UE_*), 0 none */
    uint8_t drive;              /* (0.4.10) a drive read (its address, slot and endpoints set), until it is unplugged */
    uint32_t t_ms;              /* when the lines changed */
    uint32_t tag;               /* mass storage command tag */
} usbh;
static uint8_t uh_sec[512];     /* a sector (usb_files.c: a take's header) */
/* (0.4.8) the walk through hubs: the address the next device gets, how deep the walk is */
#define UH_DEPTH 3u
static struct {
    uint8_t next;
    uint8_t depth;
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

/* numbers into text (usb_files.c: the takes' names) */
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

/* (0.4.9) Bulk-Only Mass Storage Reset, then both endpoints' halts cleared */
static void uh_bot_reset(void)
{
    uh_ctrl(0x21u, 0xFFu, 0, usbh.ifc, 0, 0, 0);
    uh_unstall((uint8_t)(usbh.ep_in | 0x80u));
    uh_unstall(usbh.ep_out);
}
/* the drive made ready: INQUIRY (once more after a reset), each slot until one is ready and its size reads (READ
 * CAPACITY), blocks of 512 bytes (what FatFs here takes). 1 = a drive to use, at usbh.lun */
static int uh_msc(void)
{
    static const uint8_t INQ[6] = {0x12, 0, 0, 0, 36, 0}, TUR[6] = {0}, SENSE[6] = {0x03, 0, 0, 0, 18, 0};
    static const uint8_t CAP[10] = {0x25};
    uint8_t *d = uh_sec;
    uint32_t tries, lun, maxlun, got = 0;
    uint8_t m = 0;
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
            return 0;
    }
    for (lun = 0; lun <= maxlun; lun++) {             /* (0.4.9) each slot until one has a card that reads */
        usbh.lun = (uint8_t)lun;
        for (tries = 0; tries < (maxlun ? 10u : 20u); tries++) {
            if (uh_scsi(TUR, 6, 0, 0) == 0) break;
            uh_scsi(SENSE, 6, d, 18);
            uh_wait(100);
        }
        if (uh_scsi(CAP, 10, d, 8) == 0)
            break;
    }
    if (lun > maxlun) {
        usbh.lun = 0;
        return 0;
    }
    return uh_be32(d + 4) == 512u;
}

/* ---- enumeration ---- */
static struct { uint8_t cls, sub, proto, cfg; } uh_dev;
/* the device at address 0 (just reset) to address a: its descriptors, configuration */
static int uh_enumerate(uint8_t a)
{
    uint8_t d[64];
    uint32_t got = 0, total, i, k;
    int r;
    usbh.addr = 0;
    usbh.mps0 = 8;
    if ((r = uh_get_desc(1, 0, 0, 18, d, &got)) < 0 || got < 8u) return r < 0 ? r : uh_fail(UE_SHORT);
    usbh.mps0 = d[7] >= 8u ? d[7] : 8u;
    if ((r = uh_ctrl(0x00u, 5u, a, 0, 0, 0, 0)) < 0) return r;     /* SET_ADDRESS */
    uh_wait(10);
    usbh.addr = a;
    if ((r = uh_get_desc(1, 0, 0, 18, d, &got)) < 0 || got < 18u) return r < 0 ? r : uh_fail(UE_SHORT);
    uh_dev.cls = d[4];
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
    if ((r = uh_ctrl(0x00u, 9u, uh_dev.cfg, 0, 0, 0, 0)) < 0) return r;     /* SET_CONFIGURATION */
    return 0;
}

/* ---- hubs (0.4.8: the whole tree) ---- */
/* the hub at usbh.addr: its ports powered, then which have something on them, a bit per port (1..7). Devices take
 * a moment after power: watch up to 2 s, and stop once the answer has held for 300 ms */
static uint32_t uh_hub_ports(void)
{
    uint8_t d[16];
    uint32_t got = 0, p, n, mask = 0, last = 0, same = 0, t0;
    if (uh_ctrl(0xA0u, 6u, 0x2900u, 0, 9, d, &got) < 0 || got < 3u) return 0;   /* the hub descriptor */
    n = d[2] > 7u ? 7u : d[2];
    for (p = 1; p <= n; p++)
        uh_ctrl(0x23u, 3u, 8u, (uint16_t)p, 0, 0, 0);   /* SET_FEATURE(PORT_POWER) */
    uh_wait(2u * d[5] + 100u);                          /* bPwrOn2PwrGood (2 ms units) and a margin */
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
/* the device just enumerated (uh_dev, usbh.addr): a drive is made ready; a hub's ports are each reset, enumerated
 * and visited in turn. 1 = a drive ready */
static int uh_visit(uint32_t depth)
{
    uint32_t mask, p;
    uint8_t hub;
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
        uh_walk.depth = (uint8_t)(depth + 1u);
        if (uh_port_reset(p) < 0) {
            uh_walk.depth = (uint8_t)depth;
            continue;
        }
        usbh.err = 0;
        if (uh_enumerate(++uh_walk.next) < 0) {         /* off the bus: it must not answer at address 0 later */
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

/* the bus reset, then the walk from the device on the port */
static void uh_probe(void)
{
    usbh.err = 0;
    usbh.tag = 0;
    usbh.drive = 0;
    uh_walk.next = 1;
    uh_walk.depth = 0;
    sie_wr(S_DEVCTL, 0x01);                             /* Session (again: some cores want it after connect) */
    sie_wr(S_POWER, 0x08);                              /* bus reset */
    uh_wait(50);
    sie_wr(S_POWER, 0x00);
    uh_wait(20);
    if (uh_enumerate(1) < 0)
        return;
    if (uh_visit(0))
        usbh.drive = 1;                                 /* (0.4.10) its files can be opened */
}

/* ---- (0.4.20) frozen tracks: a track's freeze in progress, shared by the MIXER page (ui_input.c, ui_draw.c), the
 * card side (usb_files.c frz_task) and the audio side (card_freeze.c) ----
 * (0.5.6) A freeze belongs to a phrase now (card_freeze.c frzp[]: its take, kept on the card and in the record
 * store); frz[] is only a track's freeze while it is made. KNOB 3 right asks (want +1) to freeze the phrase the
 * track is on; the main loop wakes the port and the card (FZ_CARD), opens the take's file and arms the audio side
 * (FZ_ARMED: it waits for a pass of that phrase, with one pass played before so its tails are in); the audio side
 * records the pass (FZ_REC) and a little past its end (FZ_POST); the main loop blends that into the take's start,
 * cuts it to the pass and hands it to the phrase (FZ_SAVE), and the freeze is done (FZ_OFF): from then on the take
 * plays whenever a track plays that phrase. KNOB 3 left (want -1): cancel a freeze, or unfreeze the phrase the
 * track is on. FZ_FAIL: the audio side gave up (the sequencer stopped mid-take); the main loop cleans up. why: the
 * last word for the page (an error). frz_need: bit k, track k needs the card (a frozen phrase in its banks, or a
 * freeze under way): the port stays a host. */
enum { FZ_OFF, FZ_CARD, FZ_ARMED, FZ_REC, FZ_POST, FZ_SAVE, FZ_ON_UNUSED, FZ_FAIL };
static struct {
    volatile uint8_t st;
    volatile int8_t want;                               /* the knob: +1 freeze, -1 unfreeze or cancel */
    uint8_t phrase;                                     /* (0.5.6) the phrase being frozen (1..64) */
    uint16_t take;                                      /* its file: T<k+1>_<take>.WAV in folder dir */
    uint8_t dir;                                        /* (0.6.1) its folder (usb_files.c frzd): the project's */
    int16_t lvl;                                        /* its LEVEL when frozen (the take has it in) */
    int16_t bpm;                                        /* (0.5.6) the tempo when frozen */
    uint32_t hash;                                      /* (0.5.6) the phrase's CRC when frozen (card_freeze.c frzp_t) */
    uint32_t t0;                                        /* (FZ_CARD) since when */
    char why[10];                                       /* the page's word after a failure ("Card?") */
} frz[4];
static volatile uint8_t frz_need;                       /* (0.5.6) see above */
static void frz_knob(uint32_t k, int32_t dir) { if (k < 4u && dir) frz[k].want = (int8_t)(dir > 0 ? 1 : -1); }
static volatile uint8_t uf_browse;                      /* (0.6.1) the card's browser open (usb_files.c fb): the port
                                                         * stays a host (main.c) */
static int frz_busy(void)                               /* a frozen phrase in use or a freeze under way: a host */
{
    uint32_t k;
    if (frz_need) return 1;
    for (k = 0; k < 4u; k++) if (frz[k].st != FZ_OFF || frz[k].want > 0) return 1;
    return 0;
}
/* the port a host (on) or the normal device again; frozen tracks keep it a host (frz_task asks) */
static void usbh_want(int on)
{
    if (!on && frz_busy()) return;                      /* (0.4.20) frozen tracks keep the card */
    usbh.want = (uint8_t)!!on;
}
/* the controller as a host. Melodee found that it does not answer at power-on without a cable (usb.c usb_retry):
 * it may need 5 V on the port. usbh.sie says whether it answered. */
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
    sie_rd(S_DEVCTL);
    usbh.sie = usb.timeouts == 0;
}
static void usbh_open(void)
{
    if (usbh.st != UH_OFF) return;
    usb.up = 0;                                         /* TIMER5 leaves the SIE alone, usb_retry too */
    usb.config = 0;
    usb.suspended = 0;
    usb.detached = 1;
    uh_attach();
    usbh.err = 0;
    usbh.st = UH_WAIT;
    usbh.t_ms = fm1_ms;
}
static void usbh_close(void)
{
    if (usbh.st == UH_OFF) return;
    usbh.st = UH_OFF;
    usbh.drive = 0;
    fm1_usb_off();
    usb.detached = 0;
    usb.retry_ms = fm1_ms;
    usb_start();                                        /* the device again (usb_retry, if the SIE does not answer) */
}
/* main loop: open or close as frozen tracks want; watch the lines; a device seen for 150 ms is probed; gone for 300 ms,
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
        else if (fm1_ms - usbh.t_ms >= 300u) {          /* unplugged: wait for the next one */
            usbh.err = 0;
            usbh.drive = 0;
            usbh.st = UH_WAIT;
        }
        break;
    }
}
