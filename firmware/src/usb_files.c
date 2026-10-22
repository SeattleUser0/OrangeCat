/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat: files on the card, for frozen tracks (build doc phase 2; spec 0.9, 10.6). The card is a USB drive the
 * port reads as a host (usb_host.c); its takes are WAVs in /ORANGECAT, recorded and played back here.
 *
 * The file system is FatFs (ChaN, R0.16 patch 2; firmware/src/fatfs, its own BSD-style licence, GPL compatible),
 * configured in fatfs/ffconf.h: reading and writing, long names, FAT12/16/32 and exFAT. Its disk layer is here: sectors
 * through usb_host.c's SCSI commands (READ(10), WRITE(10)) to the drive usb_host.c found, its slot (LUN) and endpoints.
 *
 * Who does what: the MIXER page only asks (usb_host.c frz[].want); this file runs from the main loop (usbf_task,
 * main.c), so the UI never waits on the card.
 *
 * Included by melodee.c after everything but main.c, since ff.c leaves its macros defined. The firmware has no C
 * library: fatfs/sys/string.h (on the firmware build's include path only) declares what ff.c uses; memset,
 * memcpy and memcmp are libc.c's, strchr and strlen are here, and so are __udivdi3 / __umoddi3 (the first build
 * failed to link without them). The host tests use their own C library (UF_HOST_LIBC).
 *
 * Revisions:
 *   0.4.10: first version: the drive test's file list.
 *   0.4.11 to 0.4.19: WAVs played from the list (up to four, in time with the sequencer), the recorder (REC on the list:
 *          the mix or one track, stereo), the card's speed limit (uf_budget).
 *   0.4.20: frozen tracks, the card side (frz_*, before usbf_task; the audio side: card_freeze.c).
 *   0.5.3: the drive test is gone, and with it its file list (folders, a file's details and read speed), its players
 *          (OCT+ and SEQ on a WAV, the foot's loads and gaps) and its recorder (REC, OC001.WAV ..). What freezing
 *          uses stays: the disk layer, the mount, a WAV's chunks (uf_wav_scan), a file's sectors (uf_extents), the
 *          take's header and writes (ur_*), a take rewound when the sequencer stops (uf_rearm), and frz_*.
 */
#ifndef UF_HOST_LIBC
char *strchr(const char *s, int c)                      /* ff.c: the characters a name may not hold */
{
    for (;; s++) {
        if (*s == (char)c) return (char *)s;
        if (!*s) return 0;
    }
}
unsigned strlen(const char *s) { unsigned n = 0; while (s[n]) n++; return n; }
/* 64-bit division (exFAT's sizes and offsets are 64-bit): the firmware links no compiler runtime, so the calls
 * clang makes for it land here. Shift and subtract; main loop only, never in the audio path. */
static unsigned long long uf_divmod64(unsigned long long n, unsigned long long d, unsigned long long *rem)
{
    unsigned long long q = 0, r = 0;
    int i;
    if (!d) { if (rem) *rem = 0; return ~0ull; }
    if (!(n >> 32) && !(d >> 32)) {                     /* the usual case: 32-bit */
        if (rem) *rem = (uint32_t)n % (uint32_t)d;
        return (uint32_t)n / (uint32_t)d;
    }
    for (i = 63; i >= 0; i--) {
        r = r << 1 | (n >> i & 1u);
        if (r >= d) { r -= d; q |= 1ull << i; }
    }
    if (rem) *rem = r;
    return q;
}
unsigned long long __udivdi3(unsigned long long n, unsigned long long d) { return uf_divmod64(n, d, 0); }
unsigned long long __umoddi3(unsigned long long n, unsigned long long d)
{
    unsigned long long r;
    uf_divmod64(n, d, &r);
    return r;
}
#endif
#include "fatfs/ff.c"
#include "fatfs/ffunicode.c"

/* ---- FatFs's disk layer: one drive, the one usb_host.c found ---- */
DSTATUS disk_status(BYTE pdrv) { return pdrv == 0 && usbh.drive ? 0 : STA_NOINIT; }
DSTATUS disk_initialize(BYTE pdrv) { return disk_status(pdrv); }
DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count)
{
    uint8_t cb[10];
    uint32_t i;
    if (pdrv || !usbh.drive) return RES_NOTRDY;
    if (!cs_claim()) return RES_ERROR;                  /* (0.4.12) a WAV streaming: between its commands */
    sie_wr(S_FADDR, usbh.addr);
    while (count) {
        uint32_t n = count > 64u ? 64u : count, lba = (uint32_t)sector;
        for (i = 0; i < 10u; i++) cb[i] = 0;
        cb[0] = 0x28;                                   /* READ(10) */
        cb[2] = (uint8_t)(lba >> 24); cb[3] = (uint8_t)(lba >> 16); cb[4] = (uint8_t)(lba >> 8); cb[5] = (uint8_t)lba;
        cb[8] = (uint8_t)n;
        if (uh_scsi(cb, 10, buff, n * 512u) < 0) { cs_release(); return RES_ERROR; }
        buff += n * 512u;
        sector += n;
        count -= n;
        uh_feed();
    }
    cs_release();
    return RES_OK;
}
/* (0.4.16) WRITE(10), up to 64 sectors (32 KB) a command */
DRESULT disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count)
{
    uint8_t cb[10];
    uint32_t i;
    if (pdrv || !usbh.drive) return RES_NOTRDY;
    if (!cs_claim()) return RES_ERROR;
    sie_wr(S_FADDR, usbh.addr);
    while (count) {
        uint32_t n = count > 64u ? 64u : count, lba = (uint32_t)sector;
        for (i = 0; i < 10u; i++) cb[i] = 0;
        cb[0] = 0x2A;                                   /* WRITE(10) */
        cb[2] = (uint8_t)(lba >> 24); cb[3] = (uint8_t)(lba >> 16); cb[4] = (uint8_t)(lba >> 8); cb[5] = (uint8_t)lba;
        cb[8] = (uint8_t)n;
        if (uh_scsi_w(cb, 10, buff, n * 512u) < 0) { cs_release(); return RES_ERROR; }
        buff += n * 512u;
        sector += n;
        count -= n;
        uh_feed();
    }
    cs_release();
    return RES_OK;
}
DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    (void)buff;
    if (pdrv || !usbh.drive) return RES_NOTRDY;
    return cmd == CTRL_SYNC ? RES_OK : RES_PARERR;
}

/* ---- the volume ---- */
static FATFS uf_fs;
static uint8_t uf_mounted;
static FRESULT uf_mount(void)
{
    FRESULT r;
    if (uf_mounted) return FR_OK;
    r = f_mount(&uf_fs, "", 1);
    if (r == FR_OK) uf_mounted = 1;
    return r;
}

/* (0.4.11) a WAV's format and where its samples are: the chunks walked in the file itself (a DAW's file can carry a
 * "bext" or "JUNK" chunk that puts the samples past the first 512 bytes). 1 = a WAV with "fmt " and "data" */
struct uf_wi { uint32_t fmt, ch, rate, bits, data0, data; };
static uint32_t uf_le32(const uint8_t *p) { return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static int uf_wav_scan(FIL *f, uint32_t fsize, struct uf_wi *w)
{
    uint8_t h[24];
    UINT got = 0;
    uint32_t pos = 12, k;
    w->fmt = w->ch = w->rate = w->bits = w->data0 = w->data = 0;
    if (f_lseek(f, 0) != FR_OK || f_read(f, h, 12, &got) != FR_OK || got < 12u ||
        memcmp(h, "RIFF", 4) || memcmp(h + 8, "WAVE", 4))
        return 0;
    for (k = 0; k < 64u && pos + 8u <= fsize; k++) {
        uint32_t len;
        if (f_lseek(f, pos) != FR_OK || f_read(f, h, 8, &got) != FR_OK || got < 8u) return 0;
        len = uf_le32(h + 4);
        if (!memcmp(h, "fmt ", 4)) {
            if (f_read(f, h + 8, 16, &got) != FR_OK || got < 16u) return 0;
            w->fmt = h[8] | (uint32_t)h[9] << 8;
            w->ch = h[10] | (uint32_t)h[11] << 8;
            w->rate = uf_le32(h + 12);
            w->bits = h[22] | (uint32_t)h[23] << 8;
        } else if (!memcmp(h, "data", 4)) {
            w->data0 = pos + 8u;
            w->data = len > fsize - w->data0 ? fsize - w->data0 : len;   /* (a recorder that never closed it) */
            return w->rate && w->ch && w->bits;
        }
        pos += 8u + len + (len & 1u);
    }
    return 0;
}
/* ---- (0.4.11) takes playing: the main loop's side (the audio and the reading: card_play.c, card_stream.c) ----
 * The main loop only starts and stops them: it maps a file onto the card (uf_extents: the runs of sectors its
 * clusters make) and hands that to the stream engine (cs), which the 10 kHz timer drives; the stream's ring is half
 * filled before the audio ISR starts to play it. At the end of the samples the engine starts again (a loop). */
/* (0.4.15) what the card keeps up with: 0.4.14 on the unit read 550 KB/s at most, with or without the sequencer
 * (3 files of 16-bit stereo 44.1 kHz, 516 KB/s, played clean; 4, 689 KB/s, dropped out). A freeze that would take the
 * playing takes past this does not start ("Full"). */
#ifndef UF_BUDGET
#define UF_BUDGET 545000u                               /* bytes a second (532 KB/s) */
#endif
static uint32_t uf_budget = UF_BUDGET;                  /* (host tests change it) */
static FIL uf_pf;
/* the file's clusters as runs of sectors (stream s's ext), from FatFs's own chain (ff.c's get_fat, clst2sect: one
 * unit with this file). 0 = more runs than ext holds, or a broken chain */
static int uf_extents(FIL *f, uint32_t fsize, struct cs_one *o)
{
    FATFS *fs = f->obj.fs;
    uint32_t bpc = (uint32_t)fs->csize * 512u, need = fsize / bpc + (fsize % bpc != 0u), n;
    DWORD clst = f->obj.sclust;
    o->next = 0;
    for (n = 0; n < need; n++) {
        LBA_t lba = clst2sect(fs, clst);
        if (!lba) return 0;
        if (o->next && o->ext[o->next - 1u].lba + o->ext[o->next - 1u].n == (uint32_t)lba) {
            o->ext[o->next - 1u].n += fs->csize;
        } else {
            if (o->next == CS_EXT) return 0;
            o->ext[o->next].fsec = n * fs->csize;
            o->ext[o->next].lba = (uint32_t)lba;
            o->ext[o->next].n = fs->csize;
            o->next++;
        }
        if (n + 1u < need) {
            clst = get_fat(&f->obj, clst);
            if (clst < 2u || clst >= fs->n_fatent) return 0;
        }
    }
    return 1;
}
/* stop them all; after an error the drive is reset (stopped mid-command: it must be back in step); frozen tracks
 * go back to their engines */
static void frz_drop_all(const char *why);              /* (0.4.20) below */
static void uf_play_stop_all(void)
{
    uint8_t err = cs.err;
    uint32_t s;
    if (!err && !cs.run) return;
    for (s = 0; s < CP_N; s++) cp[s].on = cp[s].armed = 0;
    cs_claim();
    for (s = 0; s < CP_N; s++) cs.st[s].run = 0;
    cs.run = 0;
    cs.hold = 0;
    if (err) uh_bot_reset();
    cs.err = 0;
    cs.state = CS_IDLE;
    frz_drop_all(err ? "Error" : "");
}
static void frz_unfreeze(uint32_t k, const char *why);  /* (0.4.20) below */
/* (0.4.17) a frozen take the sequencer stopped: back to its start, its ring half full, waiting for its cycle to come
 * round (card_freeze.c). 0: the card did not keep up (the track back to its engine: "Slow") */
static int uf_rearm(uint32_t s)
{
    struct cs_one *o = &cs.st[s];
    struct cp_stream *c = &cp[s];
    uint32_t t0, want;
    c->on = c->armed = 0;                               /* (the ISR leaves the slot alone from here) */
    c->ended = 0;
    cs_claim();                                         /* (a command for it ends first: its bytes go, then the reset) */
    o->pos = 0;
    o->nc = 0;
    c->w = c->r = c->wi = c->ri = c->phase = 0;
    c->shrt = 0;
    cs_release();
    want = o->data / o->bpf < c->cap / 2u ? o->data / o->bpf : c->cap / 2u;
    for (t0 = fm1_ms; c->w < want && !cs.err && fm1_ms - t0 < 1000u;)
        CS_WAIT();
    if (cs.err || c->w < want) {
        if (c->trk) frz_unfreeze(c->trk - 1u, "Slow");
        return 0;
    }
    c->armed = 1;
    return 1;
}

/* ---- (0.4.16) the recorder: a take, to a WAV on the card ----
 * The audio ISR fills card_play.c's ring (cr; a freeze: card_freeze.c frz_after); the main loop writes it out 8 KB at
 * a time (16 sectors, one WRITE(10)) with f_write. The file is a plain 16-bit 44.1 kHz WAV whose samples start at byte
 * 512 (a JUNK chunk fills the header's sector), so every write is whole sectors and goes straight to the card
 * (FatFs's window is not used for the samples). */
#define UR_CHUNK_B 8192u                                /* bytes a write: 16 sectors */
static FIL uf_rf;
static struct {
    uint8_t on;                                         /* a file open */
    uint8_t frz;                                        /* (0.4.20) track frz - 1's freeze (frz_task drives it) */
    uint8_t ch;                                         /* 2: stereo */
    char path[40];                                      /* its path (a cancelled take is deleted) */
    uint32_t frames;                                    /* written to the card */
} ur;
static uint32_t ur_chunk(void) { return UR_CHUNK_B / (2u * ur.ch); }   /* frames a write: 2,048 (stereo) */
/* the header sector: RIFF, fmt (16-bit, ch channels, 44.1 kHz), JUNK to byte 504, data (its length) at 504 */
static void ur_header(uint8_t *h, uint32_t frames, uint32_t ch)
{
    uint32_t i, data = frames * 2u * ch, bps = 44100u * 2u * ch;
    static const uint8_t FMT[24] = {'f', 'm', 't', ' ', 16, 0, 0, 0, 1, 0, 2, 0, 0x44, 0xAC, 0, 0,
                                    0x10, 0xB1, 0x02, 0, 4, 0, 16, 0};      /* PCM, 2 ch, 44100, 176400 B/s, 4, 16 */
    for (i = 0; i < 512u; i++) h[i] = 0;
    h[0] = 'R'; h[1] = 'I'; h[2] = 'F'; h[3] = 'F';
    for (i = 0; i < 4u; i++) h[4 + i] = (uint8_t)((data + 504u) >> (8u * i));   /* the file's length less 8 */
    h[8] = 'W'; h[9] = 'A'; h[10] = 'V'; h[11] = 'E';
    for (i = 0; i < 24u; i++) h[12 + i] = FMT[i];
    h[22] = (uint8_t)ch;                                /* (0.4.17) channels, bytes a second, bytes a frame */
    for (i = 0; i < 4u; i++) h[28 + i] = (uint8_t)(bps >> (8u * i));
    h[32] = (uint8_t)(2u * ch);
    h[36] = 'J'; h[37] = 'U'; h[38] = 'N'; h[39] = 'K';
    h[40] = 460u & 0xFFu; h[41] = 460u >> 8;            /* 44 + 460 = 504 */
    h[504] = 'd'; h[505] = 'a'; h[506] = 't'; h[507] = 'a';
    for (i = 0; i < 4u; i++) h[508 + i] = (uint8_t)(data >> (8u * i));
}
/* the header with the length so far, then f_sync: what was recorded stays if the card is pulled */
static FRESULT ur_sync(void)
{
    UINT bw;
    FRESULT r;
    FSIZE_t at = f_tell(&uf_rf);
    ur_header(uh_sec, ur.frames, ur.ch);
    r = f_lseek(&uf_rf, 0);
    if (r == FR_OK) r = f_write(&uf_rf, uh_sec, 512, &bw);
    if (r == FR_OK && bw != 512u) r = FR_DENIED;
    if (r == FR_OK) r = f_lseek(&uf_rf, at);
    if (r == FR_OK) r = f_sync(&uf_rf);
    return r;
}
/* n frames from the ring (from where the main loop is: whole chunks never wrap) to the card */
static FRESULT ur_write(uint32_t n)
{
    UINT bw;
    FRESULT r;
    uint32_t at = cr.r % cr.cap, bytes = n * 2u * ur.ch;
    r = f_write(&uf_rf, cr.buf + at * ur.ch, bytes, &bw);
    if (r == FR_OK && bw != bytes) r = FR_DENIED;       /* (the card full) */
    if (r != FR_OK) return r;
    ur.frames += n;
    RING_PUBLISH();
    cr.r = cr.r + n;
    return FR_OK;
}
/* ---- (0.4.20) frozen tracks: the card side (spec 0.9, 10.6; the audio side: card_freeze.c; the state: usb_host.c
 * frz[]) ----
 * The MIXER page's KNOB 3 right asks for a freeze (frz[k].want +1). frz_task, each pass of the main loop:
 *   FZ_CARD   the port made a host (usbh_want: it stays one while a track is frozen or freezing) and the drive found
 *             (8 s at most: "Card?");
 *   (begin)   the take's file made, /ORANGECAT/T<k+1>_<nnn>.WAV (the first free number), its header sector written,
 *             the cycle counted (frz_cycle), the recorder's ring set to slots 2 and 3, the track's stem kept (mix.c):
 *             FZ_ARMED, and the audio side takes it from there;
 *   FZ_ARMED, FZ_REC, FZ_POST  the ring written out, 8 KB a write (ur_write);
 *   FZ_SAVE   the rest written; the frames past the cycle's end blended into its start (a linear crossfade over up
 *             to FZ_X frames: the end now runs on into the start); the file cut to the cycle and its header set; then
 *             loaded into a free player slot (looping, its ring half filled) and armed: FZ_ON. The audio side brings
 *             it in at the track's next cycle.
 *   FZ_FAIL   (the sequencer stopped mid-take) the file deleted: "STOP".
 * KNOB 3 left: a frozen track back to its engine (its take stays on the card), or a freeze cancelled (its file
 * deleted). A new project (pattern_epoch) or the track removed unfreezes it. Not kept with the project yet (spec
 * 10.6: saving, resampling and undo come next). */
static void frz_say(uint32_t k, const char *why) { str_cpy(frz[k].why, why, sizeof frz[k].why); }
/* track k's cycle in passes of its phrase: what PLAY plays (lists.mode) until it comes round */
static uint32_t frz_cycle(uint32_t k)
{
    uint32_t f, l, e, n = 0, r, r0, r1, c;
    k %= NTRK;
    if (lists.mode == OCL_CHAIN) {                      /* its chain: each step's REPS */
        c = lists.tchain[k] % OCL_NCHAIN;
        ocl_chain_span(c, &f, &l);
        if (f == OCL_NENT) return 1u;
        for (e = f; e <= l; e++) n += (lists.chain[c][e][1] & 15u) + 1u;
        return n ? n : 1u;
    }
    if (lists.mode == OCL_SONG) {                       /* its block of song rows: each row's chain */
        r = lists.row[k];
        if (r >= OCL_NROW || !lists.song[r][k]) return 1u;
        for (r0 = r; r0 > 0u && lists.song[r0 - 1u][k]; r0--)
            ;
        for (r1 = r; r1 + 1u < OCL_NROW && lists.song[r1 + 1u][k]; r1++)
            ;
        for (r = r0; r <= r1; r++) {
            c = (lists.song[r][k] - 1u) % OCL_NCHAIN;
            ocl_chain_span(c, &f, &l);
            if (f == OCL_NENT) continue;
            for (e = f; e <= l; e++) n += (lists.chain[c][e][1] & 15u) + 1u;
        }
        return n ? n : 1u;
    }
    return 1u;                                          /* (the phrase mode: its phrase) */
}
static void frz_path(char *p, uint32_t k, uint32_t take)   /* "/ORANGECAT/T1_003.WAV" */
{
    char *e = uh_cat(p, "/ORANGECAT/T");
    e = uh_dec(e, k + 1u);
    e = uh_cat(e, take < 10u ? "_00" : take < 100u ? "_0" : "_");
    uh_cat(uh_dec(e, take), ".WAV");
}
/* the recorder let go (its file closed; deleted when drop) */
static void frz_rec_end(int drop)
{
    if (!ur.on || !ur.frz) return;
    f_close(&uf_rf);
    if (drop) f_unlink(ur.path);
    ur.on = 0;
    ur.frz = 0;
    CR_STEM_TRK(0xFFu);
}
/* a freeze cancelled: its file gone, the track as it was */
static void frz_cancel(uint32_t k, const char *why)
{
    frz[k].st = FZ_OFF;                                 /* (first: the audio side lets go at once) */
    if (ur.frz == k + 1u) frz_rec_end(1);
    frz_say(k, why ? why : "");
}
/* a frozen track back to its engine: its take stopped, its slot freed */
static void frz_unfreeze(uint32_t k, const char *why)
{
    uint32_t s = frz[k].slot % CP_N;
    frz[k].st = FZ_OFF;
    cp[s].on = cp[s].armed = 0;
    mix_frozen &= (uint8_t)~(1u << k);
    if (cs.st[s].run) {
        cs_claim();
        cs.st[s].run = 0;
        cs_update_run();
        cs_release();
    }
    cp[s].trk = 0;
    panic_req |= (uint8_t)(1u << k);                    /* (its engine's notes since: let go) */
    frz_say(k, why ? why : "");
}
static void frz_drop_all(const char *why)              /* the card gone or failed: every track back to its engine */
{
    uint32_t k;
    for (k = 0; k < 4u; k++) {
        if (frz[k].st == FZ_ON) frz_unfreeze(k, why);
        else if (frz[k].st != FZ_OFF) frz_cancel(k, why);
    }
}
/* the take's file and the recorder made ready; the audio side armed */
static void frz_begin(uint32_t k)
{
    char p[40];
    FILINFO fi;
    UINT bw;
    FRESULT r;
    uint32_t i, s, need = 176400u;
    if (ur.on) { frz_cancel(k, "Busy"); return; }
    if (k >= trk_n()) { frz_cancel(k, "No trk"); return; }
    if (!trk[k].p[P_LEVEL]) { frz_cancel(k, "Lvl 0"); return; }
    for (s = 2; s < CP_N; s++)
        if (cs.st[s].run || cp[s].on) { frz_cancel(k, "Full"); return; }   /* (takes play in 2 or 3: no ring) */
    for (s = 0; s < CP_N; s++) if (cs.st[s].run) need += cs.st[s].bps;
    if (need > uf_budget) { frz_cancel(k, "Full"); return; }
    r = uf_mount();
    if (r == FR_OK) { r = f_mkdir("/ORANGECAT"); if (r == FR_EXIST) r = FR_OK; }
    if (r != FR_OK) { frz_cancel(k, "Error"); return; }
    for (i = 1; i < 1000u; i++) {
        frz_path(p, k, i);
        r = f_stat(p, &fi);
        if (r == FR_NO_FILE) break;
        if (r != FR_OK) { frz_cancel(k, "Error"); return; }
    }
    if (i == 1000u) { frz_cancel(k, "Full"); return; }
    str_cpy(ur.path, p, sizeof ur.path);
    ur.ch = 2u;
    r = f_open(&uf_rf, p, FA_CREATE_NEW | FA_WRITE | FA_READ);   /* (read too: frz_finish reads its ends back) */
    if (r == FR_OK) {
        ur_header(uh_sec, 0, 2u);
        r = f_write(&uf_rf, uh_sec, 512, &bw);
        if (r == FR_OK && bw != 512u) r = FR_DENIED;
        if (r == FR_OK) r = f_sync(&uf_rf);
        if (r != FR_OK) f_close(&uf_rf);
    }
    if (r != FR_OK) { frz_cancel(k, r == FR_DENIED || r == FR_WRITE_PROTECTED ? "Full" : "Error"); return; }
    frz[k].take = (uint16_t)i;
    frz[k].passes = (uint16_t)frz_cycle(k);
    frz[k].lvl = trk[k].p[P_LEVEL];
    frz[k].epoch = pattern_epoch;
    ur.frames = 0;
    ur.on = 1;
    ur.frz = (uint8_t)(k + 1u);
    cr.buf = cp_pool + 2u * CP_SLOT;                    /* slots 2 and 3: 4,608 frames, 104 ms */
    cr.cap = CP_SLOT;
    cr.w = cr.r = cr.wi = 0;
    fzi[k].got = fzi[k].lost = 0;
    fzi[k].frames = 0;
    CR_STEM_TRK(k);
    RING_PUBLISH();
    frz[k].st = FZ_ARMED;
}
/* the take loaded into a free slot, looping, its ring half filled, armed: the audio side brings it in */
static void frz_load(uint32_t k)
{
    char p[40];
    struct uf_wi w;
    struct cs_one *o;
    FILINFO fi;
    uint32_t s, t0, want;
    FRESULT r;
    frz_path(p, k, frz[k].take);
    for (s = 0; s < CP_N && (cs.st[s].run || cp[s].on); s++)
        ;
    if (s == CP_N) { frz_cancel(k, "Full"); return; }
    r = f_stat(p, &fi);
    if (r == FR_OK) r = f_open(&uf_pf, p, FA_READ);
    if (r != FR_OK) { frz_cancel(k, "Error"); return; }
    o = &cs.st[s];
    if (!uf_wav_scan(&uf_pf, (uint32_t)fi.fsize, &w) || !uf_extents(&uf_pf, (uint32_t)fi.fsize, o)) {
        f_close(&uf_pf);
        frz_cancel(k, "Error");
        return;
    }
    f_close(&uf_pf);
    cp_setup(s, 2u, 44100u);
    o->bytes = 2u;
    o->bpf = 4u;
    o->bps = 176400u;
    o->right = 2u;
    o->ch = 2u;
    o->data0 = w.data0;
    o->data = w.data;
    o->pos = 0;
    o->nc = 0;                                          /* (it loops: the file is one cycle) */
    cp[s].trk = (uint8_t)(k + 1u);
    if (!cs.run) {                                      /* (the engine from fresh) */
        cs.err = 0;
        cs.hold = 0;
        cs.state = CS_IDLE;
    }
    sie_wr(S_FADDR, usbh.addr);
    o->run = 1;
    cs_update_run();
    want = w.data / 4u < cp[s].cap / 2u ? w.data / 4u : cp[s].cap / 2u;
    for (t0 = fm1_ms; cp[s].w < want && !cs.err && fm1_ms - t0 < 1000u;)
        CS_WAIT();
    frz[k].slot = (uint8_t)s;
    if (cs.err || cp[s].w < want) { frz[k].st = FZ_ON; frz_unfreeze(k, "Slow"); return; }
    cp[s].armed = 1;
    frz[k].st = FZ_ON;
    frz_say(k, "");
}
/* the take finished: the rest out, its end blended into its start, cut to the cycle, closed, loaded */
static void frz_finish(uint32_t k)
{
    FRESULT r = FR_OK;
    UINT got;
    uint32_t n, L = fzi[k].frames, P, X, i;
    while (r == FR_OK && (n = cr.w - cr.r) != 0u) {     /* the rest of the ring */
        uint32_t at = cr.r % cr.cap;
        if (n > cr.cap - at) n = cr.cap - at;
        if (n > ur_chunk()) n = ur_chunk();
        r = ur_write(n);
    }
    if (r != FR_OK) { frz_cancel(k, "Error"); return; }
    if (fzi[k].lost) { frz_cancel(k, "Slow"); return; }
    if (L < 256u || ur.frames < L) { frz_cancel(k, "Short"); return; }
    P = ur.frames - L;
    X = P < FZ_X ? P : FZ_X;
    if (X > L / 2u) X = L / 2u;
    if (X >= 32u) {                                     /* the end's continuation faded out over the start's fade in */
        int16_t *a = cr.buf, *b = cr.buf + 2u * FZ_X;   /* (the ring, drained: its first 16 KB) */
        r = f_lseek(&uf_rf, 512u + L * 4u);
        if (r == FR_OK) r = f_read(&uf_rf, b, X * 4u, &got);
        if (r == FR_OK && got != X * 4u) r = FR_INT_ERR;
        if (r == FR_OK) r = f_lseek(&uf_rf, 512u);
        if (r == FR_OK) r = f_read(&uf_rf, a, X * 4u, &got);
        if (r == FR_OK && got != X * 4u) r = FR_INT_ERR;
        if (r == FR_OK) {
            for (i = 0; i < 2u * X; i++) {
                int32_t j = (int32_t)(i / 2u);
                a[i] = (int16_t)(((int32_t)a[i] * j + (int32_t)b[i] * ((int32_t)X - j)) / (int32_t)X);
            }
            r = f_lseek(&uf_rf, 512u);
        }
        if (r == FR_OK) r = f_write(&uf_rf, a, X * 4u, &got);
        if (r == FR_OK && got != X * 4u) r = FR_DENIED;
    }
    if (r == FR_OK) r = f_lseek(&uf_rf, 512u + L * 4u);
    if (r == FR_OK) r = f_truncate(&uf_rf);              /* (the cycle, no more) */
    ur.frames = L;
    if (r == FR_OK) r = ur_sync();
    if (r != FR_OK) { frz_cancel(k, "Error"); return; }
    frz_rec_end(0);
    frz_load(k);
}
/* each pass of the main loop: the knob's wishes, the card woken, the take written, finished, loaded */
static void frz_task(void)
{
    uint32_t k;
    if (frz_busy() && !usbh.want) usbh_want(1);         /* (the port a host while a track is frozen or freezing) */
    for (k = 0; k < 4u; k++) {
        int8_t w = frz[k].want;
        frz[k].want = 0;
        if (w > 0 && frz[k].st == FZ_OFF) {
            frz_say(k, "");
            frz[k].st = FZ_CARD;
            frz[k].t0 = fm1_ms;
        } else if (w < 0) {
            if (frz[k].st == FZ_ON) frz_unfreeze(k, 0);
            else if (frz[k].st != FZ_OFF) frz_cancel(k, 0);
            else frz_say(k, "");
        }
        switch (frz[k].st) {
        case FZ_CARD:
            if (usbh.drive) frz_begin(k);
            else if (fm1_ms - frz[k].t0 > 8000u) frz_cancel(k, "Card?");
            break;
        case FZ_ARMED:
        case FZ_REC:
        case FZ_POST: {
            FRESULT r = FR_OK;
            uint32_t i;
            if (ur.frz != k + 1u) { frz_cancel(k, "Busy"); break; }
            if (fzi[k].lost) { frz_cancel(k, "Slow"); break; }
            for (i = 0; i < 5u && r == FR_OK && cr.w - cr.r >= ur_chunk(); i++) {
                uint32_t at = cr.r % cr.cap, n = ur_chunk();
                if (n > cr.cap - at) n = cr.cap - at;   /* (the ring, 4,608 frames, is not whole chunks: to its end,
                                                         * then on from its start; 512 frames still whole sectors) */
                r = ur_write(n);
            }
            if (r != FR_OK) frz_cancel(k, "Error");
            break;
        }
        case FZ_SAVE:
            frz_finish(k);
            break;
        case FZ_FAIL:
            frz_cancel(k, "Stop");
            break;
        case FZ_ON:
            if (frz[k].epoch != pattern_epoch || k >= trk_n()) frz_unfreeze(k, 0);   /* (a new project) */
            break;
        default:
            break;
        }
    }
}
/* main loop (main.c), each pass: the card engine's turn, frozen tracks, the port, takes the sequencer stopped */
static void usbf_task(void)
{
    uint32_t s;
    cs_poll();                                          /* (0.4.14) the main loop's turn at the card (every pass) */
    frz_task();                                         /* (0.4.20) frozen tracks: before usbh_task (it keeps the port) */
    usbh_task();
    for (s = 0; s < CP_N; s++)                          /* (0.4.17) takes the sequencer stopped: back to their start */
        if (cp[s].ended && cs.st[s].run) uf_rearm(s);
    if (cs.run && !usbh.drive) uf_play_stop_all();      /* (0.4.11) the drive gone: frozen tracks back to their engines */
    if (cs.err) uf_play_stop_all();                     /* (0.4.12) the engine stopped on an error */
    if (!usbh.drive && uf_mounted) {                    /* unplugged, or no longer a host: forget the volume */
        f_unmount("");
        uf_mounted = 0;
    }
}
