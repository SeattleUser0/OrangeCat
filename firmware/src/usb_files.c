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

/* "44.1 kHz 16-bit mono 0.5 s" from a WAV's first 512 bytes; 0 when it is not one */
static int uf_wav(const uint8_t *h, uint32_t n, uint32_t fsize, char *b)
{
    uint32_t i = 12, rate = 0, ch = 0, bits = 0, data = 0, fmt = 0, bps, ms, secs;
    char *e;
    if (n < 12u || memcmp(h, "RIFF", 4) || memcmp(h + 8, "WAVE", 4)) return 0;
    while (i + 8u <= n) {
        uint32_t len = h[i + 4] | (uint32_t)h[i + 5] << 8 | (uint32_t)h[i + 6] << 16 | (uint32_t)h[i + 7] << 24;
        if (!memcmp(h + i, "fmt ", 4) && i + 24u <= n) {
            fmt = h[i + 8] | (uint32_t)h[i + 9] << 8;
            ch = h[i + 10] | (uint32_t)h[i + 11] << 8;
            rate = h[i + 12] | (uint32_t)h[i + 13] << 8 | (uint32_t)h[i + 14] << 16 | (uint32_t)h[i + 15] << 24;
            bits = h[i + 22] | (uint32_t)h[i + 23] << 8;
        } else if (!memcmp(h + i, "data", 4)) {
            data = len;
            break;
        }
        i += 8u + len + (len & 1u);
    }
    if (!rate || !ch || !bits) return 0;
    if (!data) data = fsize > i ? fsize - i : 0;        /* (the data chunk past the first 512 bytes) */
    e = b;
    if (rate % 1000u) { e = uh_dec(e, rate / 1000u); *e++ = '.'; e = uh_dec(e, rate % 1000u / 100u); }
    else e = uh_dec(e, rate / 1000u);
    e = uh_cat(e, " kHz ");
    e = uh_dec(e, bits);
    e = uh_cat(e, fmt == 3u ? "-bit float " : "-bit ");
    e = ch == 1u ? uh_cat(e, "mono ") : ch == 2u ? uh_cat(e, "stereo ") : uh_cat(uh_dec(e, ch), " ch ");
    bps = rate * ch * (bits / 8u ? bits / 8u : 1u);
    if (!bps) bps = 1u;
    secs = data / bps;                                  /* (32-bit: tenths of a second are all the page shows) */
    ms = secs * 1000u + data % bps * 10u / bps * 100u;
    if (ms < 60000u) { e = uh_dec(e, ms / 1000u); *e++ = '.'; e = uh_dec(e, ms % 1000u / 100u); uh_cat(e, " s"); }
    else { e = uh_dec(e, ms / 60000u); *e++ = ':'; *e++ = (char)('0' + ms / 1000u % 60u / 10u); *e++ = (char)('0' + ms / 1000u % 10u); *e = 0; }
    return 1;
}
/* a file picked: what it is, then up to 1 MB read in 512-byte pieces, timed */
static void uf_file(const char *path, const FILINFO *fi)
{
    FIL f;
    UINT got = 0;
    uint32_t total = 0, t0, ms;
    char *e;
    FRESULT r = f_open(&f, path, FA_READ);
    if (r != FR_OK) { uf_fail(r); return; }
    r = f_read(&f, uh_sec, 512, &got);
    if (r != FR_OK) { f_close(&f); uf_fail(r); return; }
    if (!uf_wav(uh_sec, got, (uint32_t)fi->fsize, usbh.info)) {
        e = uh_cat(usbh.info, "File, ");
        uh_bytes(e, (uint32_t)fi->fsize);
    }
    total = got;
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
    FRESULT r = uf_nth(usbh.sel, &f);
    uint32_t n = str_len(usbh.path);
    char fp[sizeof usbh.path + UH_NAME * 2];
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
    else if (c == UC_UP) uf_up();
}
