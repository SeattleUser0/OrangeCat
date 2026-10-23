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
 * Revisions:
 *   0.6.1: first version.
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
#define FB_NAME 40u
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
        if ((fi.fattrib & (AM_HID | AM_SYS)) || fi.fname[0] == '.' || (!dir && !fb_wav(fi.fname))) continue;
        str_cpy(e[n].name, fi.fname, FB_NAME);
        e[n].dir = (uint8_t)dir;
        e[n].longname = (uint8_t)(str_len(fi.fname) >= FB_NAME);
        e[n].size = (uint32_t)fi.fsize;
        for (i = n; i > 0u && fb_before(&e[n], &e[i - 1u]); i--)   /* (in order as they come) */
            ;
        if (i < n) {
            fb_ent_t t = e[n];
            memmove(&e[i + 1u], &e[i], (n - i) * sizeof *e);
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
    if (fb.st != FB_READY || !fb.n || !d) return;
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
    if (fb.st == FB_FAIL && fb.relist) { fb.relist = 0; fb.st = FB_LIST; return 0; }
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
    if (ocl_keep(k, b)) { str_cpy(fb.msg, "Phrases full", sizeof fb.msg); return 1; }   /* (the steps kept: a number) */
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
/* OCT+: ".." up, a folder opened, a WAV given to the track's phrase. 1: done (the browser closed) */
static int fb_ok(void)
{
    const fb_ent_t *e;
    if (fb.st == FB_FAIL && fb.relist) { fb.relist = 0; fb.st = FB_LIST; return 0; }
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
