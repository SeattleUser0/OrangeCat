/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Orange Cat Round 28 item 1: the rising cap (firmware/src/voice.c trk_poly_cap, rise_block; engines.c mc_vpart; eng_macro.c; BUILD.md 7.42), through the whole
 * mix as the firmware runs it, on an arena of the unit's size, with budgets of 18 units (CPU1 online, 360 MHz) and 16 (one core, or 320 MHz).
 *   FAMILY   four MACRO tracks; the fourth plays a sixteen-note chord with all ON and again with tracks 1 to 3 OFF: its cap and its voices sounding are the
 *            stated number for each family (VA POLY 6 / 5, HARP 6 / 5, VA-VCF 9 / 8, TERRAN 9 / 8, NOISE POLY 9 / 8, CHIP POLY 16 / 16; WAVSHP 4, ADDTV,
 *            MODAL and the large models unchanged; SHAPES and DRUM-X unchanged; MONO 1 and UNISON 4 unchanged); the units sounding never pass the budget
 *            (checked every block; MODAL's lone-track overshoot, the protected lowest note, is older than this and is reported, not checked)
 *   FALL     CHIP at 16 with three OFF: track 1 ON: the cap falls to 12, the four voices of the leaving block are given up with the one-block fade
 *            (stage 4) and the block goes back; track 1 takes its state and plays; all ON: 4, no block held, the arena as at the start
 *   ONPLAY   VA at 6 (18 units, three OFF): track 1 ON plays a chord: its notes sound, the units never pass 18; CHIP at 16: track 2 ON and plays while the
 *            fourth holds its chord: the same
 *   MONO     a mono preset (VA BASS) and UNISON keep their voices with three OFF
 * Run by tests/run_tests.sh. On a firmware without the rising cap (49d3625f) it builds with the fallbacks below and fails. */
#define RES_HOST_MAIN_WORDS (85760u / 4u)              /* the device's banks (tests/jfx_arena_test.c) */
#define RES_HOST_LOW_WORDS (27740u / 4u)
#define RES_HOST_DATA_WORDS (4216u / 4u)
#define RES_HOST_RETAINED_WORDS (816u / 4u)
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

#ifndef OC_RISE_CAP                                     /* (the firmware before Round 28: the engine's cap, no blocks) */
static uint32_t trk_poly_cap(const track_t *t) { return trk_nvoice(t); }
static uint8_t rise_have[NPART];
static uint32_t rise_kills;
static uint32_t rise_bytes(uint32_t k) { (void)k; return 0; }
#else
static uint32_t rise_bytes(uint32_t k)
{
    uint32_t b, n = 0;
    for (b = 0; b < rise_have[k]; b++)
        if (rise_slot[k][b]) n += resource[RES_RISE0 + rise_slot[k][b] - 1u].size;
    return n;
}
#endif

static int bad, over_checked;
#define CHECK(c, ...) do { if (!(c)) { bad++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define SEC (FS / CTL + 2u)
static int32_t blk[CTL * 2];
static uint32_t over_max;                               /* the most units sounding seen over the budget (0: never over) */
static void run(uint32_t n)
{
    while (n--) {
        mix_block(blk, CTL);
        if (voices_busy() > voice_budget() && voices_busy() - voice_budget() > over_max) over_max = voices_busy() - voice_budget();
    }
}
static uint32_t held(uint32_t k) { return resource[RES_ENGINE0 + k].size; }
static uint32_t sounding(uint32_t k) { uint32_t n = 0; for (uint32_t i = 0; i < NVOICE; i++) n += trk[k].v[i].active && trk[k].v[i].stage != 4u; return n; }
static uint32_t mc_preset(const char *name)
{
    for (uint32_t i = 0; i < NELEM(MC_PRESETS); i++) if (!strcmp(MC_PRESETS[i].name, name)) return i;
    printf("FAIL: no MACRO preset %s\n", name); bad++; return 0;
}
static void fresh(uint32_t budget)
{
    ui_power_on();
    resource_host_cache_enabled = 0; res_lend_ok = 0;
    song.master_q12 = 4096; song.g[G_BPM] = 120; song.playing = 0; song.sel = 3;
    settings_budget = (int16_t)budget;
    trk_off_bits = 0;
    run(2);
    for (uint32_t k = 0; k < NPART; k++) {
        set_engine_of(&trk[k], ENGI_MACRO); apply_preset_to(&trk[k], 8u);   /* (ORGAN DRAWBAR: ADDTV, POLY, 7 units: a sound whose cap does not rise) */
        track_defaults_steps(&trk[k]);
    }
    run(8);
    over_max = 0;
}
static void off3(int off)                               /* tracks 1 to 3 OFF (1) or ON (0), and settled */
{
    for (uint32_t k = 0; k < 3u; k++) track_off_req(k, off);
    run(16);
}
static void chord(uint32_t k, uint32_t n, int on)
{
    for (uint32_t i = 0; i < n; i++) { if (on) trk_note_on(&trk[k], 48u + i, 100); else trk_note_off(&trk[k], 48u + i); }
}

/* the families: the fourth track's sound, its voice mode (0: the preset's), the caps with all ON and with three OFF at 18 and 16 units */
enum { SND_MACRO, SND_SHAPES, SND_DRUMX };
typedef struct { const char *what, *preset; uint8_t kind, mode, on, off18, off16, check_units; } fam_t;
static const fam_t FAM[] = {
    {"MACRO VA (VA BASS, POLY)", "VA BASS", SND_MACRO, V_POLY + 1u, 4, 6, 5, 1},
    {"MACRO HARP (3 units)", "HARP", SND_MACRO, 0, 4, 6, 5, 1},
    {"MACRO VA-VCF (VCF BRASS, 2 units)", "VCF BRASS", SND_MACRO, 0, 4, 9, 8, 1},
    {"MACRO TERRAN (TERRAIN PLUCK, 2 units)", "TERRAIN PLUCK", SND_MACRO, 0, 4, 9, 8, 1},
    {"MACRO NOISE (NOISE WIND, POLY, 2 units)", "NOISE WIND", SND_MACRO, V_POLY + 1u, 4, 9, 8, 1},
    {"MACRO CHIP (CHIP ARP, POLY, 1 unit)", "CHIP ARP", SND_MACRO, V_POLY + 1u, 4, 16, 16, 1},
    {"MACRO WAVSHP (FOLD PLUCK, 4 units)", "FOLD PLUCK", SND_MACRO, 0, 4, 4, 4, 1},
    {"MACRO ADDTV (ORGAN DRAWBAR, 7 units)", "ORGAN DRAWBAR", SND_MACRO, 0, 4, 4, 4, 1},
    {"MACRO MODAL (MALLET, 10 units)", "MALLET", SND_MACRO, 0, 4, 4, 4, 0},
    {"MACRO STRING (PLUCKED STRING, large, POLY)", "PLUCKED STRING", SND_MACRO, V_POLY + 1u, 1, 1, 1, 1},
    {"MACRO 6OP A (DX EPIANO, large, POLY)", "DX EPIANO", SND_MACRO, V_POLY + 1u, 1, 1, 1, 1},
    {"MONO (VA BASS as stored)", "VA BASS", SND_MACRO, 0, 4, 4, 4, 1},
    {"UNISON (VA BASS, UNISON)", "VA BASS", SND_MACRO, V_UNISON + 1u, 4, 4, 4, 1},
    {"SHAPES (POLY)", 0, SND_SHAPES, V_POLY + 1u, 4, 4, 4, 1},
    {"DRUM-X (kit 1)", 0, SND_DRUMX, 0, 8, 8, 8, 1},
};

static void set_sound(const fam_t *f)
{
    track_t *t = &trk[3];
    if (f->kind == SND_MACRO) { set_engine_of(t, ENGI_MACRO); apply_preset_to(t, mc_preset(f->preset)); }
    else { set_engine_of(t, f->kind == SND_SHAPES ? ENGI_SHAPES : ENGI_DRUMX); apply_preset_to(t, 0); }
    if (f->mode) t->p[P_VOICE] = (int16_t)(f->mode - 1u);
    run(8);
}
/* the voices a sixteen-note chord sounds on track 4 (drums: the eight lanes, two notes each) */
static uint32_t play16(const fam_t *f)
{
    uint32_t i, n;
    for (i = 0; i < 16u; i++) trk_note_on(&trk[3], f->kind == SND_DRUMX ? 36u + i % 8u : 48u + i, 100);
    run(4); n = sounding(3);
    for (i = 0; i < 16u; i++) trk_note_off(&trk[3], f->kind == SND_DRUMX ? 36u + i % 8u : 48u + i);
    run(3u * SEC);
    return n;
}

int main(void)
{
    uint32_t c, b, k, budget;
    setvbuf(stdout, NULL, _IONBF, 0);

    /* FAMILY */
    for (b = 0; b < 2u; b++) {
        budget = b ? 16u : 18u;
        for (c = 0; c < NELEM(FAM); c++) {
            const fam_t *f = &FAM[c];
            uint32_t want = b ? f->off16 : f->off18, cap_on, cap_off, v_on, v_off, v_back, cap_back, mono = f->kind == SND_MACRO && f->preset && !f->mode && !strcmp(f->preset, "VA BASS");
            fresh(budget);
            set_sound(f);
            cap_on = trk_poly_cap(&trk[3]);
            v_on = play16(f);
            off3(1);
            cap_off = trk_poly_cap(&trk[3]);
            over_max = 0;
            v_off = play16(f);
            if (f->check_units)
                CHECK(!over_max, "FAMILY %s at %u: the units sounding passed the budget by %u", f->what, budget, over_max);
            off3(0);
            cap_back = trk_poly_cap(&trk[3]);
            v_back = play16(f);
            if (mono) {
                CHECK(v_on == 1u && v_off == 1u && trk[3].p[P_VOICE] != V_POLY, "FAMILY %s at %u: one voice ON (%u) and with three OFF (%u)", f->what, budget, v_on, v_off);
            } else if (f->mode == V_UNISON + 1u || f->kind != SND_MACRO || !f->check_units) {
                CHECK(cap_off == cap_on && v_off == v_on, "FAMILY %s at %u: unchanged with three OFF: cap %u -> %u, voices %u -> %u", f->what, budget, cap_on, cap_off, v_on, v_off);
            } else {
                uint32_t u = voice_units(&trk[3]), fit = u ? budget / u : 16u;   /* (the units bound the voices under any cap: ADDTV's 7 units, 2 voices) */
                CHECK(cap_on == f->on && v_on == (f->on < fit ? f->on : fit), "FAMILY %s at %u: all ON: cap %u, voices %u (want %u, %u)", f->what, budget, cap_on, v_on, f->on, f->on < fit ? f->on : fit);
                CHECK(cap_off == want && v_off == (want < fit ? want : fit), "FAMILY %s at %u: three OFF: cap %u, voices %u (want %u, %u)", f->what, budget, cap_off, v_off, want, want < fit ? want : fit);
            }
            CHECK(cap_back == cap_on && v_back == v_on && !rise_have[3] && !rise_bytes(3), "FAMILY %s at %u: ON again: cap %u voices %u, %u blocks held (as before: %u, %u)",
                  f->what, budget, cap_back, v_back, rise_have[3], cap_on, v_on);
            printf("rising cap: FAMILY %-44s at %u units: cap %2u -> %2u three OFF -> %2u ON again; voices %2u -> %2u -> %2u; over the budget by %u\n",
                   f->what, budget, cap_on, cap_off, cap_back, v_on, v_off, v_back, over_max);
        }
    }

    /* FALL */
    {
        uint32_t used0, used_off, kills0, i, t_rel, cap12, v12, held1, pk_before = 0, pk_after = 0;
        fresh(18);
        set_sound(&FAM[5]);                             /* CHIP POLY */
        used0 = resource_used();
        off3(1);
        used_off = resource_used();
        chord(3, 16, 1); run(4);
        CHECK(sounding(3) == 16u && trk_poly_cap(&trk[3]) == 16u && rise_bytes(3) == 3u * ((MC_STATE_BYTES + 3u) & ~3u),
              "FALL: three OFF, CHIP: 16 voices (%u), cap %u, the blocks %u B", sounding(3), trk_poly_cap(&trk[3]), rise_bytes(3));
        CHECK(used_off == used0, "FALL: the arena: the three OFF tracks' states went to the fourth's voices (%u B used, %u at the start)", used_off, used0);
        for (i = 0; i < 4u; i++) { trk[3].peak = 0; run(1); if (trk[3].peak > pk_before) pk_before = trk[3].peak; }
        kills0 = rise_kills;
        track_off_req(0, 0);                            /* track 1 ON */
        run(1);
        cap12 = trk_poly_cap(&trk[3]);
        CHECK(cap12 == 12u, "FALL: track 1 ON: the cap falls to 12 at once (%u)", cap12);
        CHECK(rise_kills - kills0 == 4u, "FALL: the four voices of the leaving block given up (%u), with the one-block fade", rise_kills - kills0);
        for (i = 12; i < 16u; i++) CHECK(!trk[3].v[i].active || trk[3].v[i].stage == 4u, "FALL: voice %u fading (stage %u)", i, trk[3].v[i].stage);
        for (t_rel = 1; t_rel < 16u && (rise_bytes(3) > 2u * ((MC_STATE_BYTES + 3u) & ~3u) || !held(0)); t_rel++) {
            trk[3].peak = 0; run(1); if (trk[3].peak > pk_after) pk_after = trk[3].peak;
        }
        v12 = sounding(3); held1 = held(0);
        CHECK(v12 == 12u, "FALL: the fourth sounds 12 (%u)", v12);
        CHECK(rise_bytes(3) == 2u * ((MC_STATE_BYTES + 3u) & ~3u), "FALL: the block went back: %u B held", rise_bytes(3));
        CHECK(held1 && !((trk_on_wait >> 0) & 1u) && t_rel <= 4u, "FALL: track 1 took its state (%u B) in %u blocks", held1, t_rel);
        CHECK(pk_after <= pk_before, "FALL: the drop makes no peak (%d after, %d before)", pk_after, pk_before);
        trk_note_on(&trk[0], 72, 100); run(4);
        CHECK(sounding(0) == 1u, "FALL: track 1 plays");
        trk_note_off(&trk[0], 72);
        CHECK(!over_max, "FALL: the units never passed the budget (by %u)", over_max);
        for (k = 1; k < 3u; k++) track_off_req(k, 0);
        run(8);
        CHECK(trk_poly_cap(&trk[3]) == 4u && sounding(3) <= 4u && !rise_have[3] && !rise_bytes(3), "FALL: all ON: cap %u, %u sounding, %u blocks",
              trk_poly_cap(&trk[3]), sounding(3), rise_have[3]);
        chord(3, 16, 0); run(3u * SEC);
        CHECK(resource_used() == used0, "FALL: the arena as at the start (%u B, %u)", resource_used(), used0);
        printf("rising cap: FALL: CHIP 16 -> track 1 ON: cap %u, %u sounding, 4 voices given up, the block back and track 1's state in %u blocks; all ON: cap %u; arena %u B\n",
               cap12, v12, t_rel, trk_poly_cap(&trk[3]), resource_used());
    }

    /* ONPLAY */
    {
        uint32_t i;
        fresh(18);
        set_sound(&FAM[0]);                             /* VA POLY */
        off3(1);
        chord(3, 6, 1); run(4);
        CHECK(sounding(3) == 6u && voices_busy() == 18u, "ONPLAY: VA at 6 voices (%u), 18 units (%u)", sounding(3), voices_busy());
        apply_preset_to(&trk[0], mc_preset("VA BASS")); trk[0].p[P_VOICE] = V_POLY;
        track_off_req(0, 0); run(4);
        CHECK(trk_poly_cap(&trk[3]) == 6u && sounding(3) == 6u, "ONPLAY: track 1 ON: the fourth keeps 6 (cap %u, %u sounding: VA needs one block of the two left)", trk_poly_cap(&trk[3]), sounding(3));
        over_max = 0;
        for (i = 0; i < 3u; i++) trk_note_on(&trk[0], 60u + 4u * i, 100);
        run(4);
        CHECK(sounding(0) >= 1u && !over_max && voices_busy() <= 18u, "ONPLAY: track 1's chord sounds (%u) beside the fourth (%u); units %u, never over (by %u)",
              sounding(0), sounding(3), voices_busy(), over_max);
        CHECK(trk[3].v[lowest_held(&trk[3])].note == 48u, "ONPLAY: the fourth's lowest note kept");
        printf("rising cap: ONPLAY: VA at 6 + track 1 ON with three notes: %u + %u voices, %u units\n", sounding(3), sounding(0), voices_busy());
        for (i = 0; i < 3u; i++) trk_note_off(&trk[0], 60u + 4u * i);
        chord(3, 6, 0); run(3u * SEC);

        fresh(18);
        set_sound(&FAM[5]);                             /* CHIP POLY */
        off3(1);
        chord(3, 16, 1); run(4);
        apply_preset_to(&trk[1], mc_preset("VA BASS")); trk[1].p[P_VOICE] = V_POLY;
        over_max = 0;
        track_off_req(1, 0); run(1);
        for (i = 0; i < 3u; i++) trk_note_on(&trk[1], 60u + 4u * i, 100);
        run(8);
        CHECK(trk_poly_cap(&trk[3]) == 12u && sounding(1) >= 1u && !over_max && voices_busy() <= 18u,
              "ONPLAY: CHIP at 16, track 2 ON and plays: cap %u, track 2 sounds %u, units %u, never over (by %u)", trk_poly_cap(&trk[3]), sounding(1), voices_busy(), over_max);
        printf("rising cap: ONPLAY: CHIP at 16 + track 2 ON with three notes: %u + %u voices, %u units\n", sounding(3), sounding(1), voices_busy());
    }

    printf(bad ? "rising cap: %d FAILED\n" : "rising cap: OK\n", bad);
    (void)over_checked;
    return bad != 0;
}
