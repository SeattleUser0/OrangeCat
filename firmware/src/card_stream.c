/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat 0.4.12: the card read for a playing WAV, from the 10 kHz TIMER5 interrupt (main.c calls cs_tick while
 * cs.run). Whole SCSI READ(10) commands, one step a tick: the command block out (31 bytes on host EP1), each 64-byte
 * data packet in, the status in. A tick looks at one register and, if a packet has landed, turns it into frames in
 * card_play.c's ring (cs_feed): it never waits on the bus. The file's place on the card comes from the main loop
 * (usb_files.c uf_extents: runs of sectors); this file only follows it, round and round (a loop).
 *
 * The bus has one user at a time: the main loop (FatFs listing a folder, the drive test) calls cs_claim, which lets
 * the command in hand finish and then holds the engine, and cs_release after. Errors (a stall, a status that did
 * not pass, 2 s with nothing) stop the engine (cs.err); the main loop stops the playing and resets the drive.
 *
 * Its cost is measured where it is spent: TIMER4 ticks inside cs_tick (cs.cpu_ticks), which the page shows as
 * "card n%". Included by melodee.c after card_play.c and usb_host.c (it uses their ring, SIE access and buffers).
 *
 * Revisions:
 *   0.4.12: first version.
 */
/* ---- (0.4.12) the stream: the card read from TIMER5 ---- */
#define CS_SECS 16u                                     /* sectors a READ(10): 8 KB */
#define CS_EXT 32u                                      /* runs of sectors a file may be in (more: the old way) */
enum { CS_IDLE, CS_CBW, CS_DATA, CS_CSW };
static struct {
    volatile uint8_t run, hold, err;                    /* stream; the main loop wants the bus; stopped on an error */
    volatile uint8_t state;
    uint8_t bpf, bytes, right;                          /* the file's bytes a frame, a sample, the right channel's offset */
    uint32_t data0, data, pos;                          /* the samples: their file offset, length; how far fed */
    uint32_t skip, take, got, len;                      /* this command: bytes to drop first, to feed, fed so far; its
                                                         * data bytes (whole sectors) */
    uint32_t ticks;                                     /* ticks in this state (a timeout) */
    uint32_t cmds, busy_ticks;                          /* commands done; ticks with work */
    uint32_t cpu_ticks;                                 /* TIMER4 ticks spent in cs_tick (the page's "card" share) */
    uint8_t carry[8], nc;                               /* a frame split over two packets */
    uint8_t next;                                       /* extents: how many */
    struct { uint32_t fsec, lba, n; } ext[CS_EXT];      /* file sector fsec.. lies at lba.. for n sectors */
} cs;

/* bytes into frames in the ring (the first two channels, the top 16 bits); a frame split over packets waits in
 * carry. The caller has checked the room. */
static void cs_frame(const uint8_t *p)
{
    int16_t *o = cp_ring[cp_w & (CP_FRAMES - 1u)];
    uint32_t b = cs.bytes;
    if (b == 1u) {
        o[0] = (int16_t)((p[0] - 128) << 8);
        o[1] = (int16_t)((p[cs.right] - 128) << 8);
    } else {
        o[0] = (int16_t)(p[b - 2u] | p[b - 1u] << 8);
        o[1] = (int16_t)(p[cs.right + b - 2u] | p[cs.right + b - 1u] << 8);
    }
    RING_PUBLISH();
    cp_w = cp_w + 1u;
}
static void cs_feed(const uint8_t *p, uint32_t n)
{
    uint32_t bpf = cs.bpf;
    while (n && cs.nc) {                                /* finish the split frame */
        cs.carry[cs.nc++] = *p++;
        n--;
        if (cs.nc == bpf) { cs_frame(cs.carry); cs.nc = 0; }
    }
    while (n >= bpf) { cs_frame(p); p += bpf; n -= bpf; }
    while (n) { cs.carry[cs.nc++] = *p++; n--; }
}
/* the next command, if the ring has room for all it brings: 0 = not now */
static int cs_plan(uint32_t *lba, uint32_t *nsec)
{
    uint32_t off, fsec, i, room;
    if (cs.pos >= cs.data) { cs.pos = 0; cs.nc = 0; }   /* the end: round again */
    off = cs.data0 + cs.pos;
    fsec = off >> 9;
    for (i = 0; i < cs.next; i++)
        if (fsec >= cs.ext[i].fsec && fsec < cs.ext[i].fsec + cs.ext[i].n) break;
    if (i == cs.next) { cs.err = 3; return 0; }
    *nsec = cs.ext[i].fsec + cs.ext[i].n - fsec;
    if (*nsec > CS_SECS) *nsec = CS_SECS;
    *lba = cs.ext[i].lba + (fsec - cs.ext[i].fsec);
    cs.skip = off & 511u;
    cs.take = *nsec * 512u - cs.skip;
    if (cs.take > cs.data - cs.pos) cs.take = cs.data - cs.pos;
    room = (CP_FRAMES - 2u - (cp_w - cp_r)) * cs.bpf;   /* (2: the ISR's straight line looks one frame ahead) */
    if (room < cs.take + cs.bpf) return 0;
    cs.len = *nsec * 512u;
    cs.got = 0;
    return 1;
}
/* one step a tick (TIMER5, nested in the audio render or not): never waits */
static void cs_step(void);
static void cs_tick(void)
{
    uint32_t t0;
    if (!cs.run || cs.err) return;
    t0 = fm1_ticks();
    cs_step();
    cs.cpu_ticks += fm1_ticks() - t0;
}
static void cs_step(void)
{
    uint32_t c, k, lba, nsec, i;
    switch (cs.state) {
    case CS_IDLE:
        if (cs.hold || !cs_plan(&lba, &nsec)) return;
        for (i = 0; i < 31u; i++) ep1tx[i] = 0;         /* the CBW: READ(10), nsec sectors at lba, from the slot */
        ep1tx[0] = 'U'; ep1tx[1] = 'S'; ep1tx[2] = 'B'; ep1tx[3] = 'C';
        usbh.tag++;
        for (i = 0; i < 4u; i++) {
            ep1tx[4 + i] = (uint8_t)(usbh.tag >> (8u * i));
            ep1tx[8 + i] = (uint8_t)(cs.len >> (8u * i));
        }
        ep1tx[12] = 0x80; ep1tx[13] = usbh.lun; ep1tx[14] = 10;
        ep1tx[15] = 0x28;
        ep1tx[17] = (uint8_t)(lba >> 24); ep1tx[18] = (uint8_t)(lba >> 16); ep1tx[19] = (uint8_t)(lba >> 8); ep1tx[20] = (uint8_t)lba;
        ep1tx[23] = (uint8_t)nsec;
        fm1_usb_ep_send(1, ep1tx, 31);
        sie_wr(S_INDEX, 1);
        sie_wr(S_TXCSR1, 0x01u);
        cs.state = CS_CBW;
        cs.ticks = 0;
        cs.busy_ticks++;
        return;
    case CS_CBW:
        sie_wr(S_INDEX, 1);
        c = sie_rd(S_TXCSR1);
        if (c & 0xA4u) { cs.err = 1; return; }          /* stall, error, NAK timeout */
        if (c & 0x01u) break;
        sie_wr(S_RXCSR1, 0x20u);                        /* ReqPkt: the first data packet */
        cs.state = CS_DATA;
        cs.ticks = 0;
        cs.busy_ticks++;
        return;
    case CS_DATA:
    case CS_CSW:
        sie_wr(S_INDEX, 1);
        c = sie_rd(S_RXCSR1) | sie_rd(S_RXCSR2) << 8;
        if (c & 0x4Cu) { cs.err = 2; return; }          /* stall, error, NAK timeout */
        if (!(c & 0x01u)) break;
        k = sie_rd(S_RXCOUNT1) | sie_rd(S_RXCOUNT2) << 8;
        if (k > 64u) k = 64u;
        fm1_usb_rx_sync();
        cs.busy_ticks++;
        if (cs.state == CS_DATA) {
            const uint8_t *p = ep1rx;
            uint32_t n = k, d;
            d = cs.skip < n ? cs.skip : n;              /* the bytes before the samples */
            cs.skip -= d; p += d; n -= d;
            d = cs.take - cs.got < n ? cs.take - cs.got : n;   /* the samples */
            cs_feed(p, d);
            cs.got += d;
            cs.len -= k < cs.len ? k : cs.len;
        } else if (k != 13u || ep1rx[0] != 'U' || ep1rx[3] != 'S' || ep1rx[12]) {
            cs.err = 4;                                 /* the status: not passed */
        }
        c = (c & ~0x164u) | 0x10u;                      /* the packet taken (as uh_in) */
        sie_wr(S_RXCSR1, c & 0xFFu);
        sie_wr(S_RXCSR2, c >> 8);
        if (cs.err) return;
        if (cs.state == CS_DATA && cs.len && k == 64u) {
            sie_wr(S_RXCSR1, 0x20u);                    /* the next data packet */
        } else if (cs.state == CS_DATA) {
            sie_wr(S_RXCSR1, 0x20u);                    /* the status */
            cs.state = CS_CSW;
        } else {
            cs.pos += cs.got;
            cs.cmds++;
            cs.state = CS_IDLE;
        }
        cs.ticks = 0;
        return;
    }
    if (++cs.ticks > 20000u) cs.err = 5;                /* 2 s with nothing: give up */
}
/* the main loop wants the bus: wait for the command in hand to end (a few ms at most), then hold the engine */
#ifndef CS_WAIT
#define CS_WAIT() fm1_wdt_feed()                        /* (the timer runs the engine meanwhile; host tests step it) */
#endif
static int cs_claim(void)
{
    uint32_t t0 = fm1_ms;
    if (!cs.run) return 1;
    cs.hold = 1;
    while (cs.state != CS_IDLE && !cs.err && fm1_ms - t0 < 500u)
        CS_WAIT();
    return cs.state == CS_IDLE || cs.err;
}
static void cs_release(void) { cs.hold = 0; }
