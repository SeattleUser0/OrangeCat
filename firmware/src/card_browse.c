/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat 0.6.1: the card's takes kept in their projects' folders, and a browser to give a phrase a WAV from the
 * card (Jeremiah, October 22, 2026: "We should add a simple file browser to assign existing clips to phrases. Also,
 * the wav files should be better organized on the SD card, perhaps in folders named after the project. ... when they
 * save the project, it will also need to rename the folder.").
 *
 * Included by usb_files.c (it needs FatFs, the frozen phrases' table and the take folders: usb_files.c frzd), before
 * usbf_task, which runs frz_reconcile and fb_task each pass of the main loop.
 *
 * 1. Naming (frz_named, from project.c through frz_named_hook): a project named or renamed changes where its takes
 *    belong (their entry's want); 2. frz_reconcile moves the files there when it can; 3. the browser.
 *
 * The browser (the MIXER page's sheet, OCT+ held: "Assign clip"; ui_popup.c draws it, POP_FILES):
 *   - opening it makes the USB port a host while it is open (uf_browse: the drive needs it; USB MIDI and the web
 *     editor are offline meanwhile, as while a frozen phrase plays) and waits for the card (8 s, "No card");
 *   - it starts in the music's folder (/ORANGECAT/<its name>, else /ORANGECAT, else the card's root); KNOB 2 moves,
 *     OCT+ opens a folder (".." goes up) or gives the WAV to the phrase the track is on, OCT- goes up a folder or
 *     closes it at the top;
 *   - a WAV given to a phrase makes it frozen with that take, as a freeze would (card_freeze.c frzp): its length, the
 *     tempo and the track's LEVEL now, the phrase's CRC. It must be the takes' format: 16-bit stereo 44.1 kHz PCM, at
 *     least 256 frames, its name 13 characters at most, in a folder (not the card's root). The file stays where it
 *     is. A clip of another length than the phrase's pass plays from each pass's start and is cut at the next pass
 *     (or ends early, silence for the rest of it);
 *   - the list: up to FB_MAX entries of a folder (folders first, then .WAV files, each by name), its rows in the
 *     project staging RAM (project.c proj_wire, borrowed while the browser shows them; another job using it there
 *     has the folder read again).
 *
 * The asset pack (Orange Cat 0.8.0, oc_pack.c): the list shows the .PAK files too (value "Pack"); OCT+ on one installs it, if it
 *   is this build's (the file's size and header agree with OC_PACK_LEN and OC_PACK_HASH). It takes no preset (0.9.1: the
 *   library is in the group of lib_group.c, outside the pack area), so there is nothing to ask. Only while stopped, as every flash write. The browser closes on success ("PACK INSTALLED").
 *
 * Engine modules (Orange Cat 0.10.0, oc_module.c): .OCM files are listed too (value "Module"); OCT+ on one installs the image for the place the unit
 *   names (oc_mod_plan), if the package is for this firmware (interface identifier, table address and version, pack hash; the web page's checks, ocmCheck).
 *   Only while stopped. The browser closes on success ("MODULE INSTALLED").
 *
 * Revisions:
 * (Orange Cat 0.9.1) Library in groups with a shared spare sector (0.9.1)
 * (Orange Cat 0.10.1) module sector tails against the update loader's record rule
 * (Orange Cat 0.12.1) the Prophet as a module at 8 sectors, three module starts, a module of another engine in the way is refused and named
 *   0.6.1: first version.
 *   0.8.0: .PAK files listed and installed.
 *   0.10.0: .OCM files listed and installed.
 *   0.12.1: three module starts (sector 0, 6 and 4): the package variant is picked by its sector index (OC_MOD_START_SECTOR); an install no start can take because a module of
 *           another engine stands in its sectors says "PROPHET at 2: remove it" (oc_mod_blocked_by), never erasing that module.
 */

/* ---------------------------------------------------------------- naming --- */
/* the music named (project.c, after a save or a rename that worked): its takes' folder follows. old, nw: the name
 * before and after ("" none); slot: the slot written (its phrases: FRZ_SLOT_NAMES); rename: the project itself got
 * the new name (its slot renamed, or saved back to the slot it came from), not a copy saved under it */
static void frz_named(const char *old, const char *nw, uint32_t slot, int rename)
{
    char o[FZD_LEN], n[FZD_LEN];
    uint32_t io, in, i, j;
    frz_dir_for(o, old);
    frz_dir_for(n, nw);
    if (frz_same_dir(o, n)) return;
    in = frzd_get(n);
    if (in >= FZD_N) return;                            /* (the table full: the takes stay where they are) */
    if (rename && !frz_same_dir(o, "ORANGECAT/UNTITLED")) {   /* the project's folder renamed whole */
        io = frzd_get(o);
        if (io >= FZD_N) return;
        for (i = 0; i < FZP_N; i++)
            if (frzp[i].take && frzp[i].want == io) frzp[i].want = (uint8_t)in;
        for (j = 0; j < FZD_MV && frzd.mv_from[j] < FZD_N; j++)
            if (frzd.mv_to[j] == io) frzd.mv_to[j] = (uint8_t)in;   /* (renamed again before the card came back) */
        for (j = 0; j < FZD_MV && frzd.mv_from[j] < FZD_N && frzd.mv_from[j] != io; j++)
            ;
        if (j < FZD_MV) {                               /* (no room: its phrases' takes still move one by one) */
            frzd.mv_from[j] = (uint8_t)io;
            frzd.mv_to[j] = (uint8_t)in;
        }
    } else {                                            /* its phrases' takes from UNTITLED (a copy: none move) */
        io = frzd_find(o);
        if (io < FZD_N && frz_same_dir(o, "ORANGECAT/UNTITLED"))
            for (i = 0; i < FZP_N; i++)
                if (frzp[i].take && frzp[i].want == io && FRZ_SLOT_NAMES(slot, i)) frzp[i].want = (uint8_t)in;
    }
    frzp_dirty = 1;
}

/* ------------------------------------------------------------- the moves --- */
static int frz_card_quiet(void)                         /* the card free for moves: no freeze, no recorder, stopped */
{
    uint32_t k;
    if (song.playing || transport_busy() || ur.on) return 0;
    for (k = 0; k < 4u; k++) if (frz[k].st != FZ_OFF) return 0;
    return 1;
}
/* every entry whose file is (dir, name) now at (dir2, name2) */
static void frz_moved(uint32_t dir, const char *name, uint32_t dir2, const char *name2)
{
    for (uint32_t i = 0; i < FZP_N; i++)
        if (frzp[i].take && frzp[i].dir == dir && str_eq(frzp[i].name, name)) {
            frzp[i].dir = (uint8_t)dir2;
            str_cpy(frzp[i].name, name2, sizeof frzp[i].name);
        }
    frzp_dirty = 1;
}
/* a move still to make (a folder to rename, a take not in its project's folder) */
static int frz_pending(void)
{
    uint32_t i;
    for (i = 0; i < FZD_MV; i++) if (frzd.mv_from[i] < FZD_N) return 1;
    for (i = 0; i < FZP_N; i++) if (frzp[i].take && frzp[i].dir != frzp[i].want) return 1;
    return 0;
}
/* one move toward where the takes belong (the card in, mounted, quiet). 1: one made (or given up), 0: nothing to do */
static int frz_reconcile(void)
{
    char p[64], q[64];
    FILINFO fi;
    uint32_t i, j;
    for (j = 0; j < FZD_MV; j++) {                      /* a project's folder renamed whole */
        uint32_t a = frzd.mv_from[j], b = frzd.mv_to[j];
        if (a >= FZD_N) continue;
        frzd.mv_from[j] = frzd.mv_to[j] = FZD_N;        /* (once: whatever happens, the rest goes one by one) */
        frzp_dirty = 1;
        frz_dir_path(p, a);
        frz_dir_path(q, b);
        if (a != b && f_stat(p, &fi) == FR_OK && (fi.fattrib & AM_DIR) && f_stat(q, &fi) == FR_NO_FILE &&
            frz_mkdirs(0) == FR_OK && f_rename(p, q) == FR_OK)
            for (i = 0; i < FZP_N; i++)
                if (frzp[i].take && frzp[i].dir == a) frzp[i].dir = (uint8_t)b;
        return 1;
    }
    for (i = 0; i < FZP_N; i++) {                       /* a take to its project's folder */
        frzp_t *f = &frzp[i];
        char nm[14];
        uint32_t t;
        if (!f->take || f->dir == f->want) continue;
        frz_entry_path(p, f);
        str_cpy(nm, f->name, sizeof nm);
        if (f_stat(p, &fi) != FR_OK) {                  /* gone from where it was: there already (a move cut before
                                                         * its entry was kept), or lost (its phrase plays its notes) */
            frz_moved(f->dir, nm, f->want, nm);
            return 1;
        }
        if (frz_mkdirs(f->want) != FR_OK) return 0;
        frz_path(q, f->want, nm);
        for (t = 1; f_stat(q, &fi) == FR_OK && t < 1000u; t++) {   /* (the name taken there: the next free one) */
            frz_take_name(nm, f->trk, t);
            frz_path(q, f->want, nm);
        }
        if (t >= 1000u || f_rename(p, q) != FR_OK) {
            f->want = f->dir;                           /* (it cannot move: it stays, not tried again) */
            frzp_dirty = 1;
            return 1;
        }
        frz_moved(f->dir, f->name, f->want, nm);
        return 1;
    }
    return 0;
}

/* ------------------------------------------------------------ the browser --- */
#define FB_MAX 64u
#define FB_NAME 48u                                     /* (0.13.3: 40; a set manifest of a CI run is orangecat-X.Y.Z-abcdef0.prophet.set.json, 40 characters) */
#define FB_PATH 96u
typedef struct { char name[FB_NAME]; uint8_t dir, longname; uint32_t size; } fb_ent_t;
#ifndef FB_SCRATCH
#define FB_SCRATCH() ((fb_ent_t *)proj_wire_u.raw)      /* (project.c's staging RAM, 27.9 KB) */
#define FB_BORROW() ((void)proj_wire_borrow())
#define FB_GEN() proj_wire_gen
#endif
_Static_assert(FB_MAX * sizeof(fb_ent_t) <= 4096u, "the browser's rows: 4 KB of the borrowed RAM at most");
enum { FB_OFF, FB_CARD, FB_LIST, FB_READY, FB_FAIL };
static struct {
    uint8_t st, trk, gen, up;                           /* FB_*; the track; FB_GEN when listed; a ".." row first */
    uint8_t relist;                                     /* FB_FAIL over the list (a WAV refused): OCT+ / OCT- back to it */
    uint8_t ask, packed;                                /* (0.8.0) the row (+1) a pack install waits to be confirmed on; a pack was installed (1), a module (2), a set (3) */
    uint8_t setq;                                       /* (0.13.3) a set install: the module (+1) whose in-place question is waiting on that row */
    uint16_t n, sel;                                    /* the rows (".." too), the one picked */
    uint32_t t0;
    char path[FB_PATH];                                 /* the folder shown: "/" or "/ORANGECAT/JAM" */
    char msg[24];                                       /* FB_FAIL's words */
} fb;

static int fb_wav(const char *n)                        /* ends in .WAV (any case) */
{
    uint32_t l = str_len(n);
    return l > 4u && n[l - 4u] == '.' && (n[l - 3u] | 32) == 'w' && (n[l - 2u] | 32) == 'a' && (n[l - 1u] | 32) == 'v';
}
#if MELODEE_FLASH || defined(OC_PACK_HOST)
#define FB_PAK 1
static int fb_pak(const char *n)                        /* (0.8.0) ends in .PAK (any case) */
{
    uint32_t l = str_len(n);
    return l > 4u && n[l - 4u] == '.' && (n[l - 3u] | 32) == 'p' && (n[l - 2u] | 32) == 'a' && (n[l - 1u] | 32) == 'k';
}
#if OC_MODULES
#define FB_OCM 1
#define FB_SET 1
static int fb_set(const char *n)                        /* (0.13.3) ends in .set.json (any case): a set's manifest (docs/orangecat/SET_FORMAT.md) */
{
    static const char tail[] = ".set.json";
    uint32_t l = str_len(n), k;
    if (l <= sizeof tail - 1u) return 0;
    for (k = 0; k < sizeof tail - 1u; k++) {
        char c = n[l - (sizeof tail - 1u) + k];
        if ((c >= 'A' && c <= 'Z' ? c + 32 : c) != tail[k]) return 0;
    }
    return 1;
}
static int fb_ocm(const char *n)                        /* (0.10.0) ends in .OCM (any case) */
{
    uint32_t l = str_len(n);
    return l > 4u && n[l - 4u] == '.' && (n[l - 3u] | 32) == 'o' && (n[l - 2u] | 32) == 'c' && (n[l - 1u] | 32) == 'm';
}
#else
#define FB_OCM 0
#define FB_SET 0
static int fb_ocm(const char *n) { (void)n; return 0; }
static int fb_set(const char *n) { (void)n; return 0; }
#endif
#else
#define FB_PAK 0
#define FB_OCM 0
#define FB_SET 0
static int fb_pak(const char *n) { (void)n; return 0; }
static int fb_ocm(const char *n) { (void)n; return 0; }
static int fb_set(const char *n) { (void)n; return 0; }
#endif
static int fb_before(const fb_ent_t *a, const fb_ent_t *b)   /* folders first, then by name (case aside) */
{
    const char *x = a->name, *y = b->name;
    if (a->dir != b->dir) return a->dir > b->dir;
    for (; *x && *y; x++, y++) {
        char c = (char)(*x >= 'a' && *x <= 'z' ? *x - 32 : *x), d = (char)(*y >= 'a' && *y <= 'z' ? *y - 32 : *y);
        if (c != d) return c < d;
    }
    return !*x && *y;
}
static void fb_unfail(void)                             /* (0.8.0) a word over the list gone: the list again; a pack install's question (relist 2)
                                                         * keeps the rows and the row */
{
    fb.st = fb.relist == 2u ? FB_READY : FB_LIST;
    fb.relist = 0;
}
static void fb_fail(const char *m)
{
    str_cpy(fb.msg, m, sizeof fb.msg);
    fb.st = FB_FAIL;
}
/* the folder fb.path read into the rows */
static void fb_list(void)
{
    DIR d;
    FILINFO fi;
    fb_ent_t *e;
    uint32_t n = 0, i;
    FB_BORROW();
    e = FB_SCRATCH();
    fb.gen = (uint8_t)FB_GEN();
    fb.up = !str_eq(fb.path, "/");
    if (f_opendir(&d, fb.path) != FR_OK) { fb_fail("Folder unreadable"); return; }
    while (n < FB_MAX && f_readdir(&d, &fi) == FR_OK && fi.fname[0]) {
        int dir = (fi.fattrib & AM_DIR) != 0;
        if ((fi.fattrib & (AM_HID | AM_SYS)) || fi.fname[0] == '.' || (!dir && !fb_wav(fi.fname) && !(FB_PAK && fb_pak(fi.fname)) && !(FB_OCM && fb_ocm(fi.fname)) && !(FB_SET && fb_set(fi.fname)))) continue;
        str_cpy(e[n].name, fi.fname, FB_NAME);
        e[n].dir = (uint8_t)dir;
        e[n].longname = (uint8_t)(str_len(fi.fname) >= FB_NAME);
        e[n].size = (uint32_t)fi.fsize;
        for (i = n; i > 0u && fb_before(&e[n], &e[i - 1u]); i--)   /* (in order as they come) */
            ;
        if (i < n) {                                    /* (no memmove in the firmware's libc: one by one, from the end) */
            fb_ent_t t = e[n];
            for (uint32_t j = n; j > i; j--) e[j] = e[j - 1u];
            e[i] = t;
        }
        n++;
    }
    f_closedir(&d);
    fb.n = (uint16_t)(n + fb.up);
    fb.sel = fb.n > fb.up ? fb.up : 0u;                 /* (on the first entry, past "..") */
    fb.st = FB_READY;
}
static const fb_ent_t *fb_ent(uint32_t row)             /* row's entry, 0: ".." or none */
{
    if (fb.st != FB_READY || row < fb.up || row >= fb.n) return 0;
    return &FB_SCRATCH()[row - fb.up];
}
/* where it starts: the music's folder, else /ORANGECAT, else the root */
static void fb_start(void)
{
    char d[FZD_LEN];
    FILINFO fi;
    frz_dir_for(d, FRZ_MUSIC_NAME());
    str_cpy(fb.path, "/", FB_PATH);
    str_cpy(fb.path + 1, d, FB_PATH - 1u);
    if (f_stat(fb.path, &fi) == FR_OK && (fi.fattrib & AM_DIR)) return;
    str_cpy(fb.path, "/ORANGECAT", FB_PATH);
    if (f_stat(fb.path, &fi) == FR_OK && (fi.fattrib & AM_DIR)) return;
    str_cpy(fb.path, "/", FB_PATH);
}

/* ---- the UI's side (ui_popup.c) ---- */
static void fb_open(uint32_t k)
{
    memset(&fb, 0, sizeof fb);
    fb.trk = (uint8_t)k;
    fb.st = FB_CARD;
    fb.t0 = fm1_ms;
    uf_browse = 1;
    usbh_want(1);
}
static void fb_close(void)
{
    fb.st = FB_OFF;
    uf_browse = 0;
    usbh_want(0);                                       /* (frozen phrases in use keep it a host) */
}
static int fb_on(void) { return fb.st != FB_OFF; }
/* the main loop's turn (usbf_task): the card waited for, the folder read */
static void fb_task(void)
{
    if (fb.st == FB_CARD) {
        if (usbh.drive && uf_mount() == FR_OK) {
            fb_start();
            fb.st = FB_LIST;
        } else if (fm1_ms - fb.t0 > 8000u) {
            fb_fail("No card");
        }
    }
    if (fb.st == FB_READY && (uint8_t)FB_GEN() != fb.gen) fb.st = FB_LIST;   /* (its rows' RAM used for another job) */
    if (fb.st == FB_LIST) fb_list();
}
/* the title's words: the folder's last name ("Card" at the root) */
static const char *fb_where(void)
{
    const char *s = fb.path, *t = fb.path;
    for (; *s; s++) if (*s == '/' && s[1]) t = s + 1;
    return str_eq(fb.path, "/") ? "Card" : t;
}
/* row i: its words (name) and its value (a folder ">", a WAV's seconds, ".." "Up"); 0: none */
static int fb_row(uint32_t i, char *name, uint32_t nlen, char *val, uint32_t vlen)
{
    const fb_ent_t *e;
    name[0] = val[0] = 0;
    if (fb.st == FB_CARD) { if (i) return 0; str_cpy(name, "Waiting for the card", nlen); return 1; }
    if (fb.st == FB_FAIL || fb.st == FB_LIST) { if (i) return 0; str_cpy(name, fb.st == FB_FAIL ? fb.msg : "Reading", nlen); return 1; }
    if (fb.st != FB_READY) return 0;
    if (!fb.n) { if (i) return 0; str_cpy(name, "No WAVs here", nlen); return 1; }
    if (i >= fb.n) return 0;
    if (fb.up && i == 0u) { str_cpy(name, "..", nlen); str_cpy(val, "Up", vlen); return 1; }
    e = fb_ent(i);
    str_cpy(name, e->name, nlen);
    if (e->dir) {
        str_cpy(val, ">", vlen);
    } else if (fb_pak(e->name)) {
        str_cpy(val, "Pack", vlen);
    } else if (fb_ocm(e->name)) {
        str_cpy(val, "Module", vlen);
    } else if (fb_set(e->name)) {
        str_cpy(val, "Set", vlen);
    } else {                                            /* (seconds of 16-bit stereo 44.1 kHz, one decimal) */
        uint32_t ds = e->size > 44u ? (e->size - 44u) / 17640u : 0u;
        char *v = uh_dec(val, ds / 10u);
        *v++ = '.';
        v = uh_dec(v, ds % 10u);
        uh_cat(v, " s");
    }
    return 1;
}
static uint32_t fb_sel_now(void) { return fb.st == FB_READY ? fb.sel : 0u; }
static uint32_t fb_sig(void)                            /* what the popup shows changed */
{
    uint32_t h = (uint32_t)fb.st * 131u + fb.n * 7u + fb.sel * 977u + fb.relist;
    for (const char *s = fb.path; *s; s++) h = h * 31u + (uint8_t)*s;
    return h;
}
static void fb_move(int32_t d)
{
    if (fb.st == FB_FAIL && fb.relist && d) { fb_unfail(); return; }   /* (a word over the list: a
                                                         * turn goes back to it) */
    if (fb.st != FB_READY || !fb.n || !d) return;
    fb.ask = 0;
    fb.sel = (uint16_t)clamp((int32_t)fb.sel + (d > 0 ? 1 : -1), 0, (int32_t)fb.n - 1);
}
static void fb_go_up(void)                              /* the folder above (the root: none) */
{
    char *s = fb.path, *last = fb.path;
    for (; *s; s++) if (*s == '/') last = s;
    if (last == fb.path) fb.path[1] = 0;
    else *last = 0;
    fb.st = FB_LIST;
}
/* OCT-: up a folder, or (the root, or not reading) closed. 1: it closed */
static int fb_back(void)
{
    if (fb.st == FB_FAIL && fb.relist) { fb_unfail(); return 0; }
    if (fb.st == FB_READY && fb.up) { fb_go_up(); return 0; }
    fb_close();
    return 1;
}
/* the WAV at (fb.path, e) given to the phrase track k is on: 0 done, else fb.msg says why */
static int fb_assign(uint32_t k, const fb_ent_t *e)
{
    char p[FB_PATH + FB_NAME + 2u];
    uint32_t b = trk[k].pattern % NPAT, id, dir, i, hash;
    struct uf_wi w;
    FILINFO fi;
    FIL *f = &uf_pf;
    int ok;
    if (song.playing || transport_busy()) { str_cpy(fb.msg, "Stop first", sizeof fb.msg); return 1; }
    if (k >= trk_n() || k >= 4u) { str_cpy(fb.msg, "No track", sizeof fb.msg); return 1; }
    if (frz[k].st != FZ_OFF) { str_cpy(fb.msg, "Freezing", sizeof fb.msg); return 1; }
    if (!trk[k].p[P_LEVEL]) { str_cpy(fb.msg, "Level is 0", sizeof fb.msg); return 1; }
    if (e->longname || str_len(e->name) > 13u) { str_cpy(fb.msg, "Name too long", sizeof fb.msg); return 1; }
    if (str_eq(fb.path, "/")) { str_cpy(fb.msg, "Use a folder", sizeof fb.msg); return 1; }
    if (str_len(fb.path) - 1u >= FZD_LEN) { str_cpy(fb.msg, "Path too long", sizeof fb.msg); return 1; }
    if (ocl_keep_any(k, b)) { str_cpy(fb.msg, "Phrases full", sizeof fb.msg); return 1; }   /* (the steps kept: a number,
                                                         * an empty phrase too: 0.6.1 on the unit said "Save first") */
    id = frz_phrase_of(k);
    hash = ocl_bank_hash(k, b);
    if (!id || (frz_pool_hash && frz_pool_hash[id - 1u] != hash)) { str_cpy(fb.msg, "Save first", sizeof fb.msg); return 1; }
    uh_cat(uh_cat(uh_cat(p, fb.path), "/"), e->name);
    if (f_stat(p, &fi) != FR_OK || f_open(f, p, FA_READ) != FR_OK) { str_cpy(fb.msg, "Unreadable", sizeof fb.msg); return 1; }
    ok = uf_wav_scan(f, (uint32_t)fi.fsize, &w);
    f_close(f);
    if (!ok || w.fmt != 1u || w.ch != 2u || w.rate != 44100u || w.bits != 16u) {
        str_cpy(fb.msg, "Needs 44.1k 16b stereo", sizeof fb.msg);
        return 1;
    }
    if (w.data / 4u < 256u) { str_cpy(fb.msg, "Too short", sizeof fb.msg); return 1; }
    dir = frzd_get(fb.path + 1);
    if (dir >= FZD_N) { str_cpy(fb.msg, "Too many folders", sizeof fb.msg); return 1; }
    {
        frzp_t *z = &frzp[id - 1u];
        memset(z, 0, sizeof *z);
        z->take = 1;
        z->trk = (uint8_t)k;
        z->ver = (uint8_t)FZP_VER;
        z->lvl = trk[k].p[P_LEVEL];
        z->bpm = song.g[G_BPM];
        z->frames = w.data / 4u;
        z->hash = hash;
        z->dir = z->want = (uint8_t)dir;
        str_cpy(z->name, e->name, sizeof z->name);
    }
    frzp_dirty = 1;
    for (i = 0; i < 4u; i++)                            /* (a track holding the phrase's take before: loaded again) */
        if (fzt[i].phr == id && !(fzt[i].slot < CP_N && cp[fzt[i].slot].on)) frz_unload(i, FZW_NONE);
    fzt[k].fail = 0;
    return 0;
}
#if FB_PAK
/* (0.8.0) the pack file at (fb.path, e) installed: 0 done, else fb.msg says why. Reads it in 512-byte pieces: the header, the
 * tables to oc_pack_write, the header again to oc_pack_end (it is programmed last). */
static void fb_file(char *p, const char *name)          /* (0.13.3) the path of a file of the folder shown */
{
    uh_cat(uh_cat(uh_cat(p, fb.path), str_eq(fb.path, "/") ? "" : "/"), name);
}
/* (0.13.3) the pack file at p (size bytes) installed, or with dry only checked as far as nothing is written: 0 done, else fb.msg says why */
static int fb_pak_file(const char *p, uint32_t size, int dry)
{
    uint8_t buf[512];
    FIL *f = &uf_pf;
    UINT got;
    uint32_t len = OC_PACK_LEN, off;
    int rc;
    if (song.playing || transport_busy()) { str_cpy(fb.msg, "Stop first", sizeof fb.msg); return 1; }
    if (size != len) { str_cpy(fb.msg, "Not this build's pack", sizeof fb.msg); return 1; }
    if (f_open(f, p, FA_READ) != FR_OK) { str_cpy(fb.msg, "Unreadable", sizeof fb.msg); return 1; }
    if (f_read(f, buf, sizeof buf, &got) != FR_OK || got != sizeof buf || pk_header_state(buf) != PK_OK ||
        pk_rd32(buf + 12) != len) {
        f_close(f);
        str_cpy(fb.msg, "Not this build's pack", sizeof fb.msg);
        return 1;
    }
    if (dry) { f_close(f); return 0; }
    if (oc_pack_begin(len) != 0) { f_close(f); str_cpy(fb.msg, "Install failed", sizeof fb.msg); return 1; }
    rc = 0;
    for (off = OC_PACK_HDR; off < len && !rc; off += got) {
        uint32_t n = len - off > sizeof buf ? (uint32_t)sizeof buf : len - off;
        if (f_read(f, buf, n, &got) != FR_OK || got != n || oc_pack_write(off, buf, n) != 0) rc = 1;
        SMP_FEED();
    }
    if (!rc && (f_lseek(f, 0) != FR_OK || f_read(f, buf, sizeof buf, &got) != FR_OK || got != sizeof buf || oc_pack_end(buf) != 0))
        rc = 1;
    f_close(f);
    if (rc) { str_cpy(fb.msg, "Install failed", sizeof fb.msg); return 1; }
    return 0;
}
static int fb_install(const fb_ent_t *e)
{
    char p[FB_PATH + FB_NAME + 2u];
    fb_file(p, e->name);
    if (fb_pak_file(p, e->size, 0)) return 1;
    fb.packed = 1;
    return 0;
}
#endif
#if FB_OCM
static uint32_t fb_put(char *d, uint32_t at, const char *s, uint32_t max)        /* (0.12.1) up to max characters of s at d[at]; the new end (fb.msg holds 24 with its NUL: the callers' sum is 23 at most) */
{
    uint32_t k;
    for (k = 0; s[k] && k < max && at + 1u < sizeof fb.msg; k++) d[at++] = s[k];
    return at;
}
/* (0.10.0) the module package at (fb.path, e) installed: 0 done, else fb.msg says why. The package: a 56 byte header, a table of 16 byte variants, a CRC-32 of
 * both, then the images (web/EDITOR_PROTOCOL.md). The image for the place oc_mod_plan names goes to oc_mod_begin / write in 256 byte pieces; its first
 * 256 bytes (the header page) go to oc_mod_end, which programs them last. */
static int fb_mod_file(const char *p, uint32_t size, int armed, uint32_t q, int dry, uint32_t *engine_out)
{
    uint8_t buf[256], pg[256];
    FIL *f = &uf_pf;
    UINT got;
    uint32_t n, tab = 56u, hc, engine, start, off = 0, len = 0, o;
    int plan, rc = 0;
    if (song.playing || transport_busy()) { str_cpy(fb.msg, "Stop first", sizeof fb.msg); return 1; }
    if (f_open(f, p, FA_READ) != FR_OK) { str_cpy(fb.msg, "Unreadable", sizeof fb.msg); return 1; }
    if (f_read(f, buf, sizeof buf, &got) != FR_OK || got < 64u || pk_rd32(buf) != 0x464D434Fu || pk_rd16(buf + 4) != 1u) {
        f_close(f);
        str_cpy(fb.msg, "Not a module", sizeof fb.msg);
        return 1;
    }
    n = pk_rd16(buf + 6);
    hc = tab + 16u * n;
    if (n < 1u || n > 8u || hc + 4u > got || ocs_crc32(buf, hc) != pk_rd32(buf + hc)) { f_close(f); str_cpy(fb.msg, "Not a module", sizeof fb.msg); return 1; }
    engine = pk_rd32(buf + 16);
    if (engine_out) *engine_out = engine;
    if (pk_rd32(buf + 8) != FW_API_IFACE || pk_rd32(buf + 12) != FW_API_ADDR || pk_rd32(buf + 44) > FW_API_VERSION ||
        pk_rd32(buf + 48) > sizeof(fw_api_t) || pk_rd32(buf + 24) != OC_PACK_HASH) {
        f_close(f);
        str_cpy(fb.msg, "Not for this firmware", sizeof fb.msg);
        return 1;
    }
    plan = oc_mod_plan(engine);
    if (plan < 0) {
        int b = oc_mod_blocked_by(engine);             /* (0.12.1) a usable module of another engine stands in the way: never erased by an install; the user removes it first */
        f_close(f);
        if (b >= 0) {                                   /* "ENGINE X INSTALLED AT START N: REMOVE IT FIRST" in the 23 characters a row has: "PROPHET at 2: remove it" */
            const char *nm = ENGINES[oc_mod.engine[b] < NENGINES ? oc_mod.engine[b] : 0]->name;
            uint32_t at = 0;
            at = fb_put(fb.msg, at, nm, 9u);
            at = fb_put(fb.msg, at, " at ", 4u);
            fb.msg[at++] = (char)('0' + b);
            at = fb_put(fb.msg, at, ": remove it", 11u);
            fb.msg[at] = 0;
        } else str_cpy(fb.msg, "No place free", sizeof fb.msg);
        return 1;
    }
    start = (uint32_t)plan;
    for (o = 0; o < n; o++)
        if (pk_rd32(buf + tab + 16u * o) == OC_MOD_START_SECTOR(start)) { len = pk_rd32(buf + tab + 16u * o + 4u); off = pk_rd32(buf + tab + 16u * o + 12u); }
    if (!len || off + len > size || len <= FW_MOD_CODE_OFF || (len & 3u)) { f_close(f); str_cpy(fb.msg, "No image for it", sizeof fb.msg); return 1; }
    if (f_lseek(f, off) != FR_OK || f_read(f, pg, sizeof pg, &got) != FR_OK || got != sizeof pg) { f_close(f); str_cpy(fb.msg, "Unreadable", sizeof fb.msg); return 1; }
    if (dry) { f_close(f); return 0; }
    if (oc_mod_inplace(engine, start) && !armed) {      /* (0.10.1) no other place is free: the old module is erased first and a power cut leaves none.
                                                                                  * Said once; OCT+ on the same row again goes on (fb.ask is the row, as for a pack's question) */
        f_close(f);
        fb.ask = (uint8_t)(fb.sel + 1u);
        fb.setq = (uint8_t)q;                           /* (0.13.3) for a set: which of its modules asked */
        str_cpy(fb.msg, "Erases old first. Again", sizeof fb.msg);
        return 1;
    }
    if (oc_mod_begin(engine, start, len) != 0) { f_close(f); str_cpy(fb.msg, "Install failed", sizeof fb.msg); return 1; }
    for (o = FW_MOD_CODE_OFF; o < len && !rc; o += got) {
        uint32_t k = len - o > sizeof buf ? (uint32_t)sizeof buf : len - o;
        if (f_read(f, buf, k, &got) != FR_OK || got != k || oc_mod_write(o, buf, k) != 0) rc = 1;
        SMP_FEED();
    }
    f_close(f);
    if (rc) { str_cpy(fb.msg, "Install failed", sizeof fb.msg); return 1; }
    rc = oc_mod_end(pg);
    if (rc == -8) { str_cpy(fb.msg, "Update would erase it", sizeof fb.msg); return 1; }   /* (0.10.1) -8 is oc_module.c MI_TAIL: a sector tail like an update record, nothing committed */
    if (rc) { str_cpy(fb.msg, "Install failed", sizeof fb.msg); return 1; }
    return 0;
}
static int fb_install_mod(const fb_ent_t *e)
{
    char p[FB_PATH + FB_NAME + 2u];
    int armed = fb.ask == (uint8_t)(fb.sel + 1u);      /* (0.10.1) the in-place question was asked on this row; any other press drops it */
    fb.ask = 0;
    fb.setq = 0;
    fb_file(p, e->name);
    if (fb_mod_file(p, e->size, armed, 0u, 0, 0)) return 1;
    fb.packed = 2;
    return 0;
}
#if FB_SET
#include "oc_set.h"
#ifndef FB_TEXT
#define FB_TEXT() ((char *)proj_wire_u.raw + 4096u)      /* (0.13.3) a set's manifest, and the parsed set after it, in the borrowed RAM behind the rows */
#define FB_SETS() ((oc_set_t *)(proj_wire_u.raw + 4096u + 2048u))
#endif
#define FB_TEXT_MAX 2048u
#ifndef OC_FW_VERSION
#define OC_FW_VERSION 0u
#endif
/* (0.13.3) the file `name` of the set's folder: its path in p, its size in *size. 0, or 1 and fb.msg "Missing <name>" */
static int fb_set_file(char *p, const char *name, uint32_t *size)
{
    FILINFO fi;
    fb_file(p, name);
    if (f_stat(p, &fi) != FR_OK || (fi.fattrib & AM_DIR)) {
        uint32_t at = fb_put(fb.msg, 0u, "Missing ", 8u);
        at = fb_put(fb.msg, at, name, 15u);
        fb.msg[at] = 0;
        return 1;
    }
    *size = (uint32_t)fi.fsize;
    return 0;
}
/* (0.13.3) "Install set": the manifest at (fb.path, e) read, checked against this firmware (version, then interface identifier, table address, pack hash and
 * length), every file of it found and checked as far as nothing is written (the pack's header, each module's header and the place the unit names), the
 * modules checked in pairs (the 12 sector area holds SHAPES or the Prophet, not both); then the pack (unless this build's is there and sound), then the
 * modules in the order of the manifest, each through fb_mod_file as OCT+ on a .OCM does, with its own in-place question (OCT+ again on the row answers
 * the one that asked). Nothing is kept between the presses but fb.ask and fb.setq: a cut or a question leaves the stages done and the next press does the
 * rest (the pack is skipped, a module is installed again). Only the stages the card can do: the firmware package of the set is not started from here
 * (the update loader takes it from a computer over USB), so a set for another version of the firmware is refused with the words to update it first.
 * 0 done (fb.packed 3), else fb.msg says why. */
static int fb_install_set(const fb_ent_t *e)
{
    char p[FB_PATH + FB_NAME + 2u];
    FIL *f = &uf_pf;
    UINT got;
    oc_set_t *o = FB_SETS();
    uint32_t k, j, size, plen, phash, eng[OC_SET_OCM];
    uint8_t ask0 = fb.ask, q0 = fb.setq;
    fb.ask = 0;
    fb.setq = 0;
    if (song.playing || transport_busy()) { str_cpy(fb.msg, "Stop first", sizeof fb.msg); return 1; }
    if (e->size < 16u || e->size >= FB_TEXT_MAX) { str_cpy(fb.msg, "Not a set", sizeof fb.msg); return 1; }
    fb_file(p, e->name);
    if (f_open(f, p, FA_READ) != FR_OK) { str_cpy(fb.msg, "Unreadable", sizeof fb.msg); return 1; }
    if (f_read(f, FB_TEXT(), e->size, &got) != FR_OK || got != e->size) { f_close(f); str_cpy(fb.msg, "Unreadable", sizeof fb.msg); return 1; }
    f_close(f);
    if (oc_set_parse(FB_TEXT(), e->size, o) != OC_SET_OK) { str_cpy(fb.msg, "Not a set", sizeof fb.msg); return 1; }
    switch (oc_set_vs(o, OC_FW_VERSION, FW_API_IFACE, FW_API_ADDR, OC_PACK_HASH, OC_PACK_LEN)) {
    case OC_SET_NEWER: str_cpy(fb.msg, "Update firmware first", sizeof fb.msg); return 1;
    case OC_SET_OLDER: str_cpy(fb.msg, "Set is for older build", sizeof fb.msg); return 1;
    case OC_SET_OTHER: str_cpy(fb.msg, "Set: another build", sizeof fb.msg); return 1;
    default: break;
    }
    if (fb_set_file(p, o->pak, &size) || fb_pak_file(p, size, 1)) return 1;
    for (k = 0; k < o->nocm; k++) {
        if (fb_set_file(p, o->ocm[k], &size) || fb_mod_file(p, size, 0, 0u, 1, &eng[k])) return 1;
        for (j = 0; j < k; j++)
            if (eng[j] == eng[k] || !oc_mod_pair_fits(eng[j], eng[k])) { str_cpy(fb.msg, "Modules don't fit", sizeof fb.msg); return 1; }
    }
    if (!(oc_pack_info(&plen, &phash) == PK_OK && plen == OC_PACK_LEN && phash == OC_PACK_HASH)) {
        if (fb_set_file(p, o->pak, &size) || fb_pak_file(p, size, 0)) return 1;
    }
    for (k = 0; k < o->nocm; k++) {
        if (fb_set_file(p, o->ocm[k], &size) ||
            fb_mod_file(p, size, ask0 == (uint8_t)(fb.sel + 1u) && q0 == (uint8_t)(k + 1u), k + 1u, 0, 0))
            return 1;
    }
    fb.packed = 3;
    return 0;
}
#endif
#endif
/* OCT+: ".." up, a folder opened, a WAV given to the track's phrase. 1: done (the browser closed) */
static int fb_ok(void)
{
    const fb_ent_t *e;
    if (fb.st == FB_FAIL && fb.relist) { fb_unfail(); return 0; }
    if (fb.st == FB_FAIL) { fb_close(); return 1; }
    if (fb.st != FB_READY || !fb.n) return 0;
    if (fb.up && fb.sel == 0u) { fb_go_up(); return 0; }
    e = fb_ent(fb.sel);
    if (!e) return 0;
    if (e->dir) {
        if (e->longname || str_len(fb.path) + str_len(e->name) + 2u >= FB_PATH) { fb_fail("Path too long"); return 0; }
        if (!str_eq(fb.path, "/")) uh_cat(fb.path + str_len(fb.path), "/");
        uh_cat(fb.path + str_len(fb.path), e->name);
        fb.st = FB_LIST;
        return 0;
    }
    if (FB_PAK && fb_pak(e->name)) {
#if FB_PAK
        if (fb_install(e)) {
            char m[24];
            str_cpy(m, fb.msg, sizeof m);
            fb_fail(m);
            fb.relist = fb.ask ? 2u : 1u;
            return 0;
        }
#endif
        fb_close();
        return 1;
    }
    if (FB_SET && fb_set(e->name)) {
#if FB_SET
        if (fb_install_set(e)) {
            char m[24];
            str_cpy(m, fb.msg, sizeof m);
            fb_fail(m);
            fb.relist = fb.ask ? 2u : 1u;
            return 0;
        }
#endif
        fb_close();
        return 1;
    }
    if (FB_OCM && fb_ocm(e->name)) {
#if FB_OCM
        if (fb_install_mod(e)) {
            char m[24];
            str_cpy(m, fb.msg, sizeof m);
            fb_fail(m);
            fb.relist = fb.ask ? 2u : 1u;
            return 0;
        }
#endif
        fb_close();
        return 1;
    }
    if (fb_assign(fb.trk, e)) {                         /* (refused: its words a moment, the list again) */
        char m[24];
        str_cpy(m, fb.msg, sizeof m);
        fb_fail(m);
        fb.relist = 1;
        return 0;
    }
    fb_close();
    return 1;
}
static int fb_pack_done(void) { return fb.packed; }      /* (0.8.0) the browser's last close was a pack installed */
