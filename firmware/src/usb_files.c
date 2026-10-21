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
 *   0.4.12: the playing WAV read by the timer interrupt (card_play.c cs_*), not the main loop; FatFs's reads wait
 *          for the engine's command to end (cs_claim). A file in more than 32 runs of sectors does not play.
 *   0.4.11: a WAV picked plays from the card, looped, mixed in at the DAC (card_play.c), with the audio load, the
 *          card's share of the main loop and any gaps on the page. WAV chunks walked in the file (the samples can
 *          start past the first 512 bytes). Other files are still read and timed.
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
DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    (void)buff;
    if (pdrv || !usbh.drive) return RES_NOTRDY;
    return cmd == CTRL_SYNC ? RES_OK : RES_PARERR;
}

/* ---- the list ---- */
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
    usbh.gen++;
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

/* ---- (0.4.11) a WAV playing: the main loop's side (the audio and the reading: card_play.c) ----
 * (0.4.12) The main loop only starts and stops it: it maps the file onto the card (uf_extents: the runs of sectors
 * its clusters make) and hands that to the stream engine (cs), which the 10 kHz timer drives; the ring is filled
 * before the audio ISR starts to play. At the end of the samples the engine starts again (a loop). Every 250 ms the
 * foot shows the audio load (the header's meter), the share of the processor the timer's reading took, and gaps.
 * (0.4.11 read 4 KB at a time in the main loop: 29% of its time for one stereo track, mostly waiting on the bus.) */
#ifndef UF_CPU
#define UF_CPU() (song.cpu_q8 * 100u / 256u)
#endif
static FIL uf_pf;
static struct {
    uint8_t on;
    uint32_t win_ms, show_ms, win_ticks, pct;           /* the second measured: its start, cs.cpu_ticks then; the foot */
} up;
static int uf_playable(const struct uf_wi *w)
{
    return (w->fmt == 1u || w->fmt == 0xFFFEu) && (w->bits == 8u || w->bits == 16u || w->bits == 24u || w->bits == 32u) &&
           w->ch >= 1u && w->ch <= 8u && w->rate >= 8000u && w->rate <= 192000u;
}
/* the file's clusters as runs of sectors (cs.ext), from FatFs's own chain (ff.c's get_fat, clst2sect: one unit with
 * this file). 0 = more runs than cs.ext holds, or a broken chain */
static int uf_extents(FIL *f, uint32_t fsize)
{
    FATFS *fs = f->obj.fs;
    uint32_t bpc = (uint32_t)fs->csize * 512u, need = fsize / bpc + (fsize % bpc != 0u), n;
    DWORD clst = f->obj.sclust;
    cs.next = 0;
    for (n = 0; n < need; n++) {
        LBA_t lba = clst2sect(fs, clst);
        if (!lba) return 0;
        if (cs.next && cs.ext[cs.next - 1u].lba + cs.ext[cs.next - 1u].n == (uint32_t)lba) {
            cs.ext[cs.next - 1u].n += fs->csize;
        } else {
            if (cs.next == CS_EXT) return 0;
            cs.ext[cs.next].fsec = n * fs->csize;
            cs.ext[cs.next].lba = (uint32_t)lba;
            cs.ext[cs.next].n = fs->csize;
            cs.next++;
        }
        if (n + 1u < need) {
            clst = get_fat(&f->obj, clst);
            if (clst < 2u || clst >= fs->n_fatent) return 0;
        }
    }
    return 1;
}
static void uf_play_stop(void)
{
    uint8_t err = cs.err;
    if (!up.on) return;
    cp_on = 0;
    cs_claim();                                         /* the command in hand finished */
    cs.run = 0;
    cs.hold = 0;
    if (err) uh_bot_reset();                            /* stopped mid-command: the drive back in step */
    up.on = 0;
    f_close(&uf_pf);
    str_cpy(usbh.info2, err ? "Stopped: card error" : "Stopped", UH_LINE);
    usbh.gen++;
}
static void uf_play_show(void)
{
    char *e;
    if (fm1_ms - up.win_ms >= 1000u) {                  /* the timer's reading: its share of the last second */
        uint32_t t = cs.cpu_ticks - up.win_ticks, ms = fm1_ms - up.win_ms;
        up.pct = t / FM1_TICKS_PER_US / (ms * 10u);
        up.win_ticks = cs.cpu_ticks;
        up.win_ms = fm1_ms;
    }
    if (fm1_ms - up.show_ms < 250u) return;
    up.show_ms = fm1_ms;
    e = uh_dec(uh_cat(usbh.info2, "CPU "), UF_CPU());
    e = uh_dec(uh_cat(e, "% card "), up.pct);
    e = uh_dec(uh_cat(e, "% gaps "), cp_gaps);
    (void)e;
    usbh.gen++;
}
/* start: the file mapped, the engine started, the ring filled, then the audio ISR plays */
static void uf_play_start(const struct uf_wi *w, uint32_t fsize)
{
    uint32_t t0, want;
    if (!uf_extents(&uf_pf, fsize)) {
        f_close(&uf_pf);
        str_cpy(usbh.info2, "File in too many pieces", UH_LINE);
        usbh.gen++;
        return;
    }
    cp_on = 0;
    cp_w = cp_r = 0;
    cp_phase = 0;
    cp_gaps = 0;
    cp_step = w->rate * 16384u / 11025u;                /* file frames per 44.1 kHz frame, Q16 (192 kHz fits 32 bits) */
    cs.bytes = (uint8_t)(w->bits / 8u);
    cs.bpf = (uint8_t)(cs.bytes * w->ch);
    cs.right = (uint8_t)(w->ch > 1u ? cs.bytes : 0u);
    cs.data0 = w->data0;
    cs.data = w->data;
    cs.pos = 0;
    cs.nc = 0;
    cs.err = 0;
    cs.hold = 0;
    cs.state = CS_IDLE;
    cs.cmds = 0;
    up.win_ms = up.show_ms = fm1_ms;
    up.win_ticks = cs.cpu_ticks;
    up.pct = 0;
    up.on = 1;
    sie_wr(S_FADDR, usbh.addr);                         /* (the drive's; the bulk pipes as the test set them) */
    cs.run = 1;
    want = w->data / cs.bpf < CP_FRAMES / 2u ? w->data / cs.bpf : CP_FRAMES / 2u;
    for (t0 = fm1_ms; cp_w < want && !cs.err && fm1_ms - t0 < 1000u;)   /* half the ring first */
        CS_WAIT();
    if (cs.err || cp_w < want) {
        uf_play_stop();
        str_cpy(usbh.info2, "Card too slow to start", UH_LINE);
        return;
    }
    cp_on = 1;
    str_cpy(usbh.info2, "Playing", UH_LINE);
    usbh.gen++;
}

/* a file picked: a WAV that can play plays (OCT+ again stops it); any other is read, up to 1 MB in 512-byte pieces,
 * and timed */
static void uf_file(const char *path, const FILINFO *fi)
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
        if (uf_playable(&w)) { uf_play_start(&w, (uint32_t)fi->fsize); return; }
        f_close(&uf_pf);
        str_cpy(usbh.info2, "This WAV does not play yet", UH_LINE);
        usbh.gen++;
        return;
    }
    f_close(&uf_pf);
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
/* OCT+ on a row: a folder is entered, a file looked at */
static void uf_enter(void)
{
    FILINFO f;
    FRESULT r;
    uint32_t n = str_len(usbh.path);
    char fp[sizeof usbh.path + UH_NAME * 2];
    if (up.on) { uf_play_stop(); return; }              /* (0.4.11) OCT+ while a WAV plays stops it */
    r = uf_nth(usbh.sel, &f);
    if (r != FR_OK) { uf_fail(r); return; }
    if (f.fattrib & AM_DIR) {
        if (n + 1u + str_len(f.fname) + 1u > sizeof usbh.path) { str_cpy(usbh.info, "Folder too deep", UH_LINE); usbh.gen++; return; }
        uh_cat(uh_cat(usbh.path + n, "/"), f.fname);
        usbh.sel = usbh.top = 0;
        uf_list();
        return;
    }
    if (n + 1u + str_len(f.fname) + 1u > sizeof fp) { str_cpy(usbh.info, "Path too long", UH_LINE); usbh.gen++; return; }
    uh_cat(uh_cat(uh_cat(fp, usbh.path), "/"), f.fname);
    usbh.info[0] = usbh.info2[0] = 0;
    uf_file(fp, &f);
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
static void usbf_task(void)
{
    uint8_t c;
    usbh_task();
    if (up.on && (!usbh.drive || !usbh.view)) uf_play_stop();   /* (0.4.11) the files left, or the drive gone */
    if (up.on) {
        if (cs.err) uf_play_stop();                     /* (0.4.12) the engine stopped on an error */
        else uf_play_show();
    }
    if (!usbh.drive && uf_mounted) {                    /* unplugged, or the test ended: forget the volume */
        f_unmount("");
        uf_mounted = 0;
    }
    c = usbh.cmd;
    usbh.cmd = UC_NONE;
    if (c == UC_NONE) return;
    if (c == UC_PROBE) { usbh_reprobe(); return; }
    if (!usbh.drive) { uf_fail(FR_NOT_READY); return; }
    if (c == UC_LIST) { usbh.path[0] = 0; usbh.sel = usbh.top = 0; uf_list(); }
    else if (c == UC_WINDOW) uf_window();
    else if (c == UC_ENTER) uf_enter();
    else if (c == UC_UP) { uf_play_stop(); uf_up(); }
}
