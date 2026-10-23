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
static void frz_unload(uint32_t k, uint32_t why);      /* (0.5.6) below */
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
        if (c->trk && c->trk <= 4u) {                   /* (0.5.6: the phrase plays its notes; not tried again until
                                                         * the track moves on) */
            fzt[c->trk - 1u].fail = fzt[c->trk - 1u].phr;
            frz_unload(c->trk - 1u, FZW_SLOW);
        }
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
    char path[64];                                      /* its path (a cancelled take is deleted; 0.6.1: 64, a folder) */
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
/* ---- (0.4.20) frozen tracks, (0.5.6) frozen phrases: the card side (spec 0.9, 10.6; the audio side and the table
 * of frozen phrases: card_freeze.c; a freeze being made: usb_host.c frz[]) ----
 * The MIXER page's KNOB 3 right asks to freeze the phrase the track is on (frz[k].want +1). frz_task, each pass of
 * the main loop:
 *   (ask)     the phrase found: stopped, the track's steps kept to the pool first (ocl_keep: a phrase number, the
 *             pool's copy the same as what plays); playing, a phrase with no number or with changes not kept yet
 *             cannot be frozen ("Save?": stop first);
 *   FZ_CARD   the port made a host (usbh_want: it stays one while a frozen phrase is in a track's banks or a freeze is
 *             being made) and the drive found (8 s at most: "Card?");
 *   (begin)   the take's file made, /ORANGECAT/T<k+1>_<nnn>.WAV (the first free number), its header sector written,
 *             the recorder's ring set to slots 2 and 3, the track's stem kept (mix.c): FZ_ARMED, and the audio side
 *             takes it from there (it records the next pass of that phrase, one pass after PLAY at the soonest);
 *   FZ_ARMED, FZ_REC, FZ_POST  the ring written out, 8 KB a write (ur_write);
 *   FZ_SAVE   the rest written; the frames past the pass's end blended into its start (a linear crossfade over up
 *             to FZ_X frames: the end now runs on into the start); the file cut to the pass and its header set; the
 *             phrase marked frozen (frzp[]: the take, its track, LEVEL, tempo, length, the phrase's CRC), kept in
 *             the record store the next time the sequencer is stopped (frz_table_save);
 *   FZ_FAIL   (the sequencer stopped mid-take) the file deleted: "Stop".
 * KNOB 3 left: the phrase the track is on unfrozen (its take stays on the card), or a freeze cancelled (its file
 * deleted).
 *
 * The players (frz_manage, each pass): each track has one player slot for its takes. The phrase the track is on, or
 * the one queued next, frozen: its take loaded into the track's slot (a free one), looping, its ring half filled,
 * armed; the audio side starts it at that phrase's next pass. A take is loaded only while its slot is not playing
 * (a take playing stops at the next pass of another phrase, then the next one loads: two different frozen phrases
 * back to back, the second plays its notes for its first pass). Not loaded, the phrase plays its notes, and the
 * page says why: Card? (no card), Full (no free slot, or the card too slow for one more stream), Tempo (frozen at
 * another tempo), Slow (the card did not keep up), Error (the file unreadable or gone). */
static void frz_say(uint32_t k, const char *why) { str_cpy(frz[k].why, why, sizeof frz[k].why); }
/* ---- (0.6.1) take folders (Jeremiah, October 22, 2026: "the wav files should be better organized on the SD card,
 * perhaps in folders named after the project ... when they save the project, it will also need to rename the
 * folder") ----
 * A take is made in its project's folder: /ORANGECAT/<the project's name>/, or /ORANGECAT/UNTITLED/ while the music has
 * no name (a name's characters a card cannot hold become "_"). Each frozen phrase keeps its file's folder (dir, an
 * index into frzd: the folders in use, 16 at most) and the folder it belongs in (want). The music given a name
 * (usb_files.c frz_named, from project.c's save and rename) changes want; the files move when the card is in and the
 * sequencer stopped (frz_reconcile, card_browse.c), one move a pass of the main loop, and dir follows each move. Until
 * then a take plays from where it is: nothing is lost if the card is out when the project is named, or the power goes
 * in between (both kept in the record store, with frzp).
 *   - A project renamed (its own slot, or saved again to the slot it came from under a new name): its folder renamed
 *     whole (mv_from, mv_to), so the takes no phrase names any more go with it. Its name taken already by another
 *     folder: each of its phrases' takes moved in one by one (a name already there: the next free T<n>_<nnn>).
 *   - Music with no name saved with one (or saved as a copy under a new name): the takes in UNTITLED of the phrases
 *     that project names move to its folder. Other projects' folders are not touched (a phrase shared with another
 *     project keeps its take where it was made).
 *   - Takes of 0.4.20 .. 0.6.0 stay in /ORANGECAT itself (folder 0) unless their project is named or renamed. */
#define FZD_N 16u
#define FZD_LEN 32u                                     /* "ORANGECAT/" and a name of 12: 22, and room */
#define FZD_MV 4u
typedef struct {
    char d[FZD_N][FZD_LEN];                             /* folders from the card's root, no slash at either end; d[0]
                                                         * "ORANGECAT"; "" a free entry */
    uint8_t mv_from[FZD_MV], mv_to[FZD_MV];             /* whole folders still to rename: FZD_N none */
} frzd_t;
static frzd_t frzd;
#ifndef FRZ_MUSIC_NAME
#define FRZ_MUSIC_NAME() proj_name                      /* (project.c: the music's name now; "" none) */
#endif
#ifndef FRZ_SLOT_NAMES
#define FRZ_SLOT_NAMES(s, i) ((ocj.names[(s) & 3u][(i) >> 5] >> ((i) & 31u)) & 1u)   /* slot s's project names pool
                                                         * phrase i + 1 (oc_project.c) */
#endif
static void frzd_init(void)
{
    memset(&frzd, 0, sizeof frzd);
    str_cpy(frzd.d[0], "ORANGECAT", FZD_LEN);
    memset(frzd.mv_from, FZD_N, sizeof frzd.mv_from);
    memset(frzd.mv_to, FZD_N, sizeof frzd.mv_to);
}
/* the folder for a project named name ("" or 0: none): "ORANGECAT/<name>", "ORANGECAT/UNTITLED"; d: FZD_LEN */
static void frz_dir_for(char *d, const char *name)
{
    char *e = uh_cat(d, "ORANGECAT/");
    uint32_t i = 0, n = 0;
    while (name && name[i] == ' ') i++;                 /* (no leading spaces) */
    for (; name && name[i] && n < 12u; i++) {
        char c = name[i];
        if (c < 32 || c > 126 || c == '\\' || c == '/' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' ||
            c == '>' || c == '|')
            c = '_';                                    /* (what a FAT name cannot hold) */
        e[n++] = c;
    }
    while (n && (e[n - 1u] == ' ' || e[n - 1u] == '.')) n--;   /* (nor trailing spaces and dots) */
    e[n] = 0;
    if (!n) uh_cat(e, "UNTITLED");
}
static int frz_same_dir(const char *a, const char *b)   /* the same folder on a FAT card: case aside */
{
    for (; *a && *b; a++, b++) {
        char x = (char)(*a >= 'a' && *a <= 'z' ? *a - 32 : *a), y = (char)(*b >= 'a' && *b <= 'z' ? *b - 32 : *b);
        if (x != y) return 0;
    }
    return *a == *b;
}
static uint32_t frzd_find(const char *d)                /* its index, FZD_N none */
{
    for (uint32_t i = 0; i < FZD_N; i++) if (frzd.d[i][0] && frz_same_dir(frzd.d[i], d)) return i;
    return FZD_N;
}
static int frzd_used(uint32_t i)                        /* a phrase, a freeze being made or a rename still names it */
{
    uint32_t k;
    if (i == 0u) return 1;
    for (k = 0; k < FZP_N; k++) if (frzp[k].take && (frzp[k].dir == i || frzp[k].want == i)) return 1;
    for (k = 0; k < 4u; k++) if (frz[k].st != FZ_OFF && frz[k].dir == i) return 1;
    for (k = 0; k < FZD_MV; k++) if (frzd.mv_from[k] == i || frzd.mv_to[k] == i) return 1;
    return 0;
}
static uint32_t frzd_get(const char *d)                 /* its index, made when new (unused ones freed first); FZD_N full */
{
    uint32_t i = frzd_find(d);
    if (i < FZD_N) return i;
    for (i = 1; i < FZD_N; i++)
        if (frzd.d[i][0] && !frzd_used(i)) frzd.d[i][0] = 0;
    for (i = 1; i < FZD_N; i++)
        if (!frzd.d[i][0]) {
            str_cpy(frzd.d[i], d, FZD_LEN);
            return i;
        }
    return FZD_N;
}
/* the music's folder now: its index (FZD_N: the table full; then the takes go to /ORANGECAT itself, folder 0) */
static uint32_t frz_music_dir(void)
{
    char d[FZD_LEN];
    uint32_t i;
    frz_dir_for(d, FRZ_MUSIC_NAME());
    i = frzd_get(d);
    return i < FZD_N ? i : 0u;
}
static void frz_take_name(char *n, uint32_t k, uint32_t take)   /* "T1_003.WAV" */
{
    char *e = uh_cat(n, "T");
    e = uh_dec(e, k + 1u);
    e = uh_cat(e, take < 10u ? "_00" : take < 100u ? "_0" : "_");
    uh_cat(uh_dec(e, take), ".WAV");
}
static char *frz_dir_path(char *p, uint32_t dir)        /* "/ORANGECAT/JAM" (dir out of range: "/ORANGECAT") */
{
    char *e = uh_cat(p, "/");
    return uh_cat(e, dir < FZD_N && frzd.d[dir][0] ? frzd.d[dir] : "ORANGECAT");
}
static void frz_path(char *p, uint32_t dir, const char *name)   /* "/ORANGECAT/JAM/T1_003.WAV"; p: 64 bytes */
{
    uh_cat(uh_cat(frz_dir_path(p, dir), "/"), name);
}
static void frz_entry_path(char *p, const frzp_t *f) { frz_path(p, f->dir, f->name); }
/* the folders on the way made (FR_EXIST is fine) */
static FRESULT frz_mkdirs(uint32_t dir)
{
    char p[FZD_LEN + 2u], *e;
    FRESULT r;
    frz_dir_path(p, dir);
    for (e = p + 1; ; e++) {                            /* (each "/" after the first: the folder before it) */
        if (*e == '/' || !*e) {
            char c = *e;
            *e = 0;
            r = f_mkdir(p);
            *e = c;
            if (r != FR_OK && r != FR_EXIST) return r;
            if (!c) return FR_OK;
        }
    }
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
/* (0.5.6) track k's player emptied: its take stopped, its slot freed; why: FZW_* for the page while the track is
 * on a frozen phrase (its notes play) */
static void frz_unload(uint32_t k, uint32_t why)
{
    uint32_t s = fzt[k].slot;
    if (s < CP_N) {
        cp[s].on = cp[s].armed = 0;
        if (cs.st[s].run) {
            cs_claim();
            cs.st[s].run = 0;
            cs_update_run();
            cs_release();
        }
        cp[s].trk = 0;
    }
    mix_frozen &= (uint8_t)~(1u << k);
    fzt[k].slot = CP_N;
    fzt[k].phr = 0;
    fzt[k].why = (uint8_t)why;
}
static void frz_drop_all(const char *why)              /* the card gone or failed: freezes cancelled, players emptied */
{
    uint32_t k, w = why && why[0] == 'E' ? FZW_ERROR : FZW_CARD;
    for (k = 0; k < 4u; k++) {
        if (frz[k].st != FZ_OFF) frz_cancel(k, why);
        if (fzt[k].slot < CP_N) frz_unload(k, w);
        fzt[k].fail = 0;                                /* (the card back: every take tried again) */
    }
}
/* (0.5.6) phrase id unfrozen (KNOB 3 left; its take stays on the card): a track playing its take goes on with its
 * notes at once */
static void frz_unfreeze_phrase(uint32_t id)
{
    uint32_t k;
    if (!id || id > FZP_N || !frzp[id - 1u].take) return;
    frzp[id - 1u].take = 0;
    frzp_dirty = 1;
    for (k = 0; k < 4u; k++)
        if (fzt[k].phr == id) {
            if (fzt[k].slot < CP_N && cp[fzt[k].slot].on) fzt[k].kill = 1;   /* (the audio side stops it, faded) */
            else frz_unload(k, FZW_NONE);
        }
}
/* the take's file and the recorder made ready; the audio side armed */
static void frz_begin(uint32_t k)
{
    char p[64], nm[14];
    FILINFO fi;
    UINT bw;
    FRESULT r;
    uint32_t i, s, need = 176400u, dir;
    if (ur.on) { frz_cancel(k, "Busy"); return; }
    if (k >= trk_n()) { frz_cancel(k, "No trk"); return; }
    if (!trk[k].p[P_LEVEL]) { frz_cancel(k, "Lvl 0"); return; }
    for (s = 2; s < CP_N; s++)                          /* (the ring is slots 2 and 3: a take there, unloaded if it is
                                                         * not playing; one playing: no ring) */
        if (cs.st[s].run || cp[s].on) {
            uint32_t j = cp[s].trk ? cp[s].trk - 1u : 4u;
            if (j < 4u && fzt[j].slot == s && !cp[s].on) frz_unload(j, FZW_NONE);
            else { frz_cancel(k, "Full"); return; }
        }
    for (s = 0; s < CP_N; s++) if (cs.st[s].run) need += cs.st[s].bps;
    if (need > uf_budget) { frz_cancel(k, "Full"); return; }
    r = uf_mount();
    dir = frz_music_dir();                              /* (0.6.1) the project's folder: /ORANGECAT/<its name> */
    if (r == FR_OK) r = frz_mkdirs(dir);
    if (r != FR_OK) { frz_cancel(k, "Error"); return; }
    for (i = 1; i < 1000u; i++) {
        frz_take_name(nm, k, i);
        frz_path(p, dir, nm);
        r = f_stat(p, &fi);
        if (r == FR_NO_FILE) break;
        if (r != FR_OK) { frz_cancel(k, "Error"); return; }
    }
    if (i == 1000u) { frz_cancel(k, "Full"); return; }
    if (!card_pool_take(12u)) { frz_cancel(k, "Busy"); return; }   /* (0.6.0 rev 2: slots 2 and 3 back from the
                                                         * effects) */
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
    frz[k].dir = (uint8_t)dir;
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
/* (0.5.6) phrase id's take loaded into track k's player (its slot, else a free one; not 2 or 3 while the recorder
 * has them), looping, its ring half filled, armed: the audio side starts it at the phrase's next pass. 0: not
 * (fzt[k].why says why) */
static uint8_t frz_take_late;                           /* (0.6.0 rev 2) frz_load: the audio was late giving the slot back */
static int frz_load(uint32_t k, uint32_t id)
{
    char p[64];
    struct uf_wi w;
    struct cs_one *o;
    FILINFO fi;
    const frzp_t *f = &frzp[id - 1u];
    uint32_t s, t0, want, need = 176400u;
    FRESULT r;
    if (fzt[k].slot < CP_N) frz_unload(k, FZW_NONE);    /* (the take before: not playing, the caller saw to it) */
    for (s = 0; s < CP_N; s++) {
        if (cs.st[s].run || cp[s].on || cp[s].armed) continue;
        if (ur.on && s >= 2u) continue;                 /* (the recorder's ring) */
        break;
    }
    if (s == CP_N) { fzt[k].why = FZW_FULL; return 0; }
    for (t0 = 0; t0 < CP_N; t0++) if (cs.st[t0].run) need += cs.st[t0].bps;
    if (need > uf_budget) { fzt[k].why = FZW_FULL; return 0; }
    r = uf_mount();
    frz_entry_path(p, f);                               /* (0.6.1: where its file is now) */
    if (r == FR_OK) r = f_stat(p, &fi);
    if (r == FR_OK) r = f_open(&uf_pf, p, FA_READ);
    if (r != FR_OK) { fzt[k].why = FZW_ERROR; return 0; }
    o = &cs.st[s];
    if (!uf_wav_scan(&uf_pf, (uint32_t)fi.fsize, &w) || !uf_extents(&uf_pf, (uint32_t)fi.fsize, o) ||
        w.data / 4u != f->frames) {                     /* (not the take it was: another file by that name) */
        f_close(&uf_pf);
        fzt[k].why = FZW_ERROR;
        return 0;
    }
    f_close(&uf_pf);
    if (!card_pool_take(1u << s)) { fzt[k].why = FZW_FULL; frz_take_late = 1; return 0; }   /* (0.6.0 rev 2: the
                                                         * slot back from the effects; late: tried again, not latched) */
    cp_setup(s, 2u, 44100u);
    o->bytes = 2u;
    o->bpf = 4u;
    o->bps = 176400u;
    o->right = 2u;
    o->ch = 2u;
    o->data0 = w.data0;
    o->data = w.data;
    o->pos = 0;
    o->nc = 0;                                          /* (it loops: the file is one pass) */
    cp[s].trk = (uint8_t)(k + 1u);
    if (!cs.run) {                                      /* (the engine from fresh) */
        cs.err = 0;
        cs.hold = 0;
        cs.state = CS_IDLE;
    }
    sie_wr(S_FADDR, usbh.addr);
    o->run = 1;
    cs_update_run();
    fzt[k].slot = (uint8_t)s;
    want = w.data / 4u < cp[s].cap / 2u ? w.data / 4u : cp[s].cap / 2u;
    for (t0 = fm1_ms; cp[s].w < want && !cs.err && fm1_ms - t0 < 1000u;)
        CS_WAIT();
    if (cs.err || cp[s].w < want) { frz_unload(k, FZW_SLOW); return 0; }
    fzt[k].phr = (uint8_t)id;
    fzt[k].why = FZW_NONE;
    cp[s].armed = 1;
    return 1;
}
/* the take finished: the rest out, its end blended into its start, cut to the pass, closed; the phrase frozen */
static void frz_finish(uint32_t k)
{
    FRESULT r = FR_OK;
    UINT got;
    uint32_t n, L = fzi[k].frames, P, X, i, id = frz[k].phrase;
    while (r == FR_OK && (n = cr.w - cr.r) != 0u) {     /* the rest of the ring */
        uint32_t at = cr.r % cr.cap;
        if (n > cr.cap - at) n = cr.cap - at;
        if (n > ur_chunk()) n = ur_chunk();
        r = ur_write(n);
    }
    if (r != FR_OK) { frz_cancel(k, "Error"); return; }
    if (fzi[k].lost) { frz_cancel(k, "Slow"); return; }
    if (L < 256u || ur.frames < L) { frz_cancel(k, "Short"); return; }
    if (!id || id > FZP_N) { frz_cancel(k, "Error"); return; }
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
    if (r == FR_OK) r = f_truncate(&uf_rf);              /* (the pass, no more) */
    ur.frames = L;
    if (r == FR_OK) r = ur_sync();
    if (r != FR_OK) { frz_cancel(k, "Error"); return; }
    frz_rec_end(0);
    frzp[id - 1u].take = frz[k].take;                   /* (0.5.6) the phrase frozen: its take, and what it was made at */
    frzp[id - 1u].trk = (uint8_t)k;
    frzp[id - 1u].ver = (uint8_t)FZP_VER;
    frzp[id - 1u].dir = frzp[id - 1u].want = frz[k].dir;   /* (0.6.1) its folder and its file's name */
    frz_take_name(frzp[id - 1u].name, k, frz[k].take);
    frzp[id - 1u].lvl = frz[k].lvl;
    frzp[id - 1u].bpm = frz[k].bpm;
    frzp[id - 1u].frames = L;
    frzp[id - 1u].hash = frz[k].hash;
    frzp_dirty = 1;
    for (i = 0; i < 4u; i++)                            /* (a track holding an older take of it: loaded again) */
        if (fzt[i].phr == id && !(fzt[i].slot < CP_N && cp[fzt[i].slot].on)) frz_unload(i, FZW_NONE);
    frz[k].st = FZ_OFF;
    frz_say(k, "");
}

/* (0.5.6) the table of frozen phrases kept in the record store (OCS_FREEZE, one record), the sequencer stopped
 * (a flash write mutes the audio); read at boot. A record of another size, or an entry of another version, is not
 * read (those phrases play their notes; their takes stay on the card). */
#ifndef FRZ_STORE
#define FRZ_STORE 1
#endif
/* (0.6.1) the record: frzp (64 entries of 32 bytes), then frzd (the folders and the renames still to do). 0.5.6 ..
 * 0.6.0 wrote frzp alone at 16 bytes an entry (FZP_VER 1): read as their takes in /ORANGECAT itself */
#define FRZ_REC_V1 (FZP_N * 16u)
#define FRZ_REC (sizeof frzp + sizeof frzd)
typedef struct { ocs_src_t src; uint32_t at; } frz_rec_src_t;
static int frz_rec_get(ocs_src_t *s, uint8_t *dst, uint32_t n)
{
    frz_rec_src_t *r = (frz_rec_src_t *)s;
    while (n) {
        uint32_t k;
        if (r->at < sizeof frzp) {
            k = (uint32_t)sizeof frzp - r->at < n ? (uint32_t)sizeof frzp - r->at : n;
            memcpy(dst, (const uint8_t *)frzp + r->at, k);
        } else {
            uint32_t o = r->at - (uint32_t)sizeof frzp;
            if (o >= sizeof frzd) return -1;
            k = (uint32_t)sizeof frzd - o < n ? (uint32_t)sizeof frzd - o : n;
            memcpy(dst, (const uint8_t *)&frzd + o, k);
        }
        dst += k; n -= k; r->at += k;
    }
    return 0;
}
static void frz_table_save(void)
{
#if FRZ_STORE
    uint32_t i, any = 0;
    if (!frzp_dirty || song.playing || transport_busy()) return;
    frzp_dirty = 0;                                     /* (a failed write is not tried again at once: the next change) */
    for (i = 0; i < FZP_N; i++) any |= frzp[i].take;
    for (i = 0; i < FZD_MV; i++) any |= frzd.mv_from[i] < FZD_N;   /* (0.6.1: a rename still to do) */
    if (any) {
        frz_rec_src_t r = {{frz_rec_get}, 0};
        (void)ocs_write_src(OCS_FREEZE, 0, &r.src, (uint32_t)FRZ_REC);
    } else {
        (void)ocs_delete(OCS_FREEZE, 0);
    }
#endif
}
static int frz_name_ok(const char *n)                   /* a file's name as kept: 1 .. 13 characters, ended */
{
    uint32_t i;
    for (i = 0; i < 14u && n[i]; i++) if (n[i] < 32 || n[i] == '/' || n[i] == '\\') return 0;
    return i > 0u && i < 14u;
}
static void frz_named(const char *old, const char *nw, uint32_t slot, int rename);   /* (0.6.1) card_browse.c */
static void (*frz_named_hook)(const char *old, const char *nw, uint32_t slot, int rename);   /* (project.c's) */
static void frz_table_load(void)
{
    frz_named_hook = frz_named;                         /* (0.6.1) project.c tells it a project's new name */
    memset(frzp, 0, sizeof frzp);
    frzd_init();
#if FRZ_STORE
    uint32_t i;
    ocs_rec_t r;
    frz_pool_hash = ocp.hash;
    if (ocs_open(OCS_FREEZE, 0, &r) == 0) {
        if (r.total == FRZ_REC) {                       /* (0.6.1) */
            if (ocs_at(&r, 0, frzp, sizeof frzp) || ocs_at(&r, sizeof frzp, &frzd, sizeof frzd)) {
                memset(frzp, 0, sizeof frzp);
                frzd_init();
            }
        } else if (r.total == FRZ_REC_V1) {             /* (0.5.6 .. 0.6.0: converted, their takes in /ORANGECAT) */
            for (i = 0; i < FZP_N; i++) {
                uint8_t b[16];
                frzp_t *f = &frzp[i];
                if (ocs_at(&r, i * 16u, b, 16u)) { memset(frzp, 0, sizeof frzp); break; }
                memcpy(f, b, 16u);                      /* (the first 16 bytes are the same fields) */
                if (f->ver != 1u) { memset(f, 0, sizeof *f); continue; }
                f->ver = (uint8_t)FZP_VER;
                f->dir = f->want = 0;
                if (f->trk < 4u && f->take && f->take < 1000u) frz_take_name(f->name, f->trk, f->take);
            }
        }
    }
    if (!str_eq(frzd.d[0], "ORANGECAT")) frzd_init();   /* (the folders' table: each ended, renames in range) */
    for (i = 0; i < FZD_N; i++) {
        frzd.d[i][FZD_LEN - 1u] = 0;
        if (frzd.d[i][0] && (frzd.d[i][0] == '/' || frzd.d[i][str_len(frzd.d[i]) - 1u] == '/')) frzd.d[i][0] = 0;
    }
    for (i = 0; i < FZD_MV; i++)
        if (frzd.mv_from[i] >= FZD_N || frzd.mv_to[i] >= FZD_N || !frzd.d[frzd.mv_from[i]][0] || !frzd.d[frzd.mv_to[i]][0])
            frzd.mv_from[i] = frzd.mv_to[i] = FZD_N;
    for (i = 0; i < FZP_N; i++) {
        frzp_t *f = &frzp[i];
        if (f->ver != FZP_VER || f->trk >= 4u || f->take >= 1000u || f->frames < 256u || f->dir >= FZD_N ||
            f->want >= FZD_N || !frzd.d[f->dir][0] || !frzd.d[f->want][0] || !frz_name_ok(f->name))
            memset(f, 0, sizeof *f);
    }
#endif
    frzp_dirty = 0;
}

/* (0.5.6) each pass of the main loop: each track's player given the take of the frozen phrase it is on (or queued
 * next), or emptied; which tracks need the card (frz_need) */
static uint32_t frz_bank_phrase(uint32_t k, uint32_t b)
{
    uint32_t id = b < NPAT ? bank_pid[k][b] : 0u;
    return id <= FZP_N ? id : 0u;
}
static void frz_manage(void)
{
    uint32_t k, b, need = 0;
    for (k = 0; k < 4u && k < NTRK; k++) {
        uint32_t cur = frz_phrase_of(k), nxt = frz_bank_phrase(k, trk[k].pattern_next), want, s = fzt[k].slot;
        int playing = s < CP_N && (cp[s].on || fzt[k].kill);
        for (b = 0; b < NPAT; b++) if (frz_phrase_frozen(frz_bank_phrase(k, b))) need |= 1u << k;
        if (frz[k].st != FZ_OFF) need |= 1u << k;
        want = frz_phrase_frozen(cur) ? cur : frz_phrase_frozen(nxt) ? nxt : 0u;
        if (fzt[k].fail && fzt[k].fail != want) fzt[k].fail = 0;
        if (!want) {                                    /* nothing frozen to come: the player emptied once it stops */
            if (s < CP_N && !playing) frz_unload(k, FZW_NONE);
            else if (s >= CP_N) fzt[k].why = FZW_NONE;
            continue;
        }
        if (frzp[want - 1u].bpm != song.g[G_BPM]) {     /* another tempo: its notes (the take, if loaded, kept for
                                                         * the tempo coming back) */
            fzt[k].why = FZW_TEMPO;
            continue;
        }
        if (fzt[k].phr == want && s < CP_N) { fzt[k].why = FZW_NONE; continue; }   /* (loaded, ready) */
        if (playing) continue;                          /* (another take playing: loaded once it stops) */
        if (!usbh.drive) { if (s < CP_N) frz_unload(k, FZW_CARD); fzt[k].why = FZW_CARD; continue; }
        if (fzt[k].fail == want) continue;              /* (it failed: not again until the track moves on) */
        frz_take_late = 0;
        if (!frz_load(k, want) && !frz_take_late) fzt[k].fail = (uint8_t)want;
    }
    frz_need = (uint8_t)need;
    for (k = 0, need = 0; k < 4u && k < NTRK; k++) if (frz_track_frozen(k)) need |= 1u << k;
    seq_frz_lock = (uint8_t)need;                       /* (no recording into a frozen phrase: seq.c) */
}

/* (0.5.6) KNOB 3 right on track k: the phrase it is on to be frozen. 0: refused (frz[k].why says why) */
static int frz_ask(uint32_t k)
{
    uint32_t b = trk[k].pattern % NPAT, id;
    if (k >= trk_n()) { frz_say(k, "No trk"); return 0; }
    if (!song.playing && !transport_busy()) {           /* stopped: the steps kept to the pool first (a number) */
        int rc = ocl_keep(k, b);
        if (rc) { frz_say(k, rc == 4 ? "Full" : "Error"); return 0; }
    }
    id = frz_phrase_of(k);
    if (!id) { frz_say(k, "Save?"); return 0; }         /* (playing: no number yet; stop and ask again) */
    if (frz_phrase_frozen(id)) { frz_say(k, ""); return 0; }   /* (frozen already) */
    frz[k].hash = ocl_bank_hash(k, b);
    if (frz_pool_hash && frz_pool_hash[id - 1u] != frz[k].hash) { frz_say(k, "Save?"); return 0; }   /* (changes not
                                                         * kept yet: playing; the take would not match the phrase) */
    frz[k].phrase = (uint8_t)id;
    frz[k].lvl = trk[k].p[P_LEVEL];
    frz[k].bpm = song.g[G_BPM];
    return 1;
}
/* each pass of the main loop: the knob's wishes, the card woken, the take written, finished; the players */
static void frz_task(void)
{
    uint32_t k;
    for (k = 0; k < 4u; k++) {
        int8_t w = frz[k].want;
        frz[k].want = 0;
        if (w > 0 && frz[k].st == FZ_OFF) {
            if (frz_ask(k)) {
                frz_say(k, "");
                frz[k].st = FZ_CARD;
                frz[k].t0 = fm1_ms;
            }
        } else if (w < 0) {
            if (frz[k].st != FZ_OFF) frz_cancel(k, 0);
            else if (frz_track_frozen(k)) frz_unfreeze_phrase(frz_phrase_of(k));
            frz_say(k, "");
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
        default:
            break;
        }
    }
    frz_manage();
    frz_table_save();
    if (frz_busy() && !usbh.want) usbh_want(1);         /* (the port a host while a frozen phrase is in use or a freeze
                                                         * is being made) */
}
/* main loop (main.c), each pass: the card engine's turn, frozen tracks, the port, takes the sequencer stopped */
#include "card_browse.c"                                /* (0.6.1) take folders, their moves, the card's browser */

/* (0.6.0 rev 2) the card's slots in use (bit s: slot s): a stream, a take loaded there, the recorder and a freeze under
 * way (slots 2 and 3). The effects may borrow the others (resources.c res_lend_ok) */
static uint32_t uf_pool_busy(void)
{
    uint32_t k, m = 0;
    for (k = 0; k < CP_N; k++)
        if (cp[k].on || cp[k].armed || cs.st[k].run) m |= 1u << k;
    for (k = 0; k < 4u; k++) {
        if (fzt[k].slot < CP_N) m |= 1u << fzt[k].slot;
        if (frz[k].st != FZ_OFF) m |= 12u;
    }
    if (ur.on) m |= 12u;
    return m;
}
static void usbf_task(void)
{
    uint32_t s;
    cs_poll();                                          /* (0.4.14) the main loop's turn at the card (every pass) */
    frz_task();                                         /* (0.4.20) frozen tracks: before usbh_task (it keeps the port) */
    if (usbh.drive && frz_card_quiet() && frz_pending() && uf_mount() == FR_OK)   /* (0.6.1) a take toward its
                                                         * project's folder, one a pass */
        (void)frz_reconcile();
    fb_task();                                          /* (0.6.1) the card's browser */
    usbh_task();
    for (s = 0; s < CP_N; s++)                          /* (0.4.17) takes the sequencer stopped: back to their start */
        if (cp[s].ended && cs.st[s].run) uf_rearm(s);
    if (cs.run && !usbh.drive) uf_play_stop_all();      /* (0.4.11) the drive gone: frozen tracks back to their engines */
    if (cs.err) uf_play_stop_all();                     /* (0.4.12) the engine stopped on an error */
    if (!usbh.drive && uf_mounted) {                    /* unplugged, or no longer a host: forget the volume */
        f_unmount("");
        uf_mounted = 0;
    }
    res_lend_ok = (uint8_t)(~uf_pool_busy() & ((1u << CP_N) - 1u));   /* (0.6.0 rev 2) the slots not in use: lent */
}
