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
    sie_wr(S_FADDR, usbh.addr);
    while (count) {
        uint32_t n = count > 64u ? 64u : count, lba = (uint32_t)sector;
        for (i = 0; i < 10u; i++) cb[i] = 0;
        cb[0] = 0x28;                                   /* READ(10) */
        cb[2] = (uint8_t)(lba >> 24); cb[3] = (uint8_t)(lba >> 16); cb[4] = (uint8_t)(lba >> 8); cb[5] = (uint8_t)lba;
        cb[8] = (uint8_t)n;
        if (uh_scsi(cb, 10, buff, n * 512u) < 0) return RES_ERROR;
        buff += n * 512u;
        sector += n;
        count -= n;
        uh_feed();
    }
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

/* ---- (0.4.11) a WAV playing: the main loop's side (the audio side: card_play.c) ----
 * The file stays open; the main loop reads 8 KB at a time (whole sectors: one READ(10) each, near full speed USB)
 * into uf_raw, turns the frames into 16-bit stereo in cp_ring, and the audio ISR plays them. At the end of the
 * samples it starts again (a loop: a test can run as long as it likes). Every 250 ms the foot shows the audio load
 * (the same meter as the header's), the share of time the main loop spent reading the card, and the gaps. */
#ifndef UF_CPU
#define UF_CPU() (song.cpu_q8 * 100u / 256u)
#endif
#define UF_RAW 8192u
static uint8_t uf_raw[UF_RAW] __attribute__((section(".pool"), aligned(4)));
static FIL uf_pf;
static struct {
    uint8_t on, ch, bytes, bpf;                         /* playing; the file's channels, bytes a sample, bytes a frame */
    uint32_t data0, data, pos;                          /* where the samples are, how many bytes, how far read */
    uint32_t busy_us, win_ms, show_ms, pct;             /* time reading this second, the second's start, the foot */
} up;
static int uf_playable(const struct uf_wi *w)
{
    return (w->fmt == 1u || w->fmt == 0xFFFEu) && (w->bits == 8u || w->bits == 16u || w->bits == 24u || w->bits == 32u) &&
           w->ch >= 1u && w->ch <= 8u && w->rate >= 8000u && w->rate <= 192000u;
}
static void uf_play_stop(void)
{
    if (!up.on) return;
    cp_on = 0;
    up.on = 0;
    f_close(&uf_pf);
    str_cpy(usbh.info2, "Stopped", UH_LINE);
    usbh.gen++;
}
/* the raw frames to the ring: the left and right of each (the first two channels), the top 16 bits */
static void uf_convert(const uint8_t *p, uint32_t frames)
{
    uint32_t i, w = cp_w, b = up.bytes, step = up.bpf, right = up.ch > 1u ? b : 0u;
    for (i = 0; i < frames; i++, p += step) {
        int16_t *o = cp_ring[(w + i) & (CP_FRAMES - 1u)];
        const uint8_t *l = p + b - 2u, *r = p + right + b - 2u;   /* the two top bytes (16, 24, 32 bits) */
        if (b == 1u) {
            o[0] = (int16_t)((p[0] - 128) << 8);
            o[1] = (int16_t)((p[right] - 128) << 8);
        } else {
            o[0] = (int16_t)(l[0] | l[1] << 8);
            o[1] = (int16_t)(r[0] | r[1] << 8);
        }
    }
    RING_PUBLISH();
    cp_w = w + frames;
}
/* top the ring up, 8 KB a read, at most 4 reads a call (the main loop goes on in between). 0, or an error */
static FRESULT uf_play_fill(void)
{
    uint32_t n;
    for (n = 0; n < 4u; n++) {
        uint32_t room = CP_FRAMES - (cp_w - cp_r), want = UF_RAW / up.bpf, k, t0;
        UINT got = 0;
        FRESULT r;
        if (up.pos >= up.data) {                        /* the end: from the start again */
            if ((r = f_lseek(&uf_pf, up.data0)) != FR_OK) return r;
            up.pos = 0;
        }
        k = (up.data - up.pos) / up.bpf;
        if (k > want) k = want;
        if (!k) { up.pos = up.data; continue; }          /* (a last part frame) */
        if (room < k) break;
        t0 = fm1_ticks();
        r = f_read(&uf_pf, uf_raw, k * up.bpf, &got);
        up.busy_us += (fm1_ticks() - t0) / FM1_TICKS_PER_US;
        if (r != FR_OK) return r;
        if (got < k * up.bpf) { k = got / up.bpf; up.pos = up.data; }
        else up.pos += got;
        uf_convert(uf_raw, k);
        uh_feed();
    }
    return FR_OK;
}
static void uf_play_show(void)
{
    char *e;
    if (fm1_ms - up.win_ms >= 1000u) {                  /* the card's share of the last second */
        up.pct = up.busy_us / ((fm1_ms - up.win_ms) * 10u);
        up.busy_us = 0;
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
/* start: the ring filled first, then the ISR plays */
static void uf_play_start(const struct uf_wi *w)
{
    FRESULT r;
    up.ch = (uint8_t)w->ch;
    up.bytes = (uint8_t)(w->bits / 8u);
    up.bpf = (uint8_t)(up.bytes * w->ch);
    up.data0 = w->data0;
    up.data = w->data;
    up.pos = 0;
    up.busy_us = up.pct = 0;
    up.win_ms = up.show_ms = fm1_ms;
    cp_on = 0;
    cp_w = cp_r = 0;
    cp_phase = 0;
    cp_gaps = 0;
    cp_step = w->rate * 16384u / 11025u;                /* file frames per 44.1 kHz frame, Q16 (192 kHz fits 32 bits) */
    if ((r = f_lseek(&uf_pf, up.data0)) != FR_OK) { f_close(&uf_pf); uf_fail(r); return; }
    up.on = 1;
    while (CP_FRAMES - (cp_w - cp_r) >= UF_RAW / up.bpf && up.pos < up.data) {
        if ((r = uf_play_fill()) != FR_OK) { uf_play_stop(); uf_fail(r); return; }
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
        if (uf_playable(&w)) { uf_play_start(&w); return; }
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
        FRESULT r = uf_play_fill();
        if (r != FR_OK) { uf_play_stop(); uf_fail(r); }
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
