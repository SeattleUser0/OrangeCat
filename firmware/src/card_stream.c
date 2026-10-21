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
 * (0.4.13) Up to four streams (card_play.c's slots), one engine: the bus carries one command at a time, so the
 * engine picks, each time it is free, the running stream whose ring is emptiest and has room for at least a quarter
 * of a ring, and reads up to 8 sectors (4 KB) for it. Each stream keeps its own place in its file, its runs of
 * sectors and its split frame; the command in hand belongs to cs.cur.
 *
 * Revisions:
 *   0.4.12: first version.
 *   0.4.13: four streams (cs.st[]), the emptiest served first; 8 sectors a command (was 16: a 10 KB ring); an error
 *           stops them all (the bus needs a reset).
 */
/* ---- the streams: the card read from TIMER5 ---- */
#define CS_SECS 8u                                      /* sectors a READ(10) at most: 4 KB */
#define CS_EXT 16u                                      /* runs of sectors a file may be in */
enum { CS_IDLE, CS_CBW, CS_DATA, CS_CSW };
struct cs_one {
    uint8_t run;                                        /* reading for this slot */
    uint8_t bpf, bytes, right, ch;                      /* the file's bytes a frame, a sample, the right channel's offset;
                                                         * channels kept (1, 2) */
    uint32_t data0, data, pos;                          /* the samples: their file offset, length; how far fed */
    uint8_t carry[32], nc;                              /* a frame split over two packets (8 channels of 32 bits at most) */
    uint8_t next;                                       /* extents: how many */
    struct { uint32_t fsec, lba, n; } ext[CS_EXT];      /* file sector fsec.. lies at lba.. for n sectors */
};
static struct {
    volatile uint8_t run, hold, err;                    /* any stream reading; the main loop wants the bus; stopped */
    volatile uint8_t state;
    uint8_t cur;                                        /* the stream the command in hand is for */
    uint32_t skip, take, got, len;                      /* this command: bytes to drop first, to feed, fed so far; its
                                                         * data bytes (whole sectors) */
    uint32_t ticks;                                     /* ticks in this state (a timeout) */
    uint32_t cmds;                                      /* commands done */
    uint32_t cpu_ticks;                                 /* TIMER4 ticks spent in cs_tick (the page's "card" share) */
    struct cs_one st[CP_N];
} cs;

/* bytes into frames in stream s's ring (the first two channels, the top 16 bits); a frame split over packets waits
 * in carry. The caller has checked the room. */
static void cs_frame(uint32_t s, const uint8_t *p)
{
    const struct cs_one *o = &cs.st[s];
    uint32_t b = o->bytes;
    int16_t l, r;
    if (b == 1u) {
        l = (int16_t)((p[0] - 128) << 8);
        r = (int16_t)((p[o->right] - 128) << 8);
    } else {
        l = (int16_t)(p[b - 2u] | p[b - 1u] << 8);
        r = (int16_t)(p[o->right + b - 2u] | p[o->right + b - 1u] << 8);
    }
    cp_put(s, l, r);
}
static void cs_feed(uint32_t s, const uint8_t *p, uint32_t n)
{
    struct cs_one *o = &cs.st[s];
    uint32_t bpf = o->bpf;
    while (n && o->nc) {                                /* finish the split frame */
        o->carry[o->nc++] = *p++;
        n--;
        if (o->nc == bpf) { cs_frame(s, o->carry); o->nc = 0; }
    }
    while (n >= bpf) { cs_frame(s, p); p += bpf; n -= bpf; }
    while (n) { o->carry[o->nc++] = *p++; n--; }
}
/* the command stream s would read next, if its ring has room for all of it: 0 = not now */
static int cs_plan_one(uint32_t s, uint32_t *lba, uint32_t *nsec)
{
    struct cs_one *o = &cs.st[s];
    uint32_t off, fsec, i, room, take;
    if (o->pos >= o->data) { o->pos = 0; o->nc = 0; }   /* the end: round again */
    off = o->data0 + o->pos;
    fsec = off >> 9;
    for (i = 0; i < o->next; i++)
        if (fsec >= o->ext[i].fsec && fsec < o->ext[i].fsec + o->ext[i].n) break;
    if (i == o->next) { cs.err = 3; return 0; }
    room = cp_room(s);
    if (room < cp[s].cap / 4u) return 0;                /* (a quarter free first: commands worth the trip) */
    room = room * o->bpf - o->nc;                       /* file bytes the room takes */
    *nsec = o->ext[i].fsec + o->ext[i].n - fsec;
    if (*nsec > CS_SECS) *nsec = CS_SECS;
    while (*nsec > 1u && *nsec * 512u - (off & 511u) > room) (*nsec)--;
    take = *nsec * 512u - (off & 511u);
    if (take > o->data - o->pos) take = o->data - o->pos;
    if (take > room) return 0;
    *lba = o->ext[i].lba + (fsec - o->ext[i].fsec);
    cs.skip = off & 511u;
    cs.take = take;
    cs.len = *nsec * 512u;
    cs.got = 0;
    return 1;
}
/* the next command: the running stream with the emptiest ring that has room */
static int cs_plan(uint32_t *lba, uint32_t *nsec)
{
    uint32_t s, best = CP_N, fill, best_fill = ~0u;
    for (s = 0; s < CP_N; s++) {
        if (!cs.st[s].run) continue;
        fill = (cp[s].w - cp[s].r) * 1024u / cp[s].cap;  /* (per mille of the ring, near enough) */
        if (fill < best_fill) { best_fill = fill; best = s; }
    }
    for (s = 0; s < CP_N && best < CP_N; s++) {         /* the emptiest first; if it has no room, the next */
        uint32_t k = (best + s) % CP_N;
        if (cs.st[k].run && cs_plan_one(k, lba, nsec)) { cs.cur = (uint8_t)k; return 1; }
        if (cs.err) return 0;
    }
    return 0;
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
        return;
    case CS_CBW:
        sie_wr(S_INDEX, 1);
        c = sie_rd(S_TXCSR1);
        if (c & 0xA4u) { cs.err = 1; return; }          /* stall, error, NAK timeout */
        if (c & 0x01u) break;
        sie_wr(S_RXCSR1, 0x20u);                        /* ReqPkt: the first data packet */
        cs.state = CS_DATA;
        cs.ticks = 0;
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
        if (cs.state == CS_DATA) {
            const uint8_t *p = ep1rx;
            uint32_t n = k, d;
            d = cs.skip < n ? cs.skip : n;              /* the bytes before the samples */
            cs.skip -= d; p += d; n -= d;
            d = cs.take - cs.got < n ? cs.take - cs.got : n;   /* the samples */
            if (cs.st[cs.cur].run) cs_feed(cs.cur, p, d);   /* (a stream stopped mid-command: its bytes dropped) */
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
            cs.st[cs.cur].pos += cs.got;
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
/* streams reading: cs.run kept as "any" (main.c's test) */
static void cs_update_run(void)
{
    uint32_t s, any = 0;
    for (s = 0; s < CP_N; s++) any |= cs.st[s].run;
    cs.run = (uint8_t)any;
}
