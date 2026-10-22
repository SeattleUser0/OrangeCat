/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat: files on the USB drive (build doc phase 2, step 2). After the USB drive test (usb_host.c) has read a
 * drive, OCT+ opens its files: folders and files listed, folders entered, a file picked shows what it is (a WAV:
 * its rate, bits, channels and length) and how fast it reads in 512-byte pieces (what a sample loader or a
 * player would do without a large buffer).
 *
 * The file system is FatFs (ChaN, R0.16 patch 2; firmware/src/fatfs, its own BSD-style licence, GPL compatible),
 * configured in fatfs/ffconf.h: reading only, long names, FAT12/16/32 and exFAT. Its disk layer is here: sectors
 * through usb_host.c's SCSI commands (READ(10)) to the drive the test found, its slot (LUN) and endpoints.
 *
 * Who does what: the page (ui_menu.c) only sets usbh.view and usbh.cmd; this file runs from the main loop
 * (usbf_task, main.c) and fills usbh's list (ent, count, path, info). So the UI never waits on the drive, and the
 * host tests of the UI run without one.
 *
 * Included by melodee.c after everything but main.c, since ff.c leaves its macros defined. The firmware has no C
 * library: fatfs/sys/string.h (on the firmware build's include path only) declares what ff.c uses; memset,
 * memcpy and memcmp are libc.c's, strchr and strlen are here, and so are __udivdi3 / __umoddi3 (the first build
 * failed to link without them). The host tests use their own C library (UF_HOST_LIBC).
 *
 * Revisions:
 *   0.4.10: first version.
 *   0.4.13: up to four WAVs at once (a slot each); OCT+ on a playing one stops it; they play on through the folders;
 *          the rows playing marked (usbh.ent[].play).
 *   0.4.12: the playing WAV read by the timer interrupt (card_play.c cs_*), not the main loop; FatFs's reads wait
 *          for the engine's command to end (cs_claim). A file in more than 32 runs of sectors does not play.
 *   0.4.11: a WAV picked plays from the card, looped, mixed in at the DAC (card_play.c), with the audio load, the
 *          card's share of the main loop and any gaps on the page. WAV chunks walked in the file (the samples can
 *          start past the first 512 bytes). Other files are still read and timed.
 *   0.4.14: the foot built in a buffer and cut to fit (0.4.13 wrote past info2 into the first row's name); three
 *          lines: the file picked, a message (held 2 s) or how many play with the loads, and the gaps (dropouts, not
 *          blocks) with the card's speed: "read" against "needed" KB/s. The main loop steps the card engine too
 *          (card_stream.c cs_poll).
 *   0.4.15: a WAV that would take the playing ones past what the card keeps up with (uf_budget, 532 KB/s: measured on
 *          the unit) does not start: "Too much for the card".
 *   0.4.16: the recorder (below): REC on the file list records what the FM-1 plays to a new WAV in the folder shown
 *          (OC001.WAV, OC002.WAV, ...), REC again stops it and the list shows the file, ready to play. FatFs writes
 *          now (ffconf.h), through disk_write (WRITE(10)).
 *   0.4.17: in time with the sequencer. A WAV picked while it is stopped waits ("READY" on its row) and plays once
 *          from its start when PLAY starts the sequencer; when the sequencer stops (or the WAV ends) it rewinds and
 *          waits again (uf_rearm). Picked while the sequencer plays, it loops at once, as before. REC while stopped
 *          arms a take that starts and stops with the sequencer; REC again before PLAY cancels it (the empty file
 *          deleted). The recorder records the mix or one track (usbh.src, KNOB 3 on the page): a track is a mono
 *          WAV of its own signal, before MASTER (a freeze).
 *   0.4.19: a track take is stereo (card_play.c, mix.c's stem): after its pan and own effects, before its sends.
 *   0.4.20: frozen tracks, the card side (frz_*, before usbf_task; the audio side: card_freeze.c). The page's
 *          players stop when the page is left, a frozen track's take plays on (cp[].trk); a card error or the drive
 *          gone stops both and frozen tracks go back to their engines. The recorder's ring is cr.buf (a freeze records
 *          into slots 2 and 3 while takes play in 0 and 1).
 *   0.4.18: what a WAV does no longer depends on whether the sequencer happens to run (0.4.17 on the unit: a WAV
 *          picked while it was stopped only waited, and PLAY brought the song in with it). OCT+ plays it now, by
 *          itself, looping; SEQ (UC_SYNC) ties it to the song: READY, then once from its start with each PLAY. The
 *          foot says how many play and how many wait ("1 play, 1 ready, CPU 25%").
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

/* ---- the list ---- */
static void uf_marks(void);                             /* (0.4.13) the WAV player's, below */
static FATFS uf_fs;
static uint8_t uf_mounted;
/* left out of the list: names starting with "." (macOS leaves "._name" and ".Spotlight-V100"), hidden or system */
static int uf_hide(const FILINFO *f) { return f->fname[0] == '.' || (f->fattrib & (AM_HID | AM_SYS)); }
static const char *uf_err(FRESULT r)
{
    return r == FR_NO_FILESYSTEM ? "No FAT32 or exFAT" : r == FR_DISK_ERR || r == FR_NOT_READY ? "Drive not reading" :
           r == FR_NO_PATH || r == FR_NO_FILE ? "Not found" : "File system error";
}
static void uf_fail(FRESULT r)
{
    str_cpy(usbh.info, uf_err(r), UH_LINE);
    usbh.info2[0] = 0;
    if (r == FR_DISK_ERR || r == FR_NOT_READY) usbh.drive = 0;   /* gone: OCT- then OCT+ looks again */
    usbh.gen++;
}
static FRESULT uf_mount(void)
{
    FRESULT r;
    if (uf_mounted) return FR_OK;
    r = f_mount(&uf_fs, "", 1);
    if (r == FR_OK) uf_mounted = 1;
    return r;
}
/* the n-th entry shown in the folder (hidden ones skipped) into *f; FR_NO_FILE past the end */
static FRESULT uf_nth(uint32_t n, FILINFO *f)
{
    DIR d;
    FRESULT r = f_opendir(&d, usbh.path[0] ? usbh.path : "/");
    uint32_t i = 0;
    if (r != FR_OK) return r;
    for (;;) {
        r = f_readdir(&d, f);
        if (r != FR_OK || !f->fname[0]) { f_closedir(&d); return r != FR_OK ? r : FR_NO_FILE; }
        if (uf_hide(f)) continue;
        if (i++ == n) { f_closedir(&d); return FR_OK; }
    }
}
/* the rows from usbh.top: read again whenever the page scrolls */
static void uf_window(void)
{
    DIR d;
    FILINFO f;
    FRESULT r = f_opendir(&d, usbh.path[0] ? usbh.path : "/");
    uint32_t i = 0, k = 0;
    if (r != FR_OK) { uf_fail(r); return; }
    while (k < UH_ROWS) {
        r = f_readdir(&d, &f);
        if (r != FR_OK) { f_closedir(&d); uf_fail(r); return; }
        if (!f.fname[0]) break;
        if (uf_hide(&f)) continue;
        if (i++ < usbh.top) continue;
        str_cpy(usbh.ent[k].name, f.fname, UH_NAME);
        usbh.ent[k].size = (uint32_t)(f.fsize > 0xFFFFFFFFu ? 0xFFFFFFFFu : f.fsize);
        usbh.ent[k].dir = (uint8_t)!!(f.fattrib & AM_DIR);
        k++;
    }
    f_closedir(&d);
    usbh.nent = (uint8_t)k;
    usbh.ent_top = usbh.top;
    uf_marks();                                         /* (0.4.13) the ones playing */
}
/* the folder in usbh.path: how many, then the first rows */
static void uf_list(void)
{
    DIR d;
    FILINFO f;
    FRESULT r = uf_mount();
    uint32_t n = 0;
    usbh.info[0] = usbh.info2[0] = 0;
    usbh.count = 0;
    usbh.nent = 0;
    if (r == FR_OK) r = f_opendir(&d, usbh.path[0] ? usbh.path : "/");
    if (r != FR_OK) { uf_fail(r); return; }
    while ((r = f_readdir(&d, &f)) == FR_OK && f.fname[0] && n < 9999u)
        if (!uf_hide(&f)) n++;
    f_closedir(&d);
    if (r != FR_OK) { uf_fail(r); return; }
    usbh.count = (uint16_t)n;
    if (!n) str_cpy(usbh.info, "Empty folder", UH_LINE);
    uf_window();
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
/* "44.1 kHz 16-bit mono 0.5 s" */
static void uf_wav_text(const struct uf_wi *w, char *b)
{
    uint32_t bps, ms, secs;
    char *e = b;
    e = uh_dec(e, w->rate / 1000u);                     /* 44.1, 22.05, 11.025, 48 */
    if (w->rate % 1000u) {
        uint32_t f = w->rate % 1000u;
        *e++ = '.';
        *e++ = (char)('0' + f / 100u);
        if (f % 100u) *e++ = (char)('0' + f / 10u % 10u);
        if (f % 10u) *e++ = (char)('0' + f % 10u);
        *e = 0;
    }
    e = uh_cat(e, " kHz ");
    e = uh_dec(e, w->bits);
    e = uh_cat(e, w->fmt == 3u ? "-bit float " : "-bit ");
    e = w->ch == 1u ? uh_cat(e, "mono ") : w->ch == 2u ? uh_cat(e, "stereo ") : uh_cat(uh_dec(e, w->ch), " ch ");
    bps = w->rate * w->ch * (w->bits / 8u ? w->bits / 8u : 1u);
    if (!bps) bps = 1u;
    secs = w->data / bps;                               /* (32-bit: tenths of a second are all the page shows) */
    ms = secs * 1000u + w->data % bps * 10u / bps * 100u;
    if (ms < 60000u) { e = uh_dec(e, ms / 1000u); *e++ = '.'; e = uh_dec(e, ms % 1000u / 100u); uh_cat(e, " s"); }
    else { e = uh_dec(e, ms / 60000u); *e++ = ':'; *e++ = (char)('0' + ms / 1000u % 60u / 10u); *e++ = (char)('0' + ms / 1000u % 10u); *e = 0; }
}

/* ---- (0.4.11) WAVs playing: the main loop's side (the audio and the reading: card_play.c, card_stream.c) ----
 * The main loop only starts and stops them: it maps a file onto the card (uf_extents: the runs of sectors its
 * clusters make) and hands that to the stream engine (cs), which the 10 kHz timer drives; the stream's ring is half
 * filled before the audio ISR starts to play it. At the end of the samples the engine starts again (a loop). Every
 * 250 ms the foot shows how many play, the audio load (the header's meter), the share of the processor the timer's
 * reading took, and the gaps.
 * (0.4.11 read in the main loop: 29% of its time for one stereo track. 0.4.12 moved the reading to the timer: 2%.
 * 0.4.13: up to four at once; OCT+ on a playing one stops it; they play on through the folders, and stop when the
 * file list is left or the drive goes.) */
/* (0.4.15) what the card keeps up with: 0.4.14 on the unit read 550 KB/s at most, with or without the sequencer
 * (3 files of 16-bit stereo 44.1 kHz, 516 KB/s, played clean; 4, 689 KB/s, dropped out). A file that would take the
 * playing ones past this does not start. */
#ifndef UF_BUDGET
#define UF_BUDGET 545000u                               /* bytes a second (532 KB/s) */
#endif
static uint32_t uf_budget = UF_BUDGET;                  /* (host tests change it) */
#ifndef UF_PLAYING
#define UF_PLAYING() (song.playing)                     /* (0.4.17) the sequencer runs (host tests: their own) */
#endif
#ifndef UF_CPU
#define UF_CPU() (song.cpu_q8 * 100u / 256u)
#endif
static FIL uf_pf;
static struct {
    uint8_t n;                                          /* streams playing */
    uint32_t win_ms, show_ms, win_ticks, pct;           /* the second measured: its start, cs.cpu_ticks then; the foot */
    uint32_t win_bytes, kbs;                            /* (0.4.14) cs.bytes then; KB/s read in the last second */
    uint32_t msg_ms;                                    /* (0.4.14) when info2 got a message (held 2 s) */
    uint32_t gaps_old;                                  /* (0.4.14) gaps of streams since replaced in their slot */
    struct { uint32_t dir; char name[UH_NAME]; } who[CP_N];   /* what each slot plays: its folder (a hash), its name */
} up;
static void uf_say(const char *m)                      /* (0.4.14) a message on the foot's middle line, held 2 s */
{
    str_cpy(usbh.info2, m, UH_FOOT);
    up.msg_ms = fm1_ms;
    if (!up.n) usbh.info3[0] = 0;                       /* (none play: no speed line) */
    usbh.gen++;
}
static uint32_t uf_hash(const char *p)                  /* a folder's path, for the "playing" marks */
{
    uint32_t h = 2166136261u;
    while (*p) h = (h ^ (uint8_t)*p++) * 16777619u;
    return h;
}
static int uf_playing(const char *name)                 /* the slot playing this name in this folder, or -1 */
{
    uint32_t s, d = uf_hash(usbh.path);
    for (s = 0; s < CP_N; s++)
        if (cs.st[s].run && up.who[s].dir == d && !memcmp(up.who[s].name, name, str_len(name) + 1u)) return (int)s;
    return -1;
}
static void uf_marks(void)                              /* the rows shown: which play (1), which wait (2, 0.4.17) */
{
    uint32_t i;
    for (i = 0; i < usbh.nent && i < UH_ROWS; i++) {
        int k = usbh.ent[i].dir ? -1 : uf_playing(usbh.ent[i].name);
        usbh.ent[i].play = (uint8_t)(k < 0 ? 0u : cp[k].on ? 1u : 2u);
    }
    usbh.gen++;
}
static int uf_playable(const struct uf_wi *w)
{
    return (w->fmt == 1u || w->fmt == 0xFFFEu) && (w->bits == 8u || w->bits == 16u || w->bits == 24u || w->bits == 32u) &&
           w->ch >= 1u && w->ch <= 8u && w->rate >= 8000u && w->rate <= 192000u;
}
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
/* stop slot s (the command in hand finished first, so a new stream in the slot gets none of its bytes) */
static void uf_play_stop(uint32_t s)
{
    if (!cs.st[s].run && !cp[s].on) return;
    cp[s].on = cp[s].armed = 0;
    cs_claim();
    cs.st[s].run = 0;
    cs_update_run();
    cs_release();
    up.n = (uint8_t)(up.n ? up.n - 1u : 0u);
    uf_say(up.n ? "Stopped one" : "Stopped");
    uf_marks();
}
/* stop them all; after an error the drive is reset (stopped mid-command: it must be back in step) */
static void frz_drop_all(const char *why);              /* (0.4.20) below */
static void uf_play_stop_all(void)
{
    uint8_t err = cs.err;
    uint32_t s;
    if (!up.n && !err && !cs.run) return;
    for (s = 0; s < CP_N; s++) cp[s].on = cp[s].armed = 0;
    cs_claim();
    for (s = 0; s < CP_N; s++) cs.st[s].run = 0;
    cs.run = 0;
    cs.hold = 0;
    if (err) uh_bot_reset();
    cs.err = 0;
    cs.state = CS_IDLE;
    up.n = 0;
    frz_drop_all(err ? "Error" : "");             /* (0.4.20) their takes stopped: back to their engines */
    uf_say(err ? "Stopped: card error" : "Stopped");
    uf_marks();
}
/* (0.4.20) the page left: its players stop, frozen tracks' takes play on */
static void uf_play_stop_page(void)
{
    uint32_t s;
    if (!up.n) return;
    for (s = 0; s < CP_N; s++)
        if (!cp[s].trk && (cs.st[s].run || cp[s].on)) {
            cp[s].on = cp[s].armed = 0;
            cs_claim();
            cs.st[s].run = 0;
            cs_update_run();
            cs_release();
        }
    up.n = 0;
    uf_say("Stopped");
    uf_marks();
}
static void uf_play_show(void)
{
    char b[64], *e;                                     /* (0.4.14) built here, then cut to the line (str_cpy) */
    uint32_t s, gaps = up.gaps_old, need = 0;
    if (fm1_ms - up.win_ms >= 1000u) {                  /* the timer's reading: its share of the last second */
        uint32_t t = cs.cpu_ticks - up.win_ticks, ms = fm1_ms - up.win_ms;
        up.pct = t / FM1_TICKS_PER_US / (ms * 10u);
        up.kbs = (cs.bytes - up.win_bytes) / 1024u * 1000u / ms;   /* (0.4.14) the card's speed */
        up.win_ticks = cs.cpu_ticks;
        up.win_bytes = cs.bytes;
        up.win_ms = fm1_ms;
    }
    if (fm1_ms - up.show_ms < 250u) return;
    up.show_ms = fm1_ms;
    for (s = 0; s < CP_N; s++) {
        gaps += cp[s].gaps;
        if (cs.st[s].run) need += cs.st[s].bps;
    }
    if (fm1_ms - up.msg_ms >= 2000u) {                  /* "1 play, 1 ready, CPU 25%" (a message first, for 2 s); (0.4.18:
                                                         * was "2 on, CPU 25%, card 2%": the card's share is small) */
        uint32_t on = 0;
        for (s = 0; s < CP_N; s++) on += cp[s].on != 0;
        e = uh_dec(b, on);
        e = uh_cat(e, " play, ");
        if (up.n > on) e = uh_cat(uh_dec(e, up.n - on), " ready, ");
        uh_cat(uh_dec(uh_cat(e, "CPU "), UF_CPU()), "%");
        str_cpy(usbh.info2, b, UH_FOOT);
    }
    e = uh_dec(uh_cat(b, "gap "), gaps);                /* "gap 0, 512 of 529 KB/s": the gaps first (always seen) */
    e = uh_dec(uh_cat(e, ", "), up.kbs);
    uh_cat(uh_dec(uh_cat(e, " of "), need / 1024u), " KB/s");
    str_cpy(usbh.info3, b, UH_FOOT);
    usbh.gen++;
}
static int ur_busy(void);                               /* (0.4.20) the recorder in use (below) */
/* start one in a free slot: the file mapped, its reading started, its ring half filled, then the audio ISR plays it */
static void uf_play_start(const struct uf_wi *w, uint32_t fsize, const char *name, int sync)
{
    uint32_t s, t0, want;
    struct cs_one *o;
    for (s = 0; s < CP_N && (cs.st[s].run || cp[s].on); s++)
        ;
    if (usbh.rec || ur_busy()) {                        /* (0.4.16) the recorder has the players' memory (0.4.20: a
                                                         * freeze's too) */
        f_close(&uf_pf);
        uf_say("Recording: REC stops it");
        return;
    }
    if (s == CP_N) {
        f_close(&uf_pf);
        uf_say("Four play already");
        return;
    }
    {   /* (0.4.15) the card's limit */
        uint32_t k, need = w->rate * (w->bits / 8u) * w->ch;
        for (k = 0; k < CP_N; k++)
            if (cs.st[k].run) need += cs.st[k].bps;
        if (need > uf_budget) {
            f_close(&uf_pf);
            uf_say("Too much for the card");
            return;
        }
    }
    o = &cs.st[s];
    if (!uf_extents(&uf_pf, fsize, o)) {
        f_close(&uf_pf);
        uf_say("File in too many pieces");
        return;
    }
    f_close(&uf_pf);                                    /* (the engine needs only the runs of sectors) */
    if (!up.n) {                                        /* (0.4.14) the first: the gaps from nought */
        uint32_t k;
        for (k = 0; k < CP_N; k++) cp[k].gaps = 0;
        up.gaps_old = 0;
    } else {
        up.gaps_old += cp[s].gaps;                      /* (the slot's last stream's, kept in the count) */
    }
    cp_setup(s, w->ch, w->rate);
    o->bytes = (uint8_t)(w->bits / 8u);
    o->bpf = (uint8_t)(o->bytes * w->ch);
    o->bps = w->rate * o->bpf;                          /* (0.4.14) */
    o->right = (uint8_t)(w->ch > 1u ? o->bytes : 0u);
    o->ch = cp[s].ch;
    o->data0 = w->data0;
    o->data = w->data;
    o->pos = 0;
    o->nc = 0;
    o->done = 0;
    o->once = (uint8_t)(sync != 0);                     /* (0.4.17) in time with the sequencer, once (0.4.18: SEQ asked) */
    cp[s].synced = o->once;
    up.who[s].dir = uf_hash(usbh.path);
    str_cpy(up.who[s].name, name, UH_NAME);
    if (!up.n) {                                        /* the first: the engine and the meter from fresh */
        cs.err = 0;
        cs.hold = 0;
        cs.state = CS_IDLE;
        cs.cmds = 0;
        up.win_ms = up.show_ms = fm1_ms;
        up.win_ticks = cs.cpu_ticks;
        up.win_bytes = cs.bytes;
        up.pct = up.kbs = 0;
    }
    sie_wr(S_FADDR, usbh.addr);                         /* (the drive's; the bulk pipes as the test set them) */
    o->run = 1;                                         /* (last: the timer reads the slot from here on) */
    cs_update_run();
    up.n++;
    want = w->data / o->bpf < cp[s].cap / 2u ? w->data / o->bpf : cp[s].cap / 2u;
    for (t0 = fm1_ms; cp[s].w < want && !cs.err && fm1_ms - t0 < 1000u;)   /* half the ring first */
        CS_WAIT();
    if (cs.err || cp[s].w < want) {
        uf_play_stop(s);
        uf_say("Card too slow to start");
        return;
    }
    if (cp[s].synced) {                                 /* (0.4.17) it waits for PLAY (card_play.c card_sync) */
        cp[s].armed = 1;
        uf_say(UF_PLAYING() ? "Ready: plays at the next start" : "Ready: plays with PLAY");
    } else {
        cp[s].on = 1;
        uf_say("Playing");
    }
    uf_marks();
}
/* (0.4.17) a synced WAV the sequencer stopped (or that played to its end): back to its start, its ring half full,
 * waiting for the next start. 0: the card did not keep up (stopped) */
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
    o->done = 0;
    c->w = c->r = c->wi = c->ri = c->phase = 0;
    c->eof = c->shrt = 0;
    cs_release();
    want = o->data / o->bpf < c->cap / 2u ? o->data / o->bpf : c->cap / 2u;
    for (t0 = fm1_ms; c->w < want && !cs.err && fm1_ms - t0 < 1000u;)
        CS_WAIT();
    if (cs.err || c->w < want) {
        uf_play_stop(s);
        uf_say("Card too slow to start");
        return 0;
    }
    c->armed = 1;
    return 1;
}

/* a file picked: a WAV that can play plays (OCT+ on it again stops it); any other is read, up to 1 MB in 512-byte
 * pieces, and timed. (0.4.18) sync: SEQ: a WAV waits for the song; anything else is refused */
static void uf_file(const char *path, const FILINFO *fi, int sync)
{
    FIL f;
    UINT got = 0;
    uint32_t total = 0, t0, ms;
    struct uf_wi w;
    char *e;
    FRESULT r = f_open(&uf_pf, path, FA_READ);
    if (r != FR_OK) { uf_fail(r); return; }
    if (uf_wav_scan(&uf_pf, (uint32_t)fi->fsize, &w)) {
        uf_wav_text(&w, usbh.info);
        if (uf_playable(&w)) { uf_play_start(&w, (uint32_t)fi->fsize, fi->fname, sync); return; }
        f_close(&uf_pf);
        uf_say("This WAV does not play yet");
        return;
    }
    f_close(&uf_pf);
    if (sync) { uf_say("SEQ: on a WAV"); return; }      /* (0.4.18) */
    r = f_open(&f, path, FA_READ);
    if (r != FR_OK) { uf_fail(r); return; }
    e = uh_cat(usbh.info, "File, ");
    uh_bytes(e, (uint32_t)fi->fsize);
    got = 512u;
    t0 = fm1_ms;
    while (got == 512u && total < 1024u * 1024u) {
        r = f_read(&f, uh_sec, 512, &got);
        if (r != FR_OK) break;
        total += got;
        if (!(total & 0x3FFFu)) uh_feed();
    }
    ms = fm1_ms - t0;
    f_close(&f);
    if (r != FR_OK) { uf_fail(r); return; }
    e = uh_bytes(uh_cat(usbh.info2, "Read "), total);
    if (total >= 16384u) {                              /* (too little to time) */
        e = uh_cat(e, " at ");
        e = uh_dec(e, total * 1000u / 1024u / (ms ? ms : 1u));   /* (total <= 1 MB: fits 32 bits) */
        uh_cat(e, " KB/s");
    }
    usbh.gen++;
}
/* OCT+ on a row: a folder is entered, a file looked at (a playing WAV: stopped). (0.4.18) sync: SEQ on a row: a WAV
 * waits for the song (a playing or waiting one: stopped) */
static void uf_enter(int sync)
{
    FILINFO f;
    FRESULT r;
    uint32_t n = str_len(usbh.path);
    int s;
    char fp[sizeof usbh.path + UH_NAME * 2];
    r = uf_nth(usbh.sel, &f);
    if (r != FR_OK) { uf_fail(r); return; }
    if ((f.fattrib & AM_DIR) && sync) { uf_say("SEQ: on a WAV"); return; }
    if (f.fattrib & AM_DIR) {
        if (n + 1u + str_len(f.fname) + 1u > sizeof usbh.path) { str_cpy(usbh.info, "Folder too deep", UH_LINE); usbh.gen++; return; }
        uh_cat(uh_cat(usbh.path + n, "/"), f.fname);
        usbh.sel = usbh.top = 0;
        uf_list();
        return;
    }
    if ((s = uf_playing(f.fname)) >= 0) { uf_play_stop((uint32_t)s); return; }   /* (0.4.13) */
    if (n + 1u + str_len(f.fname) + 1u > sizeof fp) { str_cpy(usbh.info, "Path too long", UH_LINE); usbh.gen++; return; }
    uh_cat(uh_cat(uh_cat(fp, usbh.path), "/"), f.fname);
    usbh.info[0] = usbh.info2[0] = 0;
    uf_file(fp, &f, sync);
}
/* OCT- in a folder: the one above */
static void uf_up(void)
{
    uint32_t n = str_len(usbh.path);
    while (n && usbh.path[n - 1] != '/') n--;
    usbh.path[n ? n - 1 : 0] = 0;
    usbh.sel = usbh.top = 0;
    uf_list();
}

/* main loop (main.c): the drive test, then what the page asked for */
/* ---- (0.4.16) the recorder: what the FM-1 plays, to a WAV on the card ----
 * REC on the file list starts it (any WAVs playing stop first: the recorder takes their memory) in the folder shown,
 * under the first free name OC001.WAV .. OC999.WAV. The audio ISR fills card_play.c's ring (cr, 40 KB); the main
 * loop writes it out 8 KB at a time (16 sectors, one WRITE(10)) with f_write. The file is a plain 16-bit 44.1 kHz
 * WAV whose samples start at byte 512 (a JUNK chunk fills the header's sector), so every write is whole sectors and
 * goes straight to the card (FatFs's window is not used for the samples).
 *
 * (0.4.17) What it records (usbh.src, KNOB 3 on the page): 0 the mix, stereo, after MASTER; 1..4 that track alone
 * (a freeze: it plays back as it went in). (0.4.19) A track take is stereo: the track after its level, pan and its
 * own effects, before the shared effects (its sends) and MASTER (0.4.17 took it mono, before its pan). When it
 * starts: REC while the sequencer plays starts at
 * once and REC stops it; REC while it is stopped arms it ("waits for PLAY"): the take starts in the block the
 * sequencer starts and ends in the block it stops (card_play.c card_sync), so it lines up with the song from its
 * first step. REC again before PLAY cancels it and deletes the empty file.
 *
 * Pulled out mid-way: every 3 s the header gets the length so far and the file is synced (f_sync: its size and
 * clusters on the card), so the card keeps what was recorded up to then. A write that fails (the card full, gone or
 * locked) stops the recording; what was synced stays.
 *
 * The foot while recording: "REC OC001.WAV 12.3 s", then "lost 0, peak -6 dB" (frames the ring had no room for;
 * the loudest sample), then "write 412 KB/s, max 45 ms" (the card's speed for the writes, the longest one: the
 * ring holds 232 ms of stereo, so a write near that loses sound). At most 30 minutes. */
#define UR_CHUNK_B 8192u                                /* bytes a write: 16 sectors (the ring holds five) */
#define UR_MAX (44100u * 60u * 30u)                     /* frames: 30 minutes */
static FIL uf_rf;
static struct {
    uint8_t on;                                         /* a file open: recording, or armed */
    uint8_t frz;                                        /* (0.4.20) track frz - 1's freeze (frz_task drives it), 0 the page's */
    uint8_t ch;                                         /* (0.4.17) 2 the mix, 1 a track */
    char name[12];                                      /* "OC001.WAV" */
    char path[sizeof usbh.path + 16];                   /* (0.4.17) its path (a cancelled take is deleted) */
    uint32_t frames;                                    /* written to the card */
    uint32_t sync_ms, show_ms;
    uint32_t wr_ms, wr_bytes, slow_ms;                  /* time in writes, bytes written, the longest write */
} ur;
static uint32_t ur_chunk(void) { return UR_CHUNK_B / (2u * ur.ch); }   /* frames a write: 2,048 (stereo) */
static int ur_busy(void) { return ur.on; }
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
    ur.sync_ms = fm1_ms;
    return r;
}
/* n frames from the ring (from where the main loop is: whole chunks never wrap) to the card */
static FRESULT ur_write(uint32_t n)
{
    UINT bw;
    FRESULT r;
    uint32_t t0 = fm1_ms, ms, at = cr.r % cr.cap, bytes = n * 2u * ur.ch;
    r = f_write(&uf_rf, cr.buf + at * ur.ch, bytes, &bw);   /* (0.4.20: cr.buf, was the whole pool) */
    ms = fm1_ms - t0;
    ur.wr_ms += ms;
    ur.wr_bytes += bw;
    if (ms > ur.slow_ms) ur.slow_ms = ms;
    if (r == FR_OK && bw != bytes) r = FR_DENIED;       /* (the card full) */
    if (r != FR_OK) return r;
    ur.frames += n;
    RING_PUBLISH();
    cr.r = cr.r + n;
    return FR_OK;
}
static void ur_time(char *e, uint32_t frames)           /* "12.3 s" under a minute, then "12:34" */
{
    uint32_t t = frames / 44100u;
    if (t < 60u) {
        e = uh_dec(e, t);
        *e++ = '.';
        uh_cat(uh_dec(e, frames % 44100u * 10u / 44100u), " s");
    } else {
        e = uh_dec(e, t / 60u);
        *e++ = ':';
        *e++ = (char)('0' + t % 60u / 10u);
        *e++ = (char)('0' + t % 10u);
        *e = 0;
    }
}
static void ur_show(void)
{
    char b[64], *e;
    uint32_t pk = (uint32_t)cr.peak, db = 0, kbs;
    e = uh_cat(uh_cat(uh_cat(b, "REC "), ur.name), " ");
    if (cr.armed) uh_cat(e, "waits for PLAY");          /* (0.4.17) */
    else ur_time(e, ur.frames);
    str_cpy(usbh.info, b, UH_FOOT);
    if (fm1_ms - up.msg_ms >= 2000u) {
        cr.peak = 0;                                    /* (the loudest since the last look) */
        while (pk && pk < 32768u / 2u && db < 60u) { pk *= 2u; db += 6u; }   /* (to 6 dB: enough to set a level) */
        e = uh_dec(uh_cat(b, "lost "), cr.lost);
        e = uh_cat(e, !pk ? ", silent" : db ? ", peak -" : ", peak 0 dB");
        if (pk && db) uh_cat(uh_dec(e, db), " dB");
        str_cpy(usbh.info2, b, UH_FOOT);
    }
    kbs = ur.wr_ms ? ur.wr_bytes / ur.wr_ms * 1000u / 1024u : 0u;
    e = uh_dec(uh_cat(b, "write "), kbs);
    uh_cat(uh_dec(uh_cat(e, " KB/s, max "), ur.slow_ms), " ms");
    str_cpy(usbh.info3, b, UH_FOOT);
    usbh.gen++;
}
/* stop: the rest of the ring out, the header with the length, the file closed (ok 0: the drive is gone, nothing
 * more can be written) */
static void ur_stop(int ok, const char *why)
{
    FRESULT r = FR_OK;
    uint32_t n;
    char b[64], *e;
    cr.on = cr.armed = 0;                               /* (the ISR runs on this core: no block is half in) */
    cr.ended = 0;
    CR_STEM_TRK(0xFFu);
    ur.on = usbh.rec = 0;
    if (ok) {
        while (r == FR_OK && (n = cr.w - cr.r) != 0u) {
            uint32_t at = cr.r % cr.cap;
            if (n > cr.cap - at) n = cr.cap - at;       /* (the last piece may end at the ring's end) */
            if (n > ur_chunk()) n = ur_chunk();
            r = ur_write(n);
        }
        if (r == FR_OK) r = ur_sync();
        f_close(&uf_rf);
    }
    ur_time(uh_cat(uh_cat(uh_cat(b, why ? why : "Saved "), ur.name), ", "), ur.frames);
    if (ok && r == FR_OK) uf_list();                    /* (the new file in the list) */
    str_cpy(usbh.info, b, UH_FOOT);
    if (r != FR_OK) str_cpy(usbh.info2, "Card error at the end", UH_FOOT);
    else { e = uh_dec(uh_cat(b, "lost "), cr.lost); uh_cat(e, " frames"); str_cpy(usbh.info2, b, UH_FOOT); }
    up.msg_ms = fm1_ms;
    usbh.gen++;
}
/* (0.4.17) REC again before PLAY: the armed take dropped, its empty file deleted */
static void ur_cancel(void)
{
    cr.on = cr.armed = 0;
    CR_STEM_TRK(0xFFu);
    ur.on = usbh.rec = 0;
    f_close(&uf_rf);
    f_unlink(ur.path);
    usbh.info[0] = usbh.info3[0] = 0;
    uf_say("Take cancelled");
}
static void ur_start(void)
{
    uint32_t i, n, src = usbh.src;
    UINT bw;
    FRESULT r = uf_mount();
    if (r != FR_OK) { uf_fail(r); return; }
    if (up.n) uf_play_stop_page();                      /* (the recorder takes the players' memory) */
    if (cs.run) { uf_say("Frozen tracks use the memory"); return; }   /* (0.4.20) */
    n = str_len(usbh.path);
    if (n + 11u > sizeof usbh.path) { uf_say("Folder too deep"); return; }
    for (i = 1; i < 1000u; i++) {                       /* the first free name */
        FILINFO fi;
        char *e = ur.name;
        e = uh_cat(e, i < 10u ? "OC00" : i < 100u ? "OC0" : "OC");
        uh_cat(uh_dec(e, i), ".WAV");
        uh_cat(uh_cat(uh_cat(ur.path, usbh.path), "/"), ur.name);
        r = f_stat(ur.path, &fi);
        if (r == FR_NO_FILE) break;
        if (r != FR_OK) { uf_fail(r); return; }
    }
    if (i == 1000u) { uf_say("No free name left"); return; }
    ur.ch = 2u;                                         /* (0.4.19: a track take too; 0.4.17 recorded a track mono) */
    r = f_open(&uf_rf, ur.path, FA_CREATE_NEW | FA_WRITE);
    if (r == FR_OK) {
        ur_header(uh_sec, 0, ur.ch);
        r = f_write(&uf_rf, uh_sec, 512, &bw);
        if (r == FR_OK && bw != 512u) r = FR_DENIED;
        if (r == FR_OK) r = f_sync(&uf_rf);
        if (r != FR_OK) f_close(&uf_rf);
    }
    if (r != FR_OK) {
        uf_say(r == FR_DENIED || r == FR_WRITE_PROTECTED ? "Card full or locked" : "Card would not write");
        return;
    }
    ur.frames = 0;
    ur.wr_ms = ur.wr_bytes = ur.slow_ms = 0;
    ur.sync_ms = ur.show_ms = fm1_ms;
    ur.on = usbh.rec = 1;
    up.msg_ms = fm1_ms - 2000u;                         /* (no message in the way) */
    cr_start(src, !UF_PLAYING());                       /* (last: the ISR fills the ring from here, or from PLAY) */
    ur_show();
}
/* each pass of the main loop while recording: whole chunks to the card, the header every 3 s, the foot */
static void ur_task(void)
{
    FRESULT r = FR_OK;
    uint32_t k, ch = ur_chunk();
    for (k = 0; k < 5u && r == FR_OK && cr.w - cr.r >= ch; k++) r = ur_write(ch);
    if (r == FR_OK && !cr.armed && fm1_ms - ur.sync_ms >= 3000u) r = ur_sync();
    if (r != FR_OK) {                                   /* (the last sync's length stays on the card) */
        cr.on = cr.armed = 0;
        CR_STEM_TRK(0xFFu);
        ur.on = usbh.rec = 0;
        f_close(&uf_rf);
        ur_stop(0, "Card error: ");
        return;
    }
    if (cr.ended) { ur_stop(1, 0); return; }            /* (0.4.17) the sequencer stopped: the take is done */
    if (ur.frames >= UR_MAX) { ur_stop(1, "30 min: "); return; }
    if (fm1_ms - ur.show_ms >= 250u) { ur.show_ms = fm1_ms; ur_show(); }
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
    if (up.n) uf_play_stop_page();                      /* (the page's players: their memory) */
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
    str_cpy(ur.name, p + 11, sizeof ur.name);           /* ("T1_003.WAV") */
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
    ur.wr_ms = ur.wr_bytes = ur.slow_ms = 0;
    ur.sync_ms = ur.show_ms = fm1_ms;
    ur.on = 1;
    ur.frz = (uint8_t)(k + 1u);
    cr.on = cr.armed = 0;                               /* (the page's capture stays off: card_freeze.c fills it) */
    cr.ch = 2u;
    cr.buf = cp_pool + 2u * CP_SLOT;                    /* slots 2 and 3: 5,120 frames, 116 ms */
    cr.cap = CP_SLOT;
    cr.w = cr.r = cr.wi = 0;
    cr.lost = 0;
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
    o->nc = 0;
    o->done = 0;
    o->once = 0;                                        /* (it loops: the file is one cycle) */
    cp[s].trk = (uint8_t)(k + 1u);
    cp[s].synced = 1;
    up.who[s].dir = 0;
    up.who[s].name[0] = 0;
    if (!cs.run) {                                      /* (the engine from fresh) */
        cs.err = 0;
        cs.hold = 0;
        cs.state = CS_IDLE;
        cs.cmds = 0;
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
                if (n > cr.cap - at) n = cr.cap - at;   /* (the ring, 5,120 frames, is not whole chunks: to its end,
                                                         * then on from its start; 1,024 frames still whole sectors) */
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
static void usbf_task(void)
{
    uint8_t c;
    cs_poll();                                          /* (0.4.14) the main loop's turn at the card (every pass) */
    if (up.n && (!usbh.want || !usbh.drive || !usbh.view)) uf_play_stop_page();   /* the files left, the test ended:
                                                         * stopped before usbh_task turns the port off (0.4.20: the
                                                         * page's players; frozen tracks play on) */
    if (ur.on && !ur.frz && (!usbh.want || !usbh.view)) ur_stop(usbh.drive, 0);   /* (0.4.16) the recording kept */
    frz_task();                                         /* (0.4.20) frozen tracks: before usbh_task (it keeps the port) */
    usbh_task();
    if (ur.on && !ur.frz && !usbh.drive) ur_stop(0, "Drive gone: ");   /* (the card keeps what the last sync wrote) */
    if (ur.on && !ur.frz) ur_task();
    {   /* (0.4.17) synced WAVs the sequencer stopped, or that played to their end: back to the start, waiting; the
         * rows' PLAY / READY when the sequencer started some */
        static uint32_t was_on;
        uint32_t s, on = 0;
        for (s = 0; s < CP_N; s++)
            if (cp[s].ended && cs.st[s].run) uf_rearm(s);
        for (s = 0; s < CP_N; s++) on |= (uint32_t)(cp[s].on != 0) << s;
        if (on != was_on) { was_on = on; uf_marks(); }
    }
    if ((up.n || cs.run) && !usbh.drive) uf_play_stop_all();   /* (0.4.11) the drive gone (0.4.20: frozen tracks back
                                                         * to their engines) */
    if (cs.err) uf_play_stop_all();                     /* (0.4.12) the engine stopped on an error */
    if (up.n) uf_play_show();
    if (!usbh.drive && uf_mounted) {                    /* unplugged, or the test ended: forget the volume */
        f_unmount("");
        uf_mounted = 0;
    }
    c = usbh.cmd;
    usbh.cmd = UC_NONE;
    if (c == UC_NONE) return;
    if (c == UC_PROBE) { usbh_reprobe(); return; }
    if (!usbh.drive) { uf_fail(FR_NOT_READY); return; }
    if (c == UC_REC) {                                  /* (0.4.16) REC: start, stop, (0.4.17) or cancel one waiting */
        if (ur.frz) { uf_say("A track is freezing"); return; }   /* (0.4.20) */
        if (ur.on && cr.armed) ur_cancel();
        else if (ur.on) ur_stop(1, 0);
        else ur_start();
        return;
    }
    if (c == UC_LIST) { usbh.path[0] = 0; usbh.sel = usbh.top = 0; uf_list(); }
    else if (c == UC_WINDOW) uf_window();
    else if (c == UC_ENTER) uf_enter(0);
    else if (c == UC_SYNC) uf_enter(1);                 /* (0.4.18) SEQ */
    else if (c == UC_UP) uf_up();                       /* (0.4.13: they play on) */
}
