/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 * Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Editor protocol: SysEx for the web editor (web/EDITOR_PROTOCOL.md; v2 = user presets + live sync,
 * v3 = four tracks: the v1 / v2 commands act on the selected track, cmds 27-30 reach any track;
 * v4 = TRACK_PARAM (31) and the TRACK_CHANGED push (32), enabled by WATCH bit 1).
 *   F0 7D 46 4C cmd args.. F7     (7D = non-commercial ID, "FL")
 * Values are 14 bit, two 7-bit bytes LSB first, offset by 8192 (so -8192..8191).
 * Every request gets a reply with the same cmd; 23/24/26 are also pushed
 * while watched. Frames arrive through sx_frame (usb.c), replies leave
 * through ota_wire_send().
 * Revisions:
 * (Orange Cat 0.9.1) Library in groups with a shared spare sector (0.9.1)
 * (Orange Cat 0.10.1) module sector tails against the update loader's record rule
 * (Orange Cat 0.11.0) admission at assignment (voice units and arena; cost word in the module header, carried)
 * (Orange Cat 0.12.0) a module's pages as data (fw_pages.h), the Prophet's number is a module slot
 * (Orange Cat 0.12.1) the Prophet as a module at 8 sectors, three module starts, a module of another engine in the way is refused and named
 *   0.12.1: MOD_INFO (120) reports three starts and ends with the blocker's start (127: none), a module of another engine that stands in the way of the engine asked about.
 * (Orange Cat 0.14.2) command 102 ARRANGE (editor_arrange.c): the chains, the song and the scenes of the music loaded, read and written whole by the web app.
 * (Orange Cat 0.15.0) the requests and replies counted by command (AUDIO_STATS [flags, 5]): a long run can tell a reply the unit never sent
 *   from one the path lost; the cost caps' readout (AUDIO_STATS [flags, 6], voice.c). */
#define ED_HDR0 0x7D
#define ED_HDR1 0x46
#define ED_HDR2 0x4C
enum { ED_INFO = 1, ED_GET, ED_SET, ED_DUMP, ED_DESC, ED_STEP_GET, ED_STEP_SET, ED_PRESET, ED_PROJECT, ED_NAMES,
       ED_SMP_BEGIN, ED_SMP_WRITE, ED_SMP_END, ED_SMP_ERASE, ED_SMP_INFO,
       ED_UP_LIST, ED_UP_GET, ED_UP_PUT, ED_UP_STORE, ED_UP_LOAD, ED_UP_ERASE,   /* v2: user presets */
       ED_WATCH, ED_CHANGED, ED_RELOAD, ED_PING, ED_STEP_CHANGED,              /* v2: live sync */
       ED_TRACK, ED_TRACK_MIX, ED_TRACK_DUMP, ED_TRACK_STEP,                    /* v3: tracks */
       ED_TRACK_PARAM, ED_TRACK_CHANGED, ED_SONG,
       ED_UI_STATE, ED_UI_SET, ED_UI_PALETTES, ED_FAV_GET, ED_FAV_SET,
       ED_MOTION = 64, ED_BACKUP_LIST, ED_BACKUP_GET, ED_BACKUP_PUT,                               /* v6: song chain */
       ED_AUDIO_STATS = 72, ED_PATTERN, ED_BANK_SONG };                                       /* USB audio diagnostics (68..71: FM6, editor_fm6.c) */
/* (Orange Cat 0.6.0) the internal user sample slot (smp_slot.c). New ids: the old ED_SMP_* (11..15) wrote at 0xA0000,
 * over what is now the record store, and stay retired */
enum { ED_SLOT_INFO = 110, ED_SLOT_BEGIN, ED_SLOT_WRITE, ED_SLOT_END, ED_SLOT_ERASE };   /* (80: the library, 95..101 taken) */
/* (Orange Cat 0.8.0) the asset pack (oc_pack.c): SHAPES' and MACRO's tables, installed into flash. 115..119 (web/EDITOR_PROTOCOL.md) */
enum { ED_PACK_INFO = 115, ED_PACK_BEGIN, ED_PACK_WRITE, ED_PACK_END, ED_PACK_ERASE };
/* (Orange Cat 0.10.0) loadable engine modules (oc_module.c): 120..124. The pack's ids and payloads are unchanged; the module has its own ids because the pack's
 * BEGIN already carries an optional byte (older pages sent one), so an area selector in it would be ambiguous. */
enum { ED_CLOCK_STATS = 103 };                      /* (Orange Cat 0.14.2) the external clock's counters (midi_clock.c) */
enum { ED_HW_TEST = 108 };                          /* (Orange Cat 0.13.8) the hardware test surface (oc_hw_test.c), OC_HW_TEST builds only */
enum { ED_MOD_INFO = 120, ED_MOD_BEGIN, ED_MOD_WRITE, ED_MOD_END, ED_MOD_REMOVE, ED_ENGINES,   /* (0.11.0) 125 ED_ENGINES: the engine list */
       ED_PAGES, ED_PANEL };      /* (0.12.0) 126 ED_PAGES, 127 ED_PANEL: a module engine's pages and native parameters (fw_pages.h) */
/* (Orange Cat 0.14.2) commands past 127: command 0 is the extension, its first byte n names command 128 + n (every byte stays 7 bit); the reply
 * has the same 0 and n. 128 KIT_GET, 129 KIT_PUT: a DRUM-X track's kit (web/EDITOR_PROTOCOL.md "DRUM-X kits") */
enum { ED_EXT = 0, ED_KIT_GET = 128, ED_KIT_PUT = 129 };
#define ED_KIT_LANE 9u                                  /* a lane's values on the wire: LEVEL TUNE DECAY PAN LAYER FDBK PUNCH ATK+ BEND */
#define ED_KIT_BODY (5u + 8u * ED_KIT_LANE)             /* base, TONE, FDBK, HANDS, DUCK, then the lanes */
#if MELODEE_CORE1_TEST && MELODEE_DUAL_CORE
enum { ED_CORE1_TEST = 79 };                         /* test builds only: CPU1 failure injection */
static void core1_test_stall(void *unused) { (void)unused; for (;;) ; }
#endif

static uint8_t ed_out[1024];                        /* the longest: a NAMES page (PROPHET pages its 201) */
static uint32_t ed_n;
/* (0.15.0) by command byte: frames with the editor's header handled (ed_handle), replies and pushes put whole into USB's SysEx queue
 * (ed_send); replies cut short (the queue full for 200 ms: usb.c ota_wire_send). Modulo 65,536: a run reads them before and after */
static uint16_t ed_req_n[128], ed_rep_n[128];
static uint32_t ed_rep_cut;

static void ed_begin(uint32_t cmd)
{
    ed_out[0] = 0xF0;
    ed_out[1] = ED_HDR0;
    ed_out[2] = ED_HDR1;
    ed_out[3] = ED_HDR2;
    ed_out[4] = (uint8_t)cmd;
    ed_n = 5;
}
static void ed_b(uint32_t v)
{
    if (ed_n < sizeof ed_out - 1u)
        ed_out[ed_n++] = (uint8_t)(v & 0x7Fu);
}
static void ed_v(int32_t v)
{
    uint32_t u = (uint32_t)(clamp(v, -8192, 8191) + 8192);
    ed_b(u);
    ed_b(u >> 7);
}
static void ed_str(const char *s, uint32_t max)   /* ASCII, 0-terminated */
{
    uint32_t i;
    for (i = 0; s && s[i] && i < max; i++)
        ed_b((uint8_t)s[i] & 0x7Fu);
    ed_b(0);
}
static void ed_send(void)
{
    ed_out[ed_n++] = 0xF7;
    if (ota_wire_send(ed_out, ed_n) == 0)
        ed_rep_n[ed_out[4] & 127u]++;
    else
        ed_rep_cut++;
}
static int32_t ed_rv(const uint8_t *p) { return (int32_t)(p[0] | p[1] << 7) - 8192; }

/* 7-bit-safe payload decoding for project/archive transfers. */
static uint32_t ed_unpack7(const uint8_t *a, uint32_t na, uint8_t *out, uint32_t max)
{
    uint32_t n = 0;                                 /* groups: msb byte, then up to 7 bytes */
    while (na && n < max) {
        uint32_t m = *a++, j;
        na--;
        uint32_t k = na > 7u ? 7u : na;
        if (!k || n + k > max || (m >> k))
            return 0;                              /* incomplete group, overflow, or unused mask bits */
        for (j = 0; j < k; j++, na--)
            out[n++] = (uint8_t)(*a++ | ((m >> j) & 1u) << 7);
    }
    return na ? 0u : n;
}
static uint8_t ed_smp_buf[512] __attribute__((aligned(4)));
/* the engine byte of DUMP / RELOAD / TRACK */
static uint32_t ed_eng(const track_t *t) { return t->eng_req % NENGINES; }

/* ---- live sync (v2): while the editor WATCHes, device-side changes are pushed.
 * Shadows of the selected track's p[] + song.g[] and of its steps are kept in step
 * with what the editor knows (its own SET / STEP_SET update them); the main loop
 * compares and pushes CHANGED / STEP_CHANGED, or RELOAD after a load or when another
 * track was selected (v3: RELOAD and STEP_CHANGED carry the selected track). Pushes
 * go out only into a half-empty SysEx ring, so they never wait. */
#define ED_PUSH_MAX 4u                                   /* frames per pass */
#define ED_NV (P_COUNT + G_COUNT)
/* v4: the parameters of the tracks that are not selected which TRACK_CHANGED follows (the mixer) */
static const uint8_t ED_TIDS[3] = {P_LEVEL, P_PAN, P_MUTE};
#define ED_NT (NTRK * 3u)
static struct {
    uint8_t on, eng, preset, sel;
    uint8_t v4;                                          /* WATCH bit 1: TRACK_CHANGED pushes too */
    uint32_t pos, last_ms, run_ms, resets;
    int16_t v[ED_NV];                                    /* TSEL->p[], then song.g[] */
    uint16_t t[ED_NV];                                   /* ms (low 16 bits) of the last push */
    uint32_t st[NSTEP];                                  /* step signatures */
    int16_t tv[ED_NT];                                   /* v4: trk[k].p[ED_TIDS[j]] at k * 3 + j */
    uint16_t tt[ED_NT];
    uint32_t tpos, pattern_gen;
} ed_w;

static int16_t *ed_val(uint32_t i)
{
    return i<P_COUNT?&TSEL->p[i]:i==P_COUNT+G_A4?&tuning_a4:i==P_COUNT+G_BUDGET?&settings_budget:&song.g[i-P_COUNT];
}
static uint32_t ed_step_sig(const step_t *s)
{
    return ((uint32_t)s->note[0] | (uint32_t)s->note[1] << 7 | (uint32_t)s->note[2] << 14 | (uint32_t)s->note[3] << 21) ^
           ((uint32_t)s->n << 28) ^ ((uint32_t)s->time * 0x9E3779B1u) ^ ((uint32_t)s->flags * 0x85EBCA6Bu) ^
           ((uint32_t)s->vel * 0xC2B2AE35u) ^ ((uint32_t)(s->hit | s->acc << 8) * 0x27D4EB2Fu) ^ ((uint32_t)s->probability * 0x165667B1u);
}
static void ed_shadow(void)                              /* the editor is in sync */
{
    uint32_t i;
    for (i = 0; i < ED_NV; i++)
        ed_w.v[i] = *ed_val(i);
    for (i = 0; i < NSTEP; i++)
        ed_w.st[i] = ed_step_sig(&seq_steps(TSEL)[i]);
    for (i = 0; i < ED_NT; i++)
        ed_w.tv[i] = trk[i / 3u].p[ED_TIDS[i % 3u]];
    ed_w.eng = (uint8_t)ed_eng(TSEL);
    ed_w.preset = TSEL->preset;
    ed_w.sel = song.sel; ed_w.pattern_gen = TSEL->pattern_gen;
    sync_reload = 0;
}
static int ed_room(void) { return so_w - so_r + 8u <= SXQ / 2u; }
static void ed_known(uint32_t k, uint32_t id)            /* the editor's own change of trk[k].p[id]: no push */
{
    uint32_t j;
    if (k == song.sel) {
        ed_w.v[id] = trk[k].p[id];
        return;
    }
    for (j = 0; j < 3u; j++)
        if (ED_TIDS[j] == id)
            ed_w.tv[k * 3u + j] = trk[k].p[id];
}

static void ed_sync(void)                                /* main loop */
{
    uint32_t i, n = 0, now = fm1_ms;
    if (!ed_w.on || now - ed_w.run_ms < 5u)
        return;
    ed_w.run_ms = now;
    if (!usb.config || usb.resets != ed_w.resets || now - ed_w.last_ms > 3000u) {
        ed_w.on = 0;                                     /* no host, USB reset, or 3 s without a request */
        return;
    }
    if (sync_reload || TSEL->pattern_gen != ed_w.pattern_gen || ed_eng(TSEL) != ed_w.eng || TSEL->preset != ed_w.preset || song.sel != ed_w.sel) {
        if (!ed_room())
            return;
        ed_shadow();
        ed_begin(ED_RELOAD);
        ed_b(ed_eng(TSEL));
        ed_b(TSEL->preset);
        ed_b(song.sel);
        ed_b(TSEL->preset >> 7);                     /* presets past 127 (PROPHET): high bits last */
        ed_send();
        return;
    }
    for (i = 0; i < NSTEP && n < ED_PUSH_MAX; i++) {
        uint32_t h = ed_step_sig(&seq_steps(TSEL)[i]);
        if (h == ed_w.st[i])
            continue;
        if (!ed_room())
            return;
        ed_w.st[i] = h;
        ed_begin(ED_STEP_CHANGED);
        ed_b(i);
        ed_b(song.sel);
        ed_send();
        n++;
    }
    for (i = 0; i < ED_NV && n < ED_PUSH_MAX; i++) {    /* round robin: nothing starves */
        uint32_t k = (ed_w.pos + i) % ED_NV;
        int16_t v = *ed_val(k);
        if (v == ed_w.v[k] || (uint16_t)(now - ed_w.t[k]) < 20u)
            continue;                                    /* coalesced: the latest value goes out later */
        if (!ed_room())
            return;
        ed_w.v[k] = v;
        ed_w.t[k] = (uint16_t)now;
        ed_begin(ED_CHANGED);
        ed_b(k < P_COUNT ? 0u : 1u);
        ed_b(k < P_COUNT ? k : k - P_COUNT);
        ed_v(v);
        ed_send();
        n++;
        ed_w.pos = k + 1u;
    }
    for (i = 0; ed_w.v4 && i < ED_NT && n < ED_PUSH_MAX; i++) {   /* v4: the other tracks' mix */
        uint32_t k = (ed_w.tpos + i) % ED_NT, tr = k / 3u, id = ED_TIDS[k % 3u];
        int16_t v = trk[tr].p[id];
        if (tr == song.sel || v == ed_w.tv[k] || (uint16_t)(now - ed_w.tt[k]) < 20u)
            continue;                                    /* (the selected track: CHANGED above) */
        if (!ed_room())
            return;
        ed_w.tv[k] = v;
        ed_w.tt[k] = (uint16_t)now;
        ed_begin(ED_TRACK_CHANGED);
        ed_b(tr);
        ed_b(id);
        ed_v(v);
        ed_send();
        n++;
        ed_w.tpos = k + 1u;
    }
}

/* descriptor of parameter id of track t: the engine parameters of the engine it asked for
 * (t->engine follows in the audio ISR, after a short fade) */
static const param_desc_t *ed_tdesc(const track_t *t, uint32_t id) { return param_desc_of(t->eng_req % NENGINES, id); }
/* descriptor and value slot of (scope, id): scope 0 = the selected track, 1 = global */
static const param_desc_t *ed_desc(uint32_t scope, uint32_t id, int16_t **vp)
{
    if (scope == 0 && id < P_COUNT) {
        *vp = &TSEL->p[id];
        return ed_tdesc(TSEL, id);
    }
    if (scope == 1 && id < G_COUNT) {
        *vp = ed_val(P_COUNT + id);
        return global_desc(id);
    }
    return 0;
}

/* a step as STEP_SET / TRACK_STEP send it (na bytes): n, 4 notes, time, flags, vel, then (v5) the lane hits
 * and their accents: hit & 127, acc & 127, bit 7 of each (bit 0 hit, bit 1 acc); each value made valid. 8
 * bytes (an editor of before the grid): the hits stay */
static void ed_step_put(step_t *st, const uint8_t *a, uint32_t na)
{
    uint32_t i, gate = step_gate(st);
    st->n = (uint8_t)(a[0] > 4u ? 4u : a[0]);
    for (i = 0; i < 4u; i++)
        st->note[i] = a[1 + i] & 0x7Fu;
    st->time = (uint8_t)(a[5] > ST_REST ? ST_REST : a[5]);
    /* (Orange Cat 0.2) RATCH and FLAM, which this message does not carry yet, stay as they were (phase 4g) */
    st->flags = (uint8_t)((st->flags & (SF_RATCH | SF_FLAM)) | (a[6] & (SF_ACCENT | SF_SLIDE)));
    st->vel = a[7] & 0x7Fu;
    if (na >= 11u) {
        st->hit = (uint8_t)((a[8] & 0x7Fu) | (a[10] & 1u) << 7);
        st->acc = st->hit ? (uint8_t)(((a[9] & 0x7Fu) | (a[10] & 2u) << 6) & st->hit) : (uint8_t)gate;
    }
    if (!st->hit && st->time == ST_REST) st->acc = 0;
    if (na >= 12u) step_set_chance(st, a[11] <= 100u ? a[11] : 100u);
    ui.force = 1;
}
static void ed_step_reply(const step_t *st)              /* the same 11 bytes */
{
    uint32_t i;
    ed_b(st->n);
    for (i = 0; i < 4u; i++)
        ed_b(st->note[i]);
    ed_b(st->time);
    ed_b(st->flags & (SF_ACCENT | SF_SLIDE));
    ed_b(st->vel);
    ed_b(st->hit & 0x7Fu);
    ed_b(st->hit ? st->acc & 0x7Fu : 0u);
    ed_b((uint32_t)(st->hit >> 7) | (st->hit ? (uint32_t)(st->acc >> 7) << 1 : 0u));
    ed_b(step_chance(st));
}

/* a flash write from the editor while the transport runs: stop it first (the erase silences the audio and
 * stalls the sequencer; the device's own SAVE refuses with "STOP TO SAVE", the editor's confirmed save stops) */
static int ed_flash_stop(void)
{
    uint32_t t0 = fm1_ms;
    if (!transport_busy())
        return 0;
    transport_req = 2;
    while ((song.playing || seq_counting() || chain_busy()) && fm1_ms - t0 < 100u)          /* the audio ISR stops it at its next block */
        fm1_wdt_feed();
    return song.playing || seq_counting() || chain_busy();
}

#include "editor_preferences.c"
#include "editor_backup.c"
#include "editor_arrange.c"                            /* (Orange Cat 0.14.2) command 102 ARRANGE: the chains, the song, the scenes, whole (web/app) */
#include "editor_fm6.c"
#include "editor_cz.c"
#include "editor_native.c"
#include "editor_library.c"
#include "editor_prophet.c"
#include "editor_cpu_lab.h"
#if OC_HW_TEST
#include "oc_hw_test.c"                                   /* (Orange Cat 0.13.8) command 108, test builds only */
#endif

static void ed_motion_reply(uint32_t k, uint32_t rc)
{
    track_t *t = &trk[k];
    ed_b(k); ed_b(rc); ed_b(motion_enabled(t)); ed_b(motion_count(t)); ed_b(MOTION_MAX);
    for (uint32_t i = 0; i < motion.count; i++) {
        const motion_event_t *e = &motion.event[i];
        if ((e->place >> 6) != k || motion_pattern[i] != t->pattern) continue;
        ed_b((e->place & 63u) | ((e->param & 128u) >> 1)); ed_b(e->param); ed_v(e->value);
    }
}
static int ed_args_ok(uint32_t cmd, const uint8_t *a, uint32_t n)
{
    switch (cmd) {
    case ED_INFO: case ED_DUMP: case ED_SMP_INFO: case ED_PING:
    case ED_UI_STATE: case ED_UI_PALETTES: case ED_PACK_INFO:
        return !n;
    case ED_MOD_INFO:
        return n <= 1u;
    case ED_ENGINES:
        return !n;
    case ED_PAGES:
        return n == 1u;
    case ED_PANEL:
        return n == 1u || n == 3u;
    case ED_GET: case ED_DESC: case ED_PRESET: case ED_PROJECT: case ED_UP_LIST:
    case ED_TRACK_PARAM:
        return n == 2u || (cmd == ED_TRACK_PARAM && n == 4u) || (cmd == ED_PRESET && n == 3u);
    case ED_SET:
        return n == 4u;
    case ED_STEP_GET: case ED_NAMES: case ED_SMP_BEGIN: case ED_SMP_ERASE:
    case ED_UP_GET: case ED_UP_LOAD: case ED_UP_ERASE: case ED_WATCH: case ED_TRACK_DUMP:
        return n == 1u || (cmd == ED_NAMES && n == 3u);
    case ED_STEP_SET:
        return n == 9u || n == 12u || (n == 13u && a[12] <= 100u);                 /* notes only, or the complete grid extension */
    case ED_TRACK:
        return n <= 1u;
    case ED_TRACK_MIX:
        return n == 1u || n == 4u;
    case ED_TRACK_STEP:
        return n == 2u || n == 10u || n == 13u || (n == 14u && a[13] <= 100u);
    case ED_MOTION:
        return (n == 1u || (n == 2u && a[1] == 2u) || (n == 3u && a[1] == 1u && a[2] <= 1u) ||
                (n == 6u && a[1] == 3u) || ((n == 4u || n == 6u) && a[1] == 4u)) && a[0] < NTRK;
    case ED_BANK_SONG: case ED_SONG:
        return n && (a[0] == 1u || n == 1u);
    default:
        return 1;                                  /* variable payloads validate in their handler */
    }
}

/* (Orange Cat 0.14.2) KIT_GET track -> track, rc (0; 1 not a DRUM-X track), on, then the body: base, TONE (BP), FDBK, HANDS, DUCK, and per lane
 * (KICK .. BELL) LEVEL, TUNE + 64, DECAY + 64, PAN + 64, LAYER, its FDBK, PUNCH, ATK+, BEND + 64. KIT_PUT track, the body -> track, rc (0; 1 not a
 * DRUM-X track; 2 a value out of range: nothing changed): the kit as an edited one (on 1), the LEVELs into P_LN0..7. Any other length: no reply */
static int ed_kit_handle(uint32_t cmd, const uint8_t *a, uint32_t na)
{
    uint32_t tr, l;
    dxk_t k;
    if (cmd == ED_KIT_GET) {
        if (na != 1u) return 0;
        tr = a[0];
        ed_b(tr);
        if (tr >= NTRK || !(MELODEE_DRUMX && trk[tr].eng_req == ENGI_DRUMX)) { ed_b(1); return 1; }
        dxk_view(tr, &k);
        ed_b(0); ed_b(dxk[tr].on); ed_b(k.base); ed_b(k.tone & 15u); ed_b(k.tone >> 4); ed_b(k.hands); ed_b(k.duck);
        for (l = 0; l < 8u; l++) {
            ed_b((uint32_t)clamp(trk[tr].p[P_LN0 + l], 0, 127)); ed_b((uint32_t)(k.tune[l] + 64)); ed_b((uint32_t)(k.decay[l] + 64));
            ed_b((uint32_t)(k.pan[l] + 64)); ed_b(k.lay[l] & 15u); ed_b(k.lay[l] >> 4); ed_b(k.punch[l]); ed_b(k.atk[l]); ed_b((uint32_t)(k.bend[l] + 64));
        }
        return 1;
    }
    if (cmd == ED_KIT_PUT) {
        const uint8_t *d = a + 1, *L;
        int bad = 0;
        if (na != 1u + ED_KIT_BODY) return 0;
        tr = a[0];
        ed_b(tr);
        if (tr >= NTRK || !(MELODEE_DRUMX && trk[tr].eng_req == ENGI_DRUMX)) { ed_b(1); return 1; }
        bad |= d[0] >= DXK_NKIT || d[1] > 15u || d[2] > 15u || d[3] > 15u || d[4] > DXK_DUCK_MAX;
        for (l = 0, L = d + 5; l < 8u; l++, L += ED_KIT_LANE)
            bad |= L[4] > 15u || L[5] > 15u || L[6] > 15u || L[7] > 15u || L[8] < 64u - DXK_BEND_MAX || L[8] > 64u + DXK_BEND_MAX;
        if (bad) { ed_b(2); return 1; }
        memset(&k, 0, sizeof k);
        k.on = 1; k.base = d[0]; k.tone = (uint8_t)(d[1] | d[2] << 4); k.hands = d[3]; k.duck = d[4];
        for (l = 0, L = d + 5; l < 8u; l++, L += ED_KIT_LANE) {
            k.tune[l] = (int8_t)((int)L[1] - 64); k.decay[l] = (int8_t)((int)L[2] - 64); k.pan[l] = (int8_t)((int)L[3] - 64);
            k.lay[l] = (uint8_t)(L[4] | L[5] << 4); k.punch[l] = L[6]; k.atk[l] = L[7]; k.bend[l] = (int8_t)((int)L[8] - 64);
        }
        dxk_put(tr, &k);
        for (l = 0, L = d + 5; l < 8u; l++, L += ED_KIT_LANE)
            trk[tr].p[P_LN0 + l] = (int16_t)L[0];
        ui.force = 1;
        ed_b(0);
        return 1;
    }
    return 0;
}

static void ed_handle(const uint8_t *f, uint32_t n)   /* f: the bytes between F0 and F7 */
{
    if (n < 4u)
        return;
    uint32_t cmd = f[3], i;
    const uint8_t *a = f + 4;
    ed_req_n[cmd & 127u]++;
    uint32_t na = n - 4u;
    int16_t *vp;
    const param_desc_t *d;
    int adm = 0;                                          /* (0.11.0) the admission result of an engine change (eng_admit) */
    if (cmd == ED_EXT) {                                 /* (0.14.2) the extension: command 128 + a[0]; unknown, or a bad length: no reply */
        if (!na) return;
        ed_begin(ED_EXT);
        ed_b(a[0]);
        if (ed_kit_handle(128u + a[0], a + 1, na - 1u)) ed_send();
        return;
    }
    if (!ed_args_ok(cmd, a, na))
        return;
    if(cmd==ED_SET || (cmd==ED_TRACK_PARAM && na==4u) || cmd==ED_FM6_PUT || cmd==ED_CZ_PUT || (cmd==95u && na && a[0]>=1u && a[0]<=3u))momentary_restore();
    ed_begin(cmd);
    if(MELODEE_P5 && ed_prophet_handle(cmd,a,na)){ed_send();return;}   /* (Orange Cat: MELODEE_P5) */

    if (ed_ui_handle(cmd, a, na)) { ed_send(); return; }
    if (ed_backup_handle(cmd, a, na)) { ed_send(); return; }
    if (ed_arrange_handle(cmd, a, na)) { ed_send(); return; }   /* (Orange Cat 0.14.2) */
    if (MELODEE_FM6 && ed_fm6_handle(cmd, a, na)) { ed_send(); return; }   /* (Orange Cat: MELODEE_FM6) */
    if (MELODEE_CZ && ed_cz_handle(cmd, a, na)) { ed_send(); return; }   /* (Orange Cat: MELODEE_CZ) */
    if (ed_library_handle(cmd,a,na)) { ed_send(); return; }
#if OC_HW_TEST
    if (cmd == ED_HW_TEST && ed_hwt_handle(cmd, a, na)) { ed_send(); return; }   /* (0.13.8) test builds only */
#endif
    if (ed_native_handle(cmd, a, na)) { ed_send(); return; }
    switch (cmd) {
#if MELODEE_AUDIO_PROFILE && MELODEE_AUDIO_BENCH
    case 101:
        ed_cpu_reply(a, na);
        break;
#endif
    case 100: {
#ifdef FM1_IRQ_TARGET
        if(na){ed_b(1);break;}
        uint32_t values[8u+NPART];fm1_irq_off();
        values[0]=fm1_clock_mhz_of(fm1_clock_mult());values[1]=audio_timer5_max_ticks;values[2]=audio_irq_max_ticks;
        values[3]=MELODEE_AUDIO_PROFILE;values[4]=MELODEE_AUDIO_BENCH;values[5]=MELODEE_ADMISSION;values[6]=HALF_FRAMES;
        for(uint32_t p=0;p<NPART;p++)values[7u+p]=trk_nvoice(&trk[p]);
        values[7u+NPART]=(uint32_t)settings_budget;
        fm1_irq_on();ed_b(0);ed_b(2);ed_b(8u+NPART);
        for(uint32_t k=0;k<8u+NPART;k++)for(uint32_t j=0;j<5u;j++)ed_b(values[k]>>(7u*j));
#else
        ed_b(2);
#endif
        break;
    }
    case 98: {
#if MELODEE_AUDIO_PROFILE
#if MELODEE_AUDIO_BENCH
#if MELODEE_DUAL_CORE && MELODEE_BATCH_AUDIO && !MELODEE_LEGACY_EXTRAS
        if(na && a[0]==7u){
            if(na>2u || (na==2u && a[1]>1u)){ed_b(1);break;}
            if(na==2u){if(!AUDIO_LAB_GUARDED()){ed_b(3);break;}fx_reverb_parallel_enabled=a[1];}
            ed_b(0);ed_b(7);ed_b(fx_reverb_parallel_enabled);break;
        }
#endif
        /* Guarded, RAM-only scheduler calibration. One group's split is
         * fixed at publication; the next group sees a new allowance. */
        if(na && a[0]==5u){
            if((na!=1u && na!=6u) || (na==6u && (a[5]>15u || (ed_bk_r32(a+1u)>31000u && ed_bk_r32(a+1u)!=AUDIO_BATCH_SERVICE_AUTO)))){ed_b(1);break;}
            if(na==6u){
                if(!AUDIO_LAB_GUARDED()){ed_b(3);break;}
                audio_batch_service_allowance=ed_bk_r32(a+1u);
            }
            ed_b(0);ed_b(5);ed_bk_u32(audio_batch_service_allowance);break;
        }
#endif
        if(na==1u && a[0]==4u){
            /* Actual committed headers, including settings (backup inventory
             * exports live preferences). Reads validate object CRCs as usual. */
            ed_b(0);ed_b(4);ed_b(OBJ_LEGACY_COUNT);
            for(uint32_t id=0;id<OBJ_LEGACY_COUNT;id++){
                /* (0.9.1) the ids are the object numbers they always were; a library bank answers from the group */
                uint32_t lb=lg_legacy_bank(id),obj=lb<LG_BANKS?OBJ_LIB0+lb:id;
                st_hdr_t h;int cur=st_current(obj,&h);
                ed_b(cur>=0);
                ed_bk_u32(cur>=0?h.len:0);ed_bk_u32(cur>=0?h.crc:0);ed_bk_u32(cur>=0?h.seq:0);
                fm1_wdt_feed();
            }
            break;
        }
        if(na && a[0]==3u){
            static uint8_t original_slot;
            if(na>2u || (na==2u && a[1]>1u)){ed_b(1);break;}
            if(na==2u){
                if(a[1] && !audio_lab_guard){
                    uint32_t busy=transport_busy() || momentary.active || template_old();
                    for(uint32_t p=0;p<NPART;p++)for(uint32_t v=0;v<NVOICE;v++)busy|=trk[p].v[v].active;
                    if(busy){ed_b(3);break;}
                    original_slot=proj_cur;
                }
                if(!a[1] && audio_lab_guard)proj_cur=original_slot;
                audio_lab_guard=a[1];
            }
            ed_b(0);ed_b(3);ed_b(audio_lab_guard);break;
        }
        if(na==1u && a[0]==2u){
            uint32_t hist[AP_HIST_BINS],peak;
            fm1_irq_off();memcpy(hist,audio_profile_hist,sizeof hist);peak=audio_profile_irq_max;fm1_irq_on();
            ed_b(0);ed_b(2);ed_b(AP_HIST_BINS);
            for(uint32_t j=0;j<5u;j++)ed_b(AP_HIST_TICKS>>(7u*j));
            for(uint32_t j=0;j<5u;j++)ed_b(peak>>(7u*j));
            for(uint32_t k=0;k<AP_HIST_BINS;k++)for(uint32_t j=0;j<5u;j++)ed_b(hist[k]>>(7u*j));
            break;
        }
        uint32_t values[AP_COUNT*3u+5u];
        if(na>1u || (na && a[0]>1u)){ed_b(1);break;}
        fm1_irq_off();
        for(uint32_t k=0;k<AP_COUNT;k++){
            values[k*3u]=audio_profile[k].ticks;values[k*3u+1u]=audio_profile[k].calls;values[k*3u+2u]=audio_profile[k].max_ticks;
        }
#if MELODEE_DUAL_CORE
        values[AP_COUNT*3u]=audio_worker.job_ticks;values[AP_COUNT*3u+1u]=audio_worker.wait_ticks;
        values[AP_COUNT*3u+2u]=audio_worker.jobs;values[AP_COUNT*3u+3u]=audio_worker.waits;
#else
        for(uint32_t k=0;k<4u;k++)values[AP_COUNT*3u+k]=0;
#endif
#ifdef FM1_IRQ_TARGET
        values[AP_COUNT*3u+4u]=fm1_clock_mhz_of(fm1_clock_mult());
#else
        values[AP_COUNT*3u+4u]=0;
#endif
        if(na && a[0]){
            memset(audio_profile,0,sizeof audio_profile);
            memset(audio_profile_hist,0,sizeof audio_profile_hist);
            audio_profile_irq_pending=audio_profile_irq_max=0;
#if MELODEE_DUAL_CORE
            audio_worker.job_ticks=audio_worker.wait_ticks=audio_worker.waits=0;
#endif
        }
        fm1_irq_on();ed_b(0);ed_b(1);ed_b(AP_COUNT);
        for(uint32_t k=0;k<AP_COUNT*3u+5u;k++)for(uint32_t j=0;j<5u;j++)ed_b(values[k]>>(7u*j));
#else
        ed_b(2);
#endif
        break;
    }
#if MELODEE_AUDIO_BENCH
    case 99: {
        if(na==3u && a[0]==2u && a[1]<NTRK && a[2]<=FM6_OPL){
            fm1_irq_off();fm6_fn_set(a[1],FN_ENGINE,a[2]);fm1_irq_on();ed_b(0);break;
        }
        if(na!=1u || a[0]>1u){ed_b(1);break;}
        fm1_irq_off();audio_bench_until=a[0]?fm1_ticks()+30u*24000000u:0;fm1_irq_on();
        ed_b(0);break;
    }
#endif
#if defined(MELODEE_FX_PROFILE) && MELODEE_FX_PROFILE && defined(FM1_IRQ_TARGET)
    case 97: {
        uint32_t rc=0;
        if((na==6u || (na==7u && a[6]<=1u)) && a[0]<=15u && a[1]<FX_COUNT && a[2]<FX_COUNT && a[3]<FX_COUNT){
            fx_profile_request=a[0]?((uint32_t)(na==7u?a[6]:0u)<<30u)|((uint32_t)a[0]<<26u)|((uint32_t)a[5]<<19u)|((uint32_t)a[4]<<12u)|((uint32_t)a[3]<<8u)|((uint32_t)a[2]<<4u)|a[1]:0;
        }else if(na)rc=1;
        uint32_t values[14];fm1_irq_off();values[0]=fx_profile_control;values[13]=fx_hw_status;
        for(uint32_t k=0;k<4u;k++){values[1+k*3]=fx_profile_ticks[k];values[2+k*3]=fx_profile_blocks[k];values[3+k*3]=fx_profile_max[k];}fm1_irq_on();
        ed_b(rc);for(uint32_t k=0;k<14u;k++)for(uint32_t j=0;j<5u;j++)ed_b(values[k]>>(j*7u));
        break;
    }
#endif
#if OC_SFC_PROBE && MELODEE_FLASH && defined(FM1_IRQ_TARGET)
    case 109: {
        /* (Orange Cat 0.13.7, branch linux-hw; OC_SFC_PROBE builds only) the flash's two read paths, read only (hal/fm1_flash.h fl_probe_paths):
         * mode (0..2), flash offset (3 x 7 bits) -> mode, the SFCENC KEY register (3 x 7 bits), then for the plaintext path and for the other
         * path: 32 bytes at the offset (each as two bytes, low 7 bits then the top bit) and the TIMER4 ticks of a cold and of a warm pass over
         * 2 KB (5 x 7 bits each). 127: bad request, 126: an offset outside 0x93000..0xFF000. Stop the transport first. */
        uint8_t got[64];
        uint32_t ticks[4], key, off = na == 4u ? (a[1] | (uint32_t)a[2] << 7 | (uint32_t)a[3] << 14) & ~31u : 0u;
        if (na != 4u || a[0] > 2u) { ed_b(127); break; }
        if (off < 0x93000u || off + 0x1000u > 0x100000u) { ed_b(126); break; }
        fm1_irq_off();
        key = fl_probe_paths(off, a[0], got, ticks);
        fm1_irq_on();
        ed_b(a[0]); ed_b(key & 127u); ed_b((key >> 7) & 127u); ed_b(key >> 14);
        for (uint32_t k = 0; k < 2u; k++) {
            for (uint32_t i = 0; i < 32u; i++) { ed_b(got[k * 32u + i] & 127u); ed_b(got[k * 32u + i] >> 7); }
            ed_bk_u32(ticks[k * 2u]); ed_bk_u32(ticks[k * 2u + 1u]);
        }
        break;
    }
#endif
#if MELODEE_USB_AUDIO
    case ED_AUDIO_STATS: {                         /* flags: 1 resets maxima, 2 voices, 4 cores, 8 memory, 16 part victims, 32 full profile,
                                                        * 64 the Audio readout (0.13.6, schema 8: 23 fields, alone) */
#if OC_CORE_STATS
        if (na == 2u && a[1] == 3u) {
            /* (0.14.3, OC_CORE_STATS builds) [flags, 3]: the batch's voice kernels, schema 10 (core_stats.h): 10, then 5 x 7 bits each: CPU1's
             * stack high-water mark in bytes (0: not painted), the number of ids N, then for each of the N profile ids the batch carries
             * (Prophet, FM6, CZ, SID, 808, 909, MACRO, DRUM-X, SHAPES, WHEEL): the id, then on CPU0 and on CPU1 the kernels run, their mean and
             * max in TIMER4 ticks (24 a microsecond). Flag 1 starts them over (the cores' readout too) and paints CPU1's stack again (when it is
             * idle). (At most 5 + 5 x 2 + 10 x 7 x 5 = 365 bytes: the reply buffer is 1,024) */
            static const uint8_t ids[] = {AP_PROPHET, AP_FM6, AP_CZ, AP_SID, AP_808, AP_909, AP_MACRO, AP_DRUMX, AP_SHAPES, AP_WHEEL, AP_DUB};   /* (Release 2: AP_DUB, DUB TAPE's job) */
            cs_kn_t k2[sizeof ids][2];
            uint32_t k, used = 0;
            fm1_irq_off();
            for (k = 0; k < sizeof ids; k++) { k2[k][0] = cs_k[ids[k]][0]; k2[k][1] = cs_k[ids[k]][1]; }
#if defined(FM1_IRQ_TARGET) && MELODEE_DUAL_CORE
            used = cs_stack_used();
#endif
            if (a[0] & 1u) {
                cs_reset();
#if defined(FM1_IRQ_TARGET) && MELODEE_DUAL_CORE
                cs_stack_paint();
#endif
            }
            fm1_irq_on();
            ed_b(10);
            ed_bk_u32(used);
            ed_bk_u32(sizeof ids);
            for (k = 0; k < sizeof ids; k++) {
                ed_bk_u32(ids[k]);
                for (uint32_t c = 0; c < 2u; c++) {
                    ed_bk_u32(k2[k][c].n);
                    ed_bk_u32(k2[k][c].n ? (uint32_t)(k2[k][c].sum / k2[k][c].n) : 0u);
                    ed_bk_u32(k2[k][c].max);
                }
            }
            break;
        }
        if (na >= 2u && (a[1] == 1u || (a[1] == 2u && na == 3u && a[2] <= 1u))) {
            /* (0.14.2, OC_CORE_STATS builds) [flags, 1]: the cores' readout, schema 9 (core_stats.h): 9, then 5 x 7 bits each: halves, jobs
             * (voices, buses, dub), submits refused, worker retirements, single-core forced, CPU1 online; then for each metric (wall, cpu0,
             * cpu1, voice, bus, dub, wait, tail_wait, tail, idle1) its min, mean, p99 and max in us (32 us bins). Flag 1 starts them over.
             * [flags, 2, s]: single-core forced (s 1) or not (0), then the same reply */
            static cs_metric_t m[CS_N] __attribute__((section(".pool")));
            uint32_t c[8], k, b;
            if (a[1] == 2u) cs_single = a[2];
            fm1_irq_off();
            for (k = 0; k < CS_N; k++) m[k] = cs_m[k];
            c[0] = cs_halves; c[1] = cs_jobs[0]; c[2] = cs_jobs[1]; c[3] = cs_jobs[2]; c[4] = cs_refused; c[5] = cs_retired;
            c[6] = cs_single; c[7] = (uint32_t)audio_worker_online;
            if (a[0] & 1u) cs_reset();
            fm1_irq_on();
            ed_b(9);
            for (k = 0; k < 8u; k++) ed_bk_u32(c[k]);
            for (k = 0; k < CS_N; k++) {
                uint32_t acc = 0, p99 = 0;
                for (b = 0; b < CS_BINS; b++) { acc += m[k].hist[b]; if (m[k].n && (uint64_t)acc * 100u >= (uint64_t)m[k].n * 99u) { p99 = (b + 1u) * 32u; break; } }
                ed_bk_u32(m[k].min / FM1_TICKS_PER_US);
                ed_bk_u32(m[k].n ? (uint32_t)(m[k].sum / m[k].n / FM1_TICKS_PER_US) : 0u);
                ed_bk_u32(p99);
                ed_bk_u32(m[k].max / FM1_TICKS_PER_US);
            }
            break;
        }
#endif
        if (na == 2u && a[1] == 5u) {
            /* (0.15.0) [flags, 5]: the editor's requests and replies, schema 12: 12, then 5 x 7 bits each: frames dropped as they came (one
             * still pending: usb.c), frames too long, replies cut short (USB's queue full 200 ms), the number N of commands that follow;
             * then for each command byte with a count: the byte, its requests handled and its replies sent, 3 x 7 bits each (modulo
             * 65,536). Replies include the pushes (23, 24, 26, 32). The dropped and long counts are of any SysEx (a DX7 bank is "long"); a
             * reply with USB not configured counts as cut. Flag 1 starts them over after this reply. (At most 21 + 128 x 7 = 917
             * bytes) */
            uint32_t k, nc = 0;
            for (k = 0; k < 128u; k++) nc += ed_req_n[k] || ed_rep_n[k];
            ed_b(12);
#if MELODEE_OTA
            ed_bk_u32(sx_rx_dropped); ed_bk_u32(sx_rx_long);
#else
            ed_bk_u32(0); ed_bk_u32(0);
#endif
            ed_bk_u32(ed_rep_cut); ed_bk_u32(nc);
            for (k = 0; k < 128u; k++)
                if (ed_req_n[k] || ed_rep_n[k]) {
                    ed_b(k);
                    ed_b(ed_req_n[k]); ed_b(ed_req_n[k] >> 7); ed_b(ed_req_n[k] >> 14);
                    ed_b(ed_rep_n[k]); ed_b(ed_rep_n[k] >> 7); ed_b(ed_rep_n[k] >> 14);
                }
            if (a[0] & 1u) {
                memset(ed_req_n, 0, sizeof ed_req_n); memset(ed_rep_n, 0, sizeof ed_rep_n); ed_rep_cut = 0;
#if MELODEE_OTA
                fm1_irq_off(); sx_rx_dropped = sx_rx_long = 0; fm1_irq_on();   /* (usb_poll, TIMER5, counts them) */
#endif
            }
            break;
        }
#if OC_LATE_DIAG
        if (na == 2u && a[1] == 9u) {
            /* (round 3, measurement builds) [flags, 9]: 20, then gap min, gap max (ticks), n, late, ring head, 24 gap bins (16 us from 2,800), then 8 late halves x
             * (gap before, gap after, pre, us (microseconds), t5, total) in ticks. Then the largest nested TIMER5 of a half (ticks) and the halves above 200 us. Flag 1 starts over. Copied with the interrupts off, formatted after. */
            uint32_t k, v[5 + 24 + 48 + 2];
            fm1_irq_off();
            v[0] = ld_gmin; v[1] = ld_gmax; v[2] = ld_n; v[3] = ld_late; v[4] = ld_head; v[77] = ld_t5max; v[78] = ld_t5big;
            for (k = 0; k < 24u; k++) v[5 + k] = ld_hist[k];
            for (k = 0; k < 8u; k++) { v[29 + 6 * k] = ld_ring[k].gprev; v[30 + 6 * k] = ld_ring[k].gnext; v[31 + 6 * k] = ld_ring[k].pre; v[32 + 6 * k] = ld_ring[k].us; v[33 + 6 * k] = ld_ring[k].t5; v[34 + 6 * k] = ld_ring[k].total; }
            if (a[0] & 1u) ld_reset();
            fm1_irq_on();
            ed_b(20);
            for (k = 0; k < 5u + 24u + 48u + 2u; k++) ed_bk_u32(v[k]);
            break;
        }
#endif
#if MELODEE_DUB
        if (na == 2u && a[1] == 7u) {
            /* (Release 2) [flags, 7]: DUB TAPE, schema 14: 14, then 5 x 7 bits each: jfx_full (FX FULL 0x0F, DUB FULL 0xF0), the governor's evictions since power-on, then for each of the four tracks: the
             * instance's state (0 off, 1 waiting for its memory, 2 running, 3 ramping out, 4 clearing), its memory complete (0/1), the blocks it holds, the rate
             * divisor (2 or 4), the hold (blocks it will go on running), whether the job ran this block. Flag 1 is not used. */
            uint32_t k;
            ed_b(14);
            fm1_irq_off();
            ed_bk_u32((uint32_t)jfx_full);                  /* (the whole byte: the buses' FX FULL in 0x0F, DUB FULL in 0xF0) */
            ed_bk_u32(dub_gov_fires);
            for (k = 0; k < DUB_N; k++) {
                ed_bk_u32(dub_i[k].state); ed_bk_u32(dub_ready[k]); ed_bk_u32(dub_have[k]); ed_bk_u32(dub_i[k].R); ed_bk_u32(dub_i[k].hold); ed_bk_u32(dub_i[k].run);
            }
            fm1_irq_on();
            break;
        }
#endif
        if ((na == 2u || na == 5u || na == 6u) && a[1] == 6u) {
            /* (0.15.0) [flags, 6]: the cost caps (voice.c voice_cap_room), schema 15 (Release 2; 13 before: three fields a class, no third cap column): 15, then 5 x 7 bits each: engaged now, times
             * engaged, halves engaged, extra UNISON voices refused, the shared effects ran last block, N (3: MODAL, FM6, the Prophet); then
             * for each: voices given up for the cap, the cap with the effects off, with them on, with a DUB TAPE instance running. Flag 1 starts the counts over.
             * [flags, 6, c, off, on] (OC_CORE_STATS builds only, the unit's runs): engine c's caps (0: as built), then the same reply */
            uint32_t k;
#if OC_CORE_STATS
            if ((na == 5u || na == 6u) && a[2] < VC_N) { vc_cap[a[2]][0] = a[3]; vc_cap[a[2]][1] = a[4]; vc_cap[a[2]][2] = na == 6u ? a[5] : 0; }
#endif
            ed_b(15);                                   /* (Release 2: schema 15: the third cap column, four fields a class) */
            fm1_irq_off();
            ed_bk_u32(vc_on); ed_bk_u32(vc_engages); ed_bk_u32(vc_engaged_halves); ed_bk_u32(vc_refused); ed_bk_u32(voice_fx_on);
            ed_bk_u32(VC_N);
            for (k = 0; k < VC_N; k++) {
                ed_bk_u32(vc_steals[k]);
                ed_bk_u32(vc_cap[k][0] ? vc_cap[k][0] : VC_CAP[k][0]);
                ed_bk_u32(vc_cap[k][1] ? vc_cap[k][1] : VC_CAP[k][1]);
                ed_bk_u32(vc_cap[k][2] ? vc_cap[k][2] : VC_CAP[k][2]);
            }
            if (a[0] & 1u) { vc_engages = vc_engaged_halves = vc_refused = 0; for (k = 0; k < VC_N; k++) vc_steals[k] = 0; }
            fm1_irq_on();
            break;
        }
        uint32_t snapshot[68] = {0}, k, count = na && (a[0] & 64u) ? 23u : na && (a[0] & 32u) ? 64u : na && (a[0] & 16u) ? 55u : na && (a[0] & 8u) ? 47u : na && (a[0] & 4u) ? 32u : na && (a[0] & 2u) ? 26u : 20u;
        fm1_irq_off();
        snapshot[0] = ua.play_alt;
        snapshot[1] = ua.cap_alt;
        snapshot[2] = UA_RATE;
        snapshot[3] = UA_RATE;
        snapshot[4] = ua.pw - ua.pr;
        snapshot[5] = ua.cw - ua.cr;
        snapshot[6] = ua.play_underruns;
        snapshot[7] = ua.play_overruns;
        snapshot[8] = ua.cap_underruns;
        snapshot[9] = ua.cap_overruns;
        snapshot[10] = ua.bad_packets;
        snapshot[11] = ua.rx_packets;
        snapshot[12] = ua.tx_packets;
        snapshot[13] = ua.missed_frames;
        snapshot[14] = ua.poll_max_ticks / FM1_TICKS_PER_US;
        snapshot[15] = ua.service_max_ticks / FM1_TICKS_PER_US;
        snapshot[16] = melodee_dbg.late;
        snapshot[17] = ua_feedback();
        snapshot[18] = melodee_dbg.max_us;            /* render time excludes TIMER5; shedding includes it */
        snapshot[19] = song.cpu_q8;
        if (count == 23u) {                           /* (0.13.6) the block, its budget and how many blocks since the reset */
            snapshot[20] = HALF_FRAMES;
            snapshot[21] = AUDIO_DEADLINE_TICKS / FM1_TICKS_PER_US;
            snapshot[22] = audio_halves - audio_stats_base;
        }
        if (count >= 26u) {
            for (uint32_t p = 0; p < NPART; p++) {
                for (uint32_t v = 0; v < NVOICE; v++) {
                    snapshot[20] += trk[p].v[v].active && trk[p].v[v].stage != 4u;
                    snapshot[21] += trk[p].v[v].active && trk[p].v[v].gate;
                }

            }
            snapshot[24] = shed_count;
            snapshot[25] = voice_kills;
        }
#if MELODEE_DUAL_CORE
        if (count >= 32u) {
            snapshot[26] = audio_worker_online;
            snapshot[27] = audio_worker.jobs;
            snapshot[28] = audio_worker.max_job_ticks / FM1_TICKS_PER_US;
            snapshot[29] = audio_worker.max_wait_ticks / FM1_TICKS_PER_US;
            snapshot[30] = audio_worker.timeouts;
            snapshot[31] = fm6_pairs;
            snapshot[22] = audio_worker.rejected;    /* (22, 23: OBXF's, retired) misread requests, */
            snapshot[23] = fm1_core1_fault.count;     /* CPU1 faults: either retires the worker */
        }
#endif
        if (count >= 47u) {
            snapshot[32] = fm1_cache.status;
            snapshot[33] = fm1_cache.con_before;
            snapshot[34] = fm1_cache.data_before;
            snapshot[35] = fm1_cache.instruction_before;
            snapshot[36] = fm1_cache.con_after;
            snapshot[37] = fm1_cache.data_after;
            snapshot[38] = fm1_cache.instruction_after;
            snapshot[39] = fm1_cache.failure_address;
            snapshot[40] = fm1_cache.test_ticks / FM1_TICKS_PER_US;
            snapshot[41] = RESOURCE_CAPACITY;
            snapshot[42] = resource_used();
            snapshot[43] = resource_peak;
            snapshot[44] = resource_failures;
            snapshot[45] = RESOURCE_CACHE_AVAILABLE ? 28672u : 0u;
            for (uint32_t r = 0; r < RES_COUNT; r++)
                if ((uintptr_t)resource[r].ptr >= (uintptr_t)_resource_cache_start &&
                    (uintptr_t)resource[r].ptr < (uintptr_t)_resource_cache_end)
                    snapshot[46] += resource[r].size;
        }
        if (count >= 55u)
            for (uint32_t p = 0; p < NPART; p++) {
                snapshot[47u+p] = shed_part[p];
                snapshot[51u+p] = voice_kills_part[p];
            }
        if(count==64u){
            /* Upper envelope includes measured C entry/exit and allows one
             * TIMER5 preemption during the short assembly footer, plus 4 us
             * for register/timestamp/return overhead at supported clocks. */
            uint32_t upper=(audio_irq_max_ticks+audio_timer5_max_ticks+96u+23u)/24u;
            snapshot[55]=audio_full_max_us>upper?audio_full_max_us:upper;snapshot[56]=audio_full_last_us;
            snapshot[57]=audio_deadline_misses;snapshot[58]=audio_target_exceeded;
            snapshot[59]=audio_batch_pairs;snapshot[60]=capacity_steals;snapshot[61]=admission_rejections;
            snapshot[62]=MELODEE_ADMISSION;
#if MELODEE_AUDIO_BENCH
            snapshot[63]=(uint32_t)audio_bench_active()|(audio_bench_shed_requests<<1u);
#endif
        }
        if (na && (a[0] & 1u)) {
            audio_full_max_us=0;
            audio_irq_max_ticks=0;
            audio_timer5_max_ticks=0;
            ua.poll_max_ticks = ua.service_max_ticks = melodee_dbg.max_us = 0;
#if MELODEE_DUAL_CORE
            audio_worker.max_job_ticks = audio_worker.max_wait_ticks = 0;
#endif
            if (count == 23u) {                     /* (0.13.6) the Audio readout's Reset also starts the late count and the block count over */
                melodee_dbg.late = 0;
                audio_stats_base = audio_halves;
            }
        }
        fm1_irq_on();
        ed_b(count == 23u ? 8u : count == 64u ? 7u : count == 55u ? 6u : count == 47u ? 5u : count == 32u ? 4u : count == 26u ? 3u : 2u);
        for (i = 0; i < count; i++)
            for (k = 0; k < 5u; k++)
                ed_b(snapshot[i] >> (7u * k));
        break;
    }
#endif
#if MELODEE_CORE1_TEST && MELODEE_DUAL_CORE
    case ED_CORE1_TEST: {        /* mode: 0 report, 1 misread request, 2 null job, 3 stalled job, 4 reboot */
        uint32_t mode = na ? a[0] : 0u, rc = 0, waited = 0, v[16];
        if (mode == 4u) {                           /* (a[1] 1: keep fm1_core1_bar, else clear it as power-off does) */
            reset_intended();
            if (!(na > 1u && a[1]))
                fm1_core1_bar.magic = 0;
            fm1_reboot();
        }
        if (mode == 10u) {                          /* a CPU0 crash (the master_out report): fetch from 0 */
            void (*volatile crash)(void) = 0;
            crash();
        }
        if (mode == 9u) {                           /* 9: raw P33 0x00..0x3F, then 0x72 0x74 0x90 0x92 0x9B, 7 bits + 1 */
            static const uint8_t extra[5] = { 0x72, 0x74, 0x90, 0x92, 0x9B };
            uint8_t r[69];
            fm1_irq_off();
            for (i = 0; i < 64u; i++)
                r[i] = fm1_p33_read(i);
            for (i = 0; i < 5u; i++)
                r[64 + i] = fm1_p33_read(extra[i]);
            fm1_irq_on();
            ed_b(mode); ed_b(rc);
            for (i = 0; i < 69u; i++) {
                ed_b(r[i] & 0x7Fu);
                ed_b(r[i] >> 7);
            }
            break;
        }
        if (mode == 5u || mode == 7u || mode == 8u) {   /* 5 rails + clock (read only), 7 count misreads only */
            fm1_irq_off();                          /* (a[1]), 8 hold (a[1]) / release eight notes on track 0 */
            if (mode == 8u)
                for (i = 0; i < 8u; i++) {
                    if (na > 1u && a[1])
                        trk_note_on(&trk[0], 41u + 3u * i, 100u);
                    else
                        trk_note_off(&trk[0], 41u + 3u * i);
                }
            if (mode == 7u)
                core1_test_count_only = na > 1u && a[1];
            for (i = 0; i < FM1_RAILS; i++)
                v[i] = fm1_power_get(i);
            v[3] = (uint32_t)fm1_core1_barred(); v[4] = bootguard.failed;
            v[5] = 0;                           /* (reserved: the boot retune's undo flag until 1.3.4 / 1.4, which never retune) */
            fm1_clock_regs(v + 6);
            v[11] = audio_worker.misread; v[12] = fm1_core1_fault.count;
            v[13] = fm1_cpu_ticks();
            fm1_irq_on();
            v[14] = audio_worker.rejected; v[15] = (uint32_t)audio_worker_online;
            ed_b(mode); ed_b(rc);
            for (i = 0; i < 16u; i++)
                for (uint32_t k = 0; k < 5u; k++)
                    ed_b(v[i] >> (7u * k));
            break;
        }
        if (mode >= 1u && mode <= 3u && !audio_worker_online)
            rc = 1;                                 /* no worker (any more) */
        else if (mode >= 1u && mode <= 3u) {
            uint32_t t0 = fm1_ticks();
            fm1_irq_off();                          /* the audio ISR neither submits nor checks meanwhile */
            if (mode == 1u) {                       /* what an affected unit's CPU1 reads (#17) */
                uint32_t r = fm1_aw_load(&audio_worker.request);
                fm1_aw_store(&audio_worker.request, r + 0x00200000u);
                while (fm1_ticks() - t0 < 1000u * FM1_TICKS_PER_US)
                    ;
                fm1_aw_store(&audio_worker.request, r);
                audio_worker_check();
            } else {                                /* a fetch from 0 (#18's idle crash) or a hung job */
                audio_worker_submit(mode == 2u ? (audio_worker_fn)0 : core1_test_stall, 0);
                rc = audio_worker_join() ? 2u : 0u; /* 2: it "completed" */
            }
            waited = (fm1_ticks() - t0) / FM1_TICKS_PER_US;
            fm1_irq_on();                           /* (a masked exception would be taken here) */
        }
        v[0] = (uint32_t)audio_worker_online; v[1] = audio_worker.rejected; v[2] = fm1_core1_fault.count;
        v[3] = fm1_core1_fault.cpu; v[4] = fm1_core1_fault.dbg; v[5] = fm1_core1_fault.pc;
        v[6] = fm1_core1_fault.rets; v[7] = audio_worker.timeouts; v[8] = melodee_dbg.halves; v[9] = waited;
        v[10] = fm1_core1_fault.emu;
        ed_b(mode); ed_b(rc);
        for (i = 0; i < 11u; i++)
            for (uint32_t k = 0; k < 5u; k++)
                ed_b(v[i] >> (7u * k));
        break;
    }
#endif
    case ED_MOTION: {
        track_t *t = &trk[a[0]]; uint32_t rc = 0;
        if (na > 1u && chain_busy()) rc = 3;
        else if (na > 1u) {
            if (a[1] == 1u) motion_set_enabled(t, a[2]);
            else if (a[1] == 2u) { load_begin(t, UNDO_PAT); motion_clear(t); load_end(t); }
            else if (a[1] == 3u) rc = motion_set_event(t, a[2] & 63u, a[3] | ((a[2] & 64u) << 1), (int16_t)ed_rv(a + 4));
            else if (a[1] == 4u) {
                uint32_t id = a[3] | ((a[2] & 64u) << 1);
                if (motion_param(id) || (id >= MO_NATIVE && id < MO_NATIVE + MO_NATIVE_N))
                    motion_delete_event(t, a[2] & 63u, na==6u ? id|(motion_native_event(id,ed_rv(a+4))?256u:0u) : id|512u);
                else rc = 1;
            }
            else rc = 1;
            ui.force = 1;
        }
        ed_motion_reply(a[0], rc);
        break;
    }
    case ED_INFO:
        ed_str("ORANGE CAT " MELODEE_VERSION, 24);   /* (Orange Cat 0.1.0) was "MELODEE "; the installer accepts both */
        ed_b(NENGINES);
        ed_b(P_COUNT);
        ed_b(G_COUNT);
        ed_b(NSTEP);
        ed_b(P_E0);
        for (i = 0; i < NENGINES; i++)
            ed_str(ENGINES[i]->name, 8);
        ed_b(NTRK);                                       /* v3 */
        ed_b(0);                                          /* project-based SONG retired: tagged bank SONG below */
        ed_b(0x55); ed_b(1); ed_b(ed_ui_caps());             /* tagged preferences v1: commands 34..38 */
        ed_b(0x4d); ed_b(2); ed_b(MOTION_MAX); ed_b(1); /* motion v2: native ids, chance v1 */
        ed_b(0x42); ed_b(1); ed_b(3); /* bounded full-backup read + restore */
        ed_b(0x50); ed_b(1); ed_b(NPAT); ed_b(CHAIN_ROWS);     /* bank controls: 73/74 */
        ed_b(0x46); ed_b(1); ed_b(FM6_NFAC); ed_b(0);   /* FM6 patches: cmds 68..71 */
        ed_b(0x43); ed_b(1); ed_b(CZ_BYTES & 127u); ed_b(CZ_BYTES >> 7); ed_b(0); ed_b(0); /* old CZ bank controls retired */
        ed_b(0x35);ed_b(1);ed_b(ENGI_PROPHET);ed_b(USER_GENERAL);ed_b(USER_NATIVE_FM);ed_b(USER_NATIVE_CZ);ed_b(USER_NATIVE_P5);ed_b(0);ed_b(1); /* legacy native clients see their addressable prefix */
        ed_b(0x4e); ed_b(1); ed_b(64); ed_b(0); ed_b(0); ed_b(1); /* legacy command 78 addressable prefixes */
        ed_b(0x55);ed_b(1);ed_b(UP_SLOTS&127u);ed_b(UP_SLOTS>>7); /* shared library, command 80 */
        break;
    case ED_GET:
    case ED_SET:
        if (na < 2u || !(d = ed_desc(a[0], a[1], &vp)))
            return;
        if (cmd == ED_SET && na >= 4u && !(a[0] ? global_reserved(a[1]) : param_reserved(a[1])) && !(chain_busy() && !a[0] && a[1] >= P_SLEN && a[1] <= P_SGATE)) {
            if (a[0] == 1 && a[1] == G_ENGSEL) {          /* engine change: the safe path */
                {   uint32_t ne = (uint32_t)clamp(ed_rv(a + 2), 0, NENGINES - 1);
                    int adm = eng_admit(TSEL, ne);                       /* (0.11.0) a module's engine only when the unit can carry it */
                    if (adm) ui_message(eng_admit_text(adm)); else set_engine(ne);
                }
            } else if (d->max > d->min) {
                *vp = (int16_t)enum_orig(d, clamp(ed_rv(a + 2), d->min, d->max));
                if (a[0] == 1 && (a[1] == G_A4 || a[1] == G_BUDGET))
                    settings_save();
                if (a[0] == 0) {
                    (void)motion_capture(TSEL, a[1], *vp);
                    load_extend(TSEL);                      /* (ui.c undo: an audition's values after G_ENGSEL) */
                    if (scale_shared(a[1]))
                        scale_share(TSEL);
                }
            }
            ui.force = 1;
            ed_w.v[a[0] ? P_COUNT + a[1] : a[1]] = *vp;   /* the editor's own change: no push */
        }
        ed_b(a[0]);
        ed_b(a[1]);
        ed_v(*vp);
        break;
    case ED_DUMP:
        for (i = 0; i < ED_NV; i++)                       /* the editor gets them all here */
            ed_w.v[i] = *ed_val(i);
        ed_b(ed_eng(TSEL));
        ed_b(TSEL->preset);
        for (i = 0; i < P_COUNT; i++)
            ed_v(motion_base_value(TSEL, i));
        for (i = 0; i < G_COUNT; i++)
            ed_v(*ed_val(P_COUNT + i));
        ed_b(TSEL->preset >> 7);
        break;
    case ED_DESC:
        if (na < 2u || !(d = ed_desc(a[0], a[1], &vp)))
            return;
        ed_b(a[0]);
        ed_b(a[1]);
        ed_b(d->fmt);
        ed_v(d->min);
        ed_v(d->max);
        ed_v(d->def);
        ed_str(d->label, 8);
        ed_str(d->unit, 8);
        if (d->fmt == F_ENUM && d->names)                  /* the full SCL catalogue fits the 1024-byte reply */
            for (i = 0; i <= (uint32_t)(d->max - d->min) && i < 96u; i++)
                ed_str(d->names[i+d->min], 8);
        break;
    case ED_STEP_GET:
    case ED_STEP_SET: {
        step_t *st;
        if (na < 1u || a[0] >= NSTEP)
            return;
        st = &TSEL->step[a[0]];
        if (cmd == ED_STEP_SET && na >= 9u && !chain_busy()) {
            ed_step_put(st, a + 1, na - 1u);
        }
        ed_w.st[a[0]] = ed_step_sig(&seq_steps(TSEL)[a[0]]);
        ed_b(a[0]);
        ed_step_reply(&seq_steps(TSEL)[a[0]]);
        break;
    }
    case ED_CLOCK_STATS: {                                 /* (0.14.2) [flags: 1 reset after reading] -> pulses (5 x 7 bits), stops on a lost
                                                            * clock, resyncs (3 x 7 each), the longest gap between two pulses in ms (3 x 7),
                                                            * the pulses the last resync found missed (3 x 7) */
        uint32_t v[5];
        fm1_irq_off();
        v[0] = midi_clock_pulses; v[1] = midi_clock_losses; v[2] = midi_clock_resyncs; v[3] = midi_clock_gap_ms; v[4] = midi_clock_missed;
        if (na && (a[0] & 1u)) midi_clock_pulses = midi_clock_losses = midi_clock_resyncs = midi_clock_gap_ms = midi_clock_missed = 0;
        fm1_irq_on();
        for (uint32_t i = 0; i < 5u; i++) ed_b((v[0] >> (7u * i)) & 127u);
        for (uint32_t k = 1; k < 5u; k++)
            for (uint32_t i = 0; i < 3u; i++) ed_b((v[k] >> (7u * i)) & 127u);
        break;
    }
    case ED_PRESET:                                        /* engine, preset */
        if (na < 2u || a[0] >= NENGINES)
            return;
        adm = 0;
        {
            if (a[0] != TSEL->eng_req) {
                adm = eng_admit(TSEL, a[0]);                      /* (0.11.0) refused: the track keeps its engine and its sound */
                if (adm) ui_message(eng_admit_text(adm));
                else set_engine(a[0]);
            }
            if (!adm)
                apply_preset(a[1] | (na >= 3u ? (uint32_t)a[2] << 7 : 0u));   /* [, high bits]: PROPHET has 201 */
        }
        ui.force = 1;
        ed_b(ed_eng(TSEL));
        ed_b(TSEL->preset);
        ed_b(TSEL->preset >> 7);
        if (adm)
            ed_b((uint8_t)adm);                                   /* (0.11.0) a fourth byte only on a refusal: 1 ENGINE TOO HEAVY, 2 NO ROOM */
        break;
    case ED_PROJECT:                                       /* 0 = load, 1 = save, 2 = query; slot 0..3 */
        if (na < 2u || a[0] > 2u || a[1] >= 4u)
            return;
        if (a[0] == 1u) {
            if (ed_flash_stop() || project_save(a[1]))
                return;                              /* the legacy reply has no failure bit: do not confirm a failed save */
        }
        else if (a[0] == 0u)
            project_load(a[1] & 3u);
        ed_b(a[0]);
        ed_b(a[1] & 3u);
        ed_b(project_used(a[1] & 3u));
        break;
    case ED_NAMES: {                                       /* engine [, first lo, hi] -> preset names of an engine */
        const engine_t *en;
        uint32_t s0, n = 0, room;
        if (na < 1u || a[0] >= NENGINES)
            return;
        en = ENGINES[a[0]];
        s0 = na >= 3u ? a[1] | (uint32_t)a[2] << 7 : 0u;
        /* the names that fit (at most 127): engine, count, names, titles (2 x 9), total and first (4), F7 */
        room = sizeof ed_out - ed_n - 2u - 18u - 4u - 1u;
        for (i = s0; i < en->npresets && n < 127u; i++, n++) {
            uint32_t len = 0;
            const char *nm = eng_preset(en, i)->name;
            while (len < 20u && nm[len])
                len++;
            if (len + 1u > room)
                break;
            room -= len + 1u;
        }
        ed_b(a[0]);
        ed_b(n);
        for (i = 0; i < n; i++)
            ed_str(eng_preset(en, s0 + i)->name, 20);
        for (i = 0; i < 2u; i++)                           /* then the two edit-page titles */
            ed_str(en->page_title[i], 8);
        ed_b(en->npresets);                                /* then all presets and this page's first: the */
        ed_b(en->npresets >> 7);                           /* editor asks again from s0 + n until it has them */
        ed_b(s0);
        ed_b(s0 >> 7);
        break;
    }
    case ED_SMP_BEGIN: case ED_SMP_WRITE: case ED_SMP_END: case ED_SMP_ERASE:
        return;                                           /* retired: never write pattern storage */
    case ED_SLOT_INFO: {                                   /* -> state (0 none, 1 empty, 2 a sample), length (3 x 7 bits),
                                                            * zones, capacity (3 x 7 bits), name */
        char nm[9];
        uint32_t len, nz, st = smp_slot_info(nm, &len, &nz);
        ed_b(st); ed_b(len); ed_b(len >> 7); ed_b(len >> 14); ed_b(nz);
        ed_b(SMP_SLOT_MAX); ed_b(SMP_SLOT_MAX >> 7); ed_b(SMP_SLOT_MAX >> 14);
        ed_str(nm, 8);
        break;
    }
    case ED_SLOT_BEGIN: case ED_SLOT_WRITE: case ED_SLOT_END: case ED_SLOT_ERASE: {
        /* BEGIN length (3 x 7 bits); WRITE offset (3 x 7 bits) then up to 256 bytes packed 7-bit, in order; END the
         * 512-byte header packed 7-bit; ERASE nothing. -> 0 done, 1 refused (no slot, a bad length, out of order, an
         * END that does not match the data), 2 the flash failed or the transport would not stop; then the bytes
         * written so far (3 x 7 bits) */
        uint32_t rc = 1, v = na >= 3u ? a[0] | (uint32_t)a[1] << 7 | (uint32_t)a[2] << 14 : 0u, got;
        if (cmd == ED_SLOT_BEGIN && na == 3u) {
            rc = ed_flash_stop() ? 2u : 0u;
            if (!rc) { int r = smp_slot_begin(v); rc = r == -1 ? 1u : r ? 2u : 0u; }
        } else if (cmd == ED_SLOT_WRITE && na > 3u && (got = ed_unpack7(a + 3, na - 3u, ed_smp_buf, 256u)) != 0) {
            rc = ed_flash_stop() ? 2u : 0u;
            if (!rc) { int r = smp_slot_write(v, ed_smp_buf, got); rc = r == -1 ? 1u : r ? 2u : 0u; }
        } else if (cmd == ED_SLOT_END && ed_unpack7(a, na, ed_smp_buf, 512u) == 512u) {
            rc = ed_flash_stop() ? 2u : 0u;
            if (!rc) { int r = smp_slot_end(ed_smp_buf); rc = r == -1 ? 1u : r ? 2u : 0u; }
        } else if (cmd == ED_SLOT_ERASE && !na) {
            rc = ed_flash_stop() ? 2u : 0u;
            if (!rc) { int r = smp_slot_erase(); rc = r == -1 ? 1u : r ? 2u : 0u; }
        }
        ed_b(rc); ed_b(smp.done); ed_b(smp.done >> 7); ed_b(smp.done >> 14);
        break;
    }
#if MELODEE_FLASH || defined(OC_PACK_HOST)
    case ED_PACK_INFO: {                                   /* -> state (0 none, 1 this build's pack, 2 another build's, 3 damaged), 0 (it said whether
                                                            * library banks held presets until 0.9.0; the pack takes none now), the stored length (3 x 7 bits), the stored
                                                            * hash (5 x 7 bits), this build's length (3 x 7), its hash (5 x 7), the area (3 x 7) */
        uint32_t len, hash, st = oc_pack_info(&len, &hash);
        ed_b(st); ed_b(0);
        ed_b(len); ed_b(len >> 7); ed_b(len >> 14);
        for (i = 0; i < 5u; i++) ed_b(hash >> (7u * i));
        ed_b(OC_PACK_LEN); ed_b(OC_PACK_LEN >> 7); ed_b(OC_PACK_LEN >> 14);
        for (i = 0; i < 5u; i++) ed_b(OC_PACK_HASH >> (7u * i));
        ed_b(OC_PACK_AREA); ed_b(OC_PACK_AREA >> 7); ed_b(OC_PACK_AREA >> 14);
        break;
    }
    case ED_PACK_BEGIN: case ED_PACK_WRITE: case ED_PACK_END: case ED_PACK_ERASE: {
        /* BEGIN length (3 x 7 bits), and a byte 0 or 1 that older pages sent (ignored since 0.9.1: a pack takes no presets); WRITE offset (3 x 7 bits)
         * then up to 256 bytes packed 7-bit, in order from byte 512; END the 512-byte header packed 7-bit; ERASE with or without that byte.
         * -> rc: 0 done, 1 refused (not this build's pack, out of order, a header that does not match), 2 the flash failed, the transport is
         * running (nothing is written while it runs: stop it first) or the move of the old preset pairs has not finished (restart the unit);
         * then the bytes written so far (3 x 7 bits). rc 3 is no longer given. */
        uint32_t rc = 1, v = na >= 3u ? a[0] | (uint32_t)a[1] << 7 | (uint32_t)a[2] << 14 : 0u, got;
        int r = 0;
        if (cmd != ED_PACK_ERASE || na <= 1u) {
            if (transport_busy()) { rc = 2; }
            else if (cmd == ED_PACK_BEGIN && (na == 3u || (na == 4u && a[3] <= 1u))) r = oc_pack_begin(v), rc = r == -1 ? 1u : r == -4 ? 4u : r ? 2u : 0u;
            else if (cmd == ED_PACK_WRITE && na > 3u && (got = ed_unpack7(a + 3, na - 3u, ed_smp_buf, 256u)) != 0)
                r = oc_pack_write(v, ed_smp_buf, got), rc = r == -1 ? 1u : r ? 2u : 0u;
            else if (cmd == ED_PACK_END && ed_unpack7(a, na, ed_smp_buf, 512u) == 512u)
                r = oc_pack_end(ed_smp_buf), rc = r == -1 ? 1u : r ? 2u : 0u;
            else if (cmd == ED_PACK_ERASE && (na == 0u || a[0] <= 1u))
                r = oc_pack_erase(), rc = r == -4 ? 4u : r ? 2u : 0u;
        }
        ed_b(rc); ed_b(pk.done); ed_b(pk.done >> 7); ed_b(pk.done >> 14);
        break;
    }
#endif
#if OC_MODULES && (MELODEE_FLASH || defined(OC_PACK_HOST))
    case ED_MOD_INFO: {                                    /* [engine] -> nstarts, sectors of the area, flags (1 SHAPES built in, 2 a module wins a built-in slot, 4 an install is under way,
                                                            * 8 (0.10.1) the install the plan names is in place: no other start is free, so the old module is erased first and a power cut leaves none),
                                                            * api_version, api_size, iface (5 x 7 bits), api_addr (5 x 7), pack_hash (5 x 7), the start an install of `engine` goes to
                                                            * (a start number, or 127 none; 127 too without the argument), then per start: state (MD_*), why, engine, length (3 x 7), then (0.12.1)
                                                            * the blocker: when no start can take `engine`, the start whose usable module of another engine is in the way (the install is refused with
                                                            * "ENGINE X INSTALLED AT START N: REMOVE IT FIRST"; that module is never erased by an install), else 127. nstarts is 3 (0.12.1): the starts are the
                                                            * module area's sector 0, sector 6 and sector 4 (SHAPES: 0 and 1; the Prophet: 0 and 2). */
        int plan = na ? oc_mod_plan(a[0]) : -1;
        ed_b(OC_MOD_STARTS); ed_b(12);
        ed_b((MELODEE_SHAPES ? 1u : 0u) | (OC_MODULE_WINS ? 2u : 0u) | (mw.writing ? 4u : 0u) | (plan >= 0 && oc_mod_inplace(a[0], (uint32_t)plan) ? 8u : 0u));
        ed_b(FW_API_VERSION); ed_b(sizeof(fw_api_t));
        for (i = 0; i < 5u; i++) ed_b(FW_API_IFACE >> (7u * i));
        for (i = 0; i < 5u; i++) ed_b(FW_API_ADDR >> (7u * i));
        for (i = 0; i < 5u; i++) ed_b(OC_PACK_HASH >> (7u * i));
        ed_b(plan < 0 ? 127u : (uint32_t)plan);
        for (i = 0; i < OC_MOD_STARTS; i++) {
            ed_b(oc_mod.state[i]); ed_b(oc_mod.why[i]); ed_b(oc_mod.engine[i]);
            ed_b(oc_mod.length[i]); ed_b(oc_mod.length[i] >> 7); ed_b(oc_mod.length[i] >> 14);
        }
        { int blk = na && plan < 0 ? oc_mod_blocked_by(a[0]) : -1; ed_b(blk < 0 ? 127u : (uint32_t)blk); }
        break;
    }
    case ED_ENGINES: {                                     /* (0.11.0) -> nengines, then per engine number: source (0 not available: retired, or a module's number with none registered; 1 built in;
                                                            * 2 a module), module state (MD_*; 0 none), why, start (0, 1 or 127 none), version (3 x 7 bits: major, minor, patch), length (3 x 7 bits),
                                                            * tracks that hold it (count); then nstarts and per start: state, why, engine (so a torn start, which names no engine, is seen).
                                                            * A number with a module that was found but is not registered (refused, banned, wrong tail ..) says its state and start with source 0 or 1. */
        uint32_t e, s, k, src, st, why, at, ver, len, use;
        ed_b(NENGINES);
        for (e = 0; e < NENGINES; e++) {
            src = eng_mod[e] ? 2u : eng_ok(e) ? 1u : 0u;
            st = why = ver = len = 0; at = 127u;
            for (s = 0; s < OC_MOD_STARTS; s++) {
                if (oc_mod.state[s] == MD_EMPTY || oc_mod.state[s] == MD_TORN || oc_mod.engine[s] != e)
                    continue;
                if (at != 127u && eng_mod[e] != oc_mod_hdr_ptr(s))
                    continue;                                       /* (two for one number: the registered one is shown) */
                st = oc_mod.state[s]; why = oc_mod.why[s]; at = s; ver = oc_mod.version[s]; len = oc_mod.length[s];
                if (eng_mod[e] == oc_mod_hdr_ptr(s))
                    break;
            }
            for (k = use = 0; k < NTRK; k++)
                use += trk[k].eng_req == e;
            ed_b(src); ed_b(st); ed_b(why); ed_b(at);
            ed_b(ver >> 16); ed_b(ver >> 8); ed_b(ver);
            ed_b(len); ed_b(len >> 7); ed_b(len >> 14);
            ed_b(use);
        }
        ed_b(OC_MOD_STARTS);
        for (s = 0; s < OC_MOD_STARTS; s++) {
            ed_b(oc_mod.state[s]); ed_b(oc_mod.why[s]); ed_b(oc_mod.engine[s]);
        }
        break;
    }
    case ED_PAGES: {                                       /* (0.12.0) engine -> engine, src (0 none, 2 a module's descriptor), npages, npanel, then per page: its title
                                                            * (8) and its four panel indices (127 = none). Only a registered module's descriptor answers (a built-in
                                                            * engine's pages are the editor's own), read in place from its flash. */
        const fw_pages_t *mp;
        uint32_t p, k;
        if (a[0] >= NENGINES)
            return;
        mp = pages_of_engine(a[0]);
        ed_b(a[0]);
        ed_b(mp ? 2u : 0u);
        ed_b(mp ? mp->npages : 0u);
        ed_b(mp ? mp->npanel : 0u);
        for (p = 0; mp && p < mp->npages; p++) {
            ed_str(mp->rows[p].title, 8);
            for (k = 0; k < 4u; k++)
                ed_b(mp->rows[p].id[k] == 0xFFu ? 127u : mp->rows[p].id[k]);
        }
        break;
    }
    case ED_PANEL: {                                       /* (0.12.0) engine [, first lo, hi] -> engine, first, count, then per parameter: present (0 = none; 1), format,
                                                            * min, max, default (2 x 7 bits each), label (8), unit (8), motion skip (0/1), and for an enum its names (8). As
                                                            * many as fit the reply; the editor asks again from first + count. */
        const fw_pages_t *mp;
        uint32_t i0, i, n = 0, cnt_at;
        if (a[0] >= NENGINES)
            return;
        mp = pages_of_engine(a[0]);
        i0 = na >= 3u ? a[1] | (uint32_t)a[2] << 7 : 0u;
        ed_b(a[0]);
        ed_b(i0);
        ed_b(i0 >> 7);
        cnt_at = ed_n;
        ed_b(0);
        for (i = i0; mp && i < mp->npanel; i++) {
            const param_desc_t *d = mod_panel(mp, i);
            uint32_t need = d ? 1u + 1u + 6u + 9u + 9u + 1u + (d->fmt == F_ENUM && d->names ? (uint32_t)(d->max - d->min + 1) * 9u : 0u) : 1u;
            if (ed_n + need + 2u > sizeof ed_out - 1u || n >= 127u)
                break;
            ed_b(d ? 1u : 0u);
            if (d) {
                uint32_t j;
                ed_b(d->fmt);
                ed_v(d->min);
                ed_v(d->max);
                ed_v(d->def);
                ed_str(d->label, 8);
                ed_str(d->unit, 8);
                ed_b(mod_skip(mp, i));
                if (d->fmt == F_ENUM && d->names)
                    for (j = 0; j <= (uint32_t)(d->max - d->min); j++)
                        ed_str(d->names[j + d->min], 8);
            }
            n++;
        }
        ed_out[cnt_at] = (uint8_t)n;
        ed_b(mp ? mp->npanel : 0u);                        /* the total, then the unit asks again until it has them */
        break;
    }
    case ED_MOD_BEGIN: case ED_MOD_WRITE: case ED_MOD_END: case ED_MOD_REMOVE: {
        /* BEGIN engine, start, length (3 x 7 bits); WRITE offset (3 x 7 bits) then up to 256 bytes packed 7-bit, in order from byte 256; END the 256-byte header page
         * packed 7-bit (programmed last); REMOVE engine (127: the whole area). -> rc: 0 done, 1 refused (not the planned start, a bad length, out of order, a header
         * that does not match the code or this firmware), 2 the flash failed or the transport is running (stop it first), 4 the move of the old preset pairs has not
         * finished (restart the unit), 5 no start free, 7 written but not readable as a module, 8 (0.10.1) a sector of the module ends like an update loader record and
         * would be erased by an update (nothing is committed); then the bytes written so far (3 x 7 bits). */
        uint32_t rc = 1, v = na >= 3u ? a[0] | (uint32_t)a[1] << 7 | (uint32_t)a[2] << 14 : 0u, got;
        int r = 0;
        if (transport_busy()) rc = 2;
        else {
            if (cmd == ED_MOD_BEGIN && na == 5u) r = oc_mod_begin(a[0], a[1], a[2] | (uint32_t)a[3] << 7 | (uint32_t)a[4] << 14);
            else if (cmd == ED_MOD_WRITE && na > 3u && (got = ed_unpack7(a + 3, na - 3u, ed_smp_buf, 256u)) != 0) r = oc_mod_write(v, ed_smp_buf, got);
            else if (cmd == ED_MOD_END && ed_unpack7(a, na, ed_smp_buf, 256u) == 256u) r = oc_mod_end(ed_smp_buf);
            else if (cmd == ED_MOD_REMOVE && na == 1u) r = oc_mod_remove(a[0]);
            else r = -1;
            rc = r == 0 ? 0u : r == -1 ? 1u : r == -4 ? 4u : r == -5 ? 5u : r == -7 ? 7u : r == -8 ? 8u : 2u;
        }
        ed_b(rc); ed_b(mw.done); ed_b(mw.done >> 7); ed_b(mw.done >> 14);
        break;
    }
#endif
    case ED_SMP_INFO:
        ed_b(0); ed_b(0);                                  /* no sample slots/material */
        break;
    case ED_UP_LIST: {                                     /* start, count -> start, count, total, per slot: used, engine, name */
        uint32_t s0, cnt;
        if (na < 2u)
            return;
        s0 = a[0];
        cnt = a[1] > 16u ? 16u : a[1];
        if (s0 >= 128u)cnt=0;
        else if(s0+cnt>128u)cnt=128u-s0;
        ed_b(s0);
        ed_b(cnt);
        ed_b(128u);
        for (i = s0; i < s0 + cnt; i++) {
            char nm[13] = {0};
            uint32_t j, u = (uint32_t)up_used(i);
            for (j = 0; u && j < 12u; j++)
                nm[j] = up_rec(i)->name[j];
            ed_b(u);
            ed_b(u ? up_rec(i)->engine : 0u);
            ed_str(nm, 12);
        }
        break;
    }
    case ED_UP_GET: {                                      /* slot -> slot, used, engine, name, P_COUNT x v14, 16 x (note,
                                                            * flags), (v5) kind [, 16 x hi] (upreset.c up_parse) */
        int16_t v[P_COUNT];
        char nm[13] = {0};
        const up_rec_t *r;
        uint32_t u;
        if (na < 1u || a[0] >= 128u)
            return;
        r = up_rec(a[0]);
        u = (uint32_t)up_used(a[0]);
        if (u)
            up_values(r, v);                               /* today's P_* order, clamped */
        for (i = 0; u && i < 12u; i++)
            nm[i] = r->name[i];
        ed_b(a[0]);
        ed_b(u);
        ed_b(u ? up_engine(a[0]) : 0u);
        ed_str(nm, 12);
        for (i = 0; i < P_COUNT; i++)
            ed_v(u ? v[i] : 0);
        for (i = 0; i < 16u; i++) {
            ed_b(u ? r->note[i] & 0x7Fu : 0u);
            ed_b(u ? r->flags[i] & 0x7Fu : 0u);
        }
        ed_b(u && up_grid(r));                  /* kind: 1 = a drum grid, then bit 7 of each step's */
        for (i = 0; u && up_grid(r) && i < 16u; i++)   /* hits (bit 0) and accents (bit 1) */
            ed_b((uint32_t)(r->note[i] >> 7) | (uint32_t)(r->flags[i] >> 7) << 1);
        break;
    }
    case ED_UP_PUT: {                                      /* slot, engine, name, values, pattern -> slot, rc */
        static up_rec_t r;
        uint32_t slot = 0, rc;
        if (na < 1u)
            return;
        rc = (uint32_t)up_parse(a, na, &r, &slot);
        if (!rc && ed_flash_stop())
            rc = 2;
        if (!rc) {
            int16_t v[P_COUNT];
            up_values(&r, v);                              /* each value inside its range */
            for (i = 0; i < P_COUNT; i++)
                up_set_value(&r, i, v[i]);                 /* the record's own form (v4: a byte each) */
            rc = up_put(slot, &r) ? 2u : 0u;
        }
        ed_b(a[0]);
        ed_b(rc);
        break;
    }
    case ED_UP_STORE: {                                    /* slot, name -> slot, rc */
        char nm[13] = {0};
        uint32_t n0, rc = 1;
        if (na < 1u)
            return;
        for (n0 = 0; 1u + n0 < na && a[1 + n0] && n0 < 13u; n0++)
            ;
        if (a[0] < 128u && (!n0 || up_name_ok(a + 1, n0)) &&
            (1u + n0 == na || (2u + n0 == na && !a[1u + n0]))) {   /* "" = automatic name */
            for (i = 0; i < n0; i++)
                nm[i] = (char)a[1 + i];
            int r;
            r = ed_flash_stop() ? 2 : up_store(a[0], nm);
            rc = r ? 2u : 0u;
        }
        ed_b(a[0]);
        ed_b(rc);
        break;
    }
    case ED_UP_LOAD:                                       /* slot -> slot, rc */
        if (na < 1u)
            return;
        ed_b(a[0]);
        ed_b(a[0] < 128u && !up_load(a[0]) ? 0u : 1u);
        break;
    case ED_UP_ERASE:
        if (na < 1u)
            return;
        ed_b(a[0]);
        ed_b(a[0] >= 128u ? 1u : ed_flash_stop() || up_put(a[0], 0) ? 2u : 0u);
        break;
    case ED_WATCH:                                         /* on -> on */
        if (na < 1u)
            return;
        ed_w.on = a[0] & 1u;
        ed_w.v4 = (uint8_t)(ed_w.on && (a[0] & 2u));     /* v4: also TRACK_CHANGED; the reply says it is known */
        ed_w.resets = usb.resets;
        if (ed_w.on)
            ed_shadow();
        ed_b(ed_w.on | ed_w.v4 << 1);
        break;
    case ED_PING:
        ed_b(0);
        break;
    case ED_TRACK:                                         /* [track] -> selected, NTRK, per track: engine, preset, level, mute, armed */
        if (na >= 1u && a[0] < NTRK && a[0] != song.sel) {
            track_select(a[0]);
            if (ed_w.on)
                ed_shadow();                               /* the editor re-reads it: no RELOAD for this */
        }
        ed_b(song.sel);
        ed_b(NTRK);
        for (i = 0; i < NTRK; i++) {
            ed_b(ed_eng(&trk[i]));
            ed_b(trk[i].preset);
            ed_v(trk[i].p[P_LEVEL]);
            ed_b(trk[i].p[P_MUTE] != 0);
            ed_b((song.rec >> i) & 1u);
        }
        for (i = 0; i < NTRK; i++)                         /* the presets' high bits, last */
            ed_b(trk[i].preset >> 7);
        break;
    case ED_TRACK_MIX: {                                   /* track [, level v14, mute] -> track, level, mute */
        track_t *t;
        int16_t *lv;
        if (na < 1u || a[0] >= NTRK)
            return;
        t = &trk[a[0]];
        lv = &t->p[P_LEVEL];
        if (na >= 4u) {
            *lv = (int16_t)clamp(ed_rv(a + 1), 0, 127);
            t->p[P_MUTE] = (int16_t)(a[3] ? 1 : 0);
            ed_known(a[0], P_LEVEL);                       /* the editor's own change: no push */
            ed_known(a[0], P_MUTE);
            ui.force = 1;
        }
        ed_b(a[0]);
        ed_v(*lv);
        ed_b(t->p[P_MUTE] != 0);
        break;
    }
    case ED_TRACK_DUMP:                                    /* track -> track, engine, preset, P_COUNT x v14 */
        if (na < 1u || a[0] >= NTRK)
            return;
        ed_b(a[0]);
        ed_b(ed_eng(&trk[a[0]]));
        ed_b(trk[a[0]].preset);
        for (i = 0; i < P_COUNT; i++)
            ed_v(motion_base_value(&trk[a[0]], i));
        ed_b(trk[a[0]].preset >> 7);
        break;
    case ED_TRACK_STEP: {                                  /* track, index [, step] -> track, index, step (as STEP_GET) */
        step_t *st;
        if (na < 2u || a[0] >= NTRK || a[1] >= NSTEP)
            return;
        st = &trk[a[0]].step[a[1]];
        if (na >= 10u && !chain_busy())
            ed_step_put(st, a + 2, na - 2u);
        if (a[0] == song.sel)
            ed_w.st[a[1]] = ed_step_sig(&seq_steps(&trk[a[0]])[a[1]]);
        ed_b(a[0]);
        ed_b(a[1]);
        ed_step_reply(&seq_steps(&trk[a[0]])[a[1]]);
        break;
    }
    case ED_TRACK_PARAM: {                                 /* track, id [, v14] -> track, id, v14 */
        track_t *t;
        if (na < 2u || a[0] >= NTRK || a[1] >= P_COUNT)
            return;
        t = &trk[a[0]];
        d = ed_tdesc(t, a[1]);
        if (na >= 4u && !param_reserved(a[1]) && !(chain_busy() && a[1] >= P_SLEN && a[1] <= P_SGATE)) {
            if (d->max > d->min)                           /* as SET: clamped; a fixed value stays */
                t->p[a[1]] = (int16_t)enum_orig(d, clamp(ed_rv(a + 2), d->min, d->max));
            (void)motion_capture(t, a[1], t->p[a[1]]);
            if (scale_shared(a[1]))
                scale_share(t);
            ed_known(a[0], a[1]);
            ui.force = 1;
        }
        ed_b(a[0]);
        ed_b(a[1]);
        ed_v(t->p[a[1]]);
        break;
    }
    case ED_PATTERN: {
        if (na < 2u || a[0] >= NTRK || a[1] > 2u || na != (a[1] == 0u ? 2u : a[1] == 1u ? 3u : 4u)) return;
        track_t *t = &trk[a[0]]; uint32_t rc = 0;
        if (a[1] == 1u) rc = pattern_request(t, a[2]);
        if (a[1] == 2u) rc = pattern_copy(t, a[2], a[3]);
        ed_b(a[0]); ed_b(a[1]); ed_b(rc); ed_b(t->pattern); ed_b(t->pattern_next < NPAT ? t->pattern_next : 127u); ed_b(NPAT);
        ui.force = 1;
        break;
    }
    case ED_BANK_SONG: {
        if (!na || a[0] > 3u) return;
        uint32_t rc = 0;
        if (a[0] == 1u) {
            if (na < 2u || a[1] > CHAIN_ROWS || na != 2u + a[1] * 5u) rc = 1;
            else if (chain_busy()) rc = 2;
            else {
                for (uint32_t row = 0; row < a[1]; row++) {
                    for (uint32_t k = 0; k < NTRK; k++) if (a[2u + row * 5u + k] >= NPAT) rc = 1;
                    if (!a[6u + row * 5u] || a[6u + row * 5u] > 16u) rc = 1;
                }
                if (!rc) {
                    chain_defaults(&chain_config); memset(chain_patterns, 0, sizeof chain_patterns);
                    chain_config.count = a[1];
                    for (uint32_t row = 0; row < a[1]; row++) {
                        memcpy(chain_patterns[row], a + 2u + row * 5u, NTRK);
                        chain_config.row[row] = (chain_row_t){chain_patterns[row][0], a[6u + row * 5u]};
                    }
                    ui.song_row = 0; ui.force = 1;
                }
            }
        } else if (a[0] == 2u) rc = chain_prepare();
        else if (a[0] == 3u) transport_req = 2;
        ed_b(a[0]); ed_b(rc); ed_b(chain_config.count); ed_b(chain.running); ed_b(chain.row); ed_b(chain.remaining);
        for (uint32_t row = 0; row < chain_config.count; row++) {
            for (uint32_t k = 0; k < NTRK; k++) ed_b(chain_patterns[row][k]);
            ed_b(chain_config.row[row].repeat);
        }
        break;
    }
    case ED_SONG: {
        uint32_t rc = 0, op;
        chain_config_t c;
        if (!na || a[0] > 3u) return;
        op = a[0];
        if (op == 1u) {
            chain_defaults(&c);
            if (na < 2u || a[1] > CHAIN_ROWS || na != 2u + 2u * a[1]) rc = 1;
            else {
                c.count = a[1];
                for (i = 0; i < c.count; i++) {
                    c.row[i].slot = a[2u + 2u * i];
                    c.row[i].repeat = a[3u + 2u * i];
                }
                if (!chain_valid(&c)) rc = 1;
                else if (chain_busy()) rc = 2;
                else {
                    chain_config = c;
                    for (uint32_t row = 0; row < c.count; row++) memset(chain_patterns[row], c.row[row].slot, NTRK);
                    ui.song_row = 0; ui.force = 1;
                }
            }
        } else if (op == 2u) rc = chain_prepare();
        else if (op == 3u) transport_req = 2;
        ed_b(op); ed_b(rc); ed_b(chain_config.count);
        ed_b(chain.running); ed_b(chain.row); ed_b(chain.remaining);
        for (i = 0; i < chain_config.count; i++) {
            ed_b(chain_config.row[i].slot); ed_b(chain_config.row[i].repeat);
        }
        break;
    }
    default:
        return;
    }
    ed_send();
}

/* main loop: editor frames first; anything else stays for ota_service() */
static void ed_service(void)
{
    const uint8_t *p;
    uint32_t n;
    ed_sync();                                             /* v2 pushes (while watched) */
#if OC_HW_TEST
    hwt_uart_poll();                                       /* (0.13.8) command 108 from the TRS input */
#endif
    if(!ota_frame_get(&p,&n))return;
    if(general_sysex_frame(p,n)){ota_frame_done();return;}
    if(MELODEE_P5 && p5_native_frame(p,n)){ota_frame_done();return;}   /* (Orange Cat: MELODEE_P5) */
    if (n < 4u || p[0] != ED_HDR0 || p[1] != ED_HDR1 || p[2] != ED_HDR2)
        return;
    ed_w.last_ms = fm1_ms;                                 /* any request keeps WATCH alive */
    if (p[3] >= ED_SMP_BEGIN || p[3] == ED_EXT) {         /* large frames: handled in place, then freed ((0.14.2) the extension's too: KIT_PUT is 79 bytes) */
        ed_handle(p, n);
        ota_frame_done();
        return;
    }
    {
        static uint8_t f[64];
        uint32_t k = n > sizeof f ? sizeof f : n, i;
        for (i = 0; i < k; i++)
            f[i] = p[i];
        ota_frame_done();                                  /* free the frame buffer before replying */
        ed_handle(f, k);
    }
}
