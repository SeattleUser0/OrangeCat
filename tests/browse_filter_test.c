/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 *
 * Round 26 item 1, the owner's rule: browse shows only what can play (firmware/src/browse.c entry_plays). A user preset, a RECENT entry or a
 * MACRO preset is listed only while its engine is installed (and for MACRO, its model); hidden ones stay stored and come back when the module is
 * registered; a project's track on a missing engine or model keeps its sound, says MODULE MISSING, and a PRESETS turn from it goes to its
 * neighbours in the list (the entries either side of the place it has in ALL's order).
 * Built as the device's engine set (tools/orangecat_engines.sh: SHAPES, the Prophet and WHEEL only as modules, MACRO with VA alone built in,
 * MELODEE_MACRO_PICK=1) with the MODELS module's host unit linked in (tests/module_host_plaits_tu.c), on the persistence harness (the simulated
 * NOR, the UI, the project store). SHAPES and WHEEL are registered as stand-in engines (a copy of DRUM-X's, as tests/engine_table_test.c does):
 * a PRESETS turn loads the stand-in WHEEL once (section 6).
 *   build/host/browse_filter_test
 */
#define OC_SET_OK 1
#define PERSISTENCE_TEST_NO_MAIN
#include "persistence_test.c"
#include <time.h>
#if !MELODEE_MACRO_PICK || MELODEE_SHAPES || MELODEE_P5 || !MELODEE_DRUMX
#error "build with the device's engine set: -DMELODEE_MACRO_PICK=1 -DMELODEE_SHAPES=0 -DMELODEE_P5=0 ... (tests/run_tests.sh browse_filter)"
#endif
#ifndef BROWSE_ENTRY_PLAYS                          /* (29d4d869, before Round 26: the proof that this test fails there) */
#define entry_plays entry_ok
static uint32_t up_model(uint32_t k) { const up_rec_t *r = up_rec(k); return r->engine == ENGI_MACRO ? (uint32_t)r->packed[r->np - 8u] - 64u : 0u; }
#endif
extern const fw_api_t *oc_host_fw;
extern const void *const *oc_host_models(uint32_t *n);
extern const void *const *oc_host_banks(void);

/* PRESETS' detents as ui_input.c pick_by gives them (accel_by with a cap of 1: one entry a detent; fast: the same way within ACC_GAP, 60 ms,
 * of the last one). The persistence harness does not take ui_input.c; this is its rule for this knob */
static uint32_t pk_t, pk_up, pk_any;
static int32_t pick_by(uint32_t role, int32_t s, uint32_t *fast)
{
    uint32_t up = s > 0;
    (void)role;
    *fast = pk_any && up == pk_up && fm1_ms - pk_t < 60u;
    pk_t = fm1_ms; pk_up = up; pk_any = 1;
    return s;
}
static int bad;
static void chk(const char *what, int ok)
{
    printf("browse filter: %-118s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
static const void *const *models;
static uint32_t nmodels;
static engine_t fake_shapes, fake_wheel;
static fw_mod_hdr_t h_shapes, h_wheel;
static fw_mod_hdr_t hdr(uint32_t engine)
{
    fw_mod_hdr_t h = {FW_MOD_MAGIC, FW_API_IFACE, sizeof(fw_api_t), FW_API_VERSION, FW_API_ADDR, engine, 1234u, 0, 0, OC_PACK_HASH, 0, 0, 0};
    return h;
}
static void modules(int on)                         /* SHAPES, WHEEL and the MODELS module registered, or none of them */
{
    if (on) {
        eng_register(&h_shapes, &fake_shapes);
        eng_register(&h_wheel, &fake_wheel);
        eng_models_set(ENGI_MACRO, models, nmodels, oc_host_banks());
    } else {
        eng_unregister(ENGI_SHAPES);
        eng_unregister(ENGI_WHEEL);
        eng_models_clear(ENGI_MACRO);
    }
}
/* entry (src, k) is in LIST m */
static int listed(uint32_t m, uint32_t src, uint32_t k)
{
    uint32_t as, ak, total;
    return list_scan(m, src, k, 0xFFFFFFFFu, &as, &ak, &total) != total;
}
/* the test's own answer (not the firmware's predicate): with no module, the user slots 1 (MACRO on MALLET, model 10), 2 (SHAPES) and 3
 * (WHEEL) cannot play, nor any MACRO factory preset whose model is not VA, nor any sound of SHAPES, the Prophet or WHEEL */
static int hidden_without(uint32_t src, uint32_t k)
{
    uint32_t e = src_engine(src, k);
    if (src == USER_GENERAL && k >= 1u && k <= 3u) return 1;
    if (src == ENGI_MACRO && eng_preset(ENGINES[ENGI_MACRO], k)->e[0] != 0) return 1;
    return e == ENGI_SHAPES || e == ENGI_PROPHET || e == ENGI_WHEEL;
}
static uint32_t ord(uint32_t e)                     /* e's place in ENGINE_ORDER */
{
    uint32_t i;
    for (i = 0; i < NENG_SHOWN && eng_vis(i) != e; i++)
        ;
    return i;
}
/* a click of PRESETS on HOME: slow (the immediate path: the knob rested since the last one) or fast (16 ms after it: the pending path) */
static void click(int32_t dir, int fast)
{
    fm1_ms += fast ? 16u : 1000u;
    browse_turn(EN_PRESET, dir);
}
static void rest(void) { fm1_ms += BROWSE_REST_MS + 10u; browse_poll(); }
static void load_at(uint32_t n) { fm1_ms += 1000u; preset_go(n); }

/* 20 clicks right then 20 left from list entry start: each one moves exactly one listed entry (the place shown: the pending one on the fast path,
 * the loaded one on the slow), wrapping at both ends, and never shows a hidden one; the fast walk loads where it rests */
static void walk(const char *how, uint32_t start, int fast, uint32_t need)
{
    uint32_t total, i, k, src, want = start, wrong = 0, hid = 0, wraps = 0, pos;
    char w[160];
    load_at(start);
    pos = list_cur(&total);
    if (pos != start) wrong++;
    for (i = 0; i < 40u; i++) {
        int32_t dir = i < 20u ? 1 : -1;
        uint32_t t2;
        click(dir, fast);
        want = (want + (dir > 0 ? 1u : total - 1u)) % total;
        wraps += dir > 0 ? want == 0u : want == total - 1u;
        pos = fast ? preset_pos(&t2) : list_cur(&t2);
        if (pos != want || t2 != total || (fast && i % 20u && !browse_pending())) wrong++;   /* (a turn's first detent, either way, loads at once) */
        src = browse_shown(&k);
        if (hidden_without(src, k) || !entry_plays(src, k)) hid++;
        if (fast && i == 19u) { rest(); if (list_cur(&t2) != want) wrong++; }
    }
    if (fast) rest();
    if (list_cur(&total) != start) wrong++;
    snprintf(w, sizeof w, "%s: 20 clicks right, 20 left from entry %u of %u: one listed sound a click, %u wraps, none hidden", how, start, total, wraps);
    chk(w, !wrong && !hid && wraps >= need);
}

int main(void)
{
    uint32_t total, i, k, src, n, ok, as, ak;
    up_rec_t rec[4];
    setvbuf(stdout, NULL, _IONBF, 0);
    reset();
    settings_init();
    oc_host_fw = &FW_API;
    models = oc_host_models(&nmodels);
    fake_shapes = ENG_DRUMX; fake_shapes.name = "SHAPES"; h_shapes = hdr(ENGI_SHAPES);
    fake_wheel = ENG_DRUMX; fake_wheel.name = "WHEEL"; h_wheel = hdr(ENGI_WHEEL);
    song.sel = 0;
    song.ntrk = 1;
    list_set(LM_ALL);

    /* the user presets: 0 MACRO on VA (built in), 1 MACRO on MALLET (model 10, the MODELS module's), 2 a SHAPES sound, 3 a WHEEL sound */
    host_preset(&trk[0], ENGI_MACRO, 0);
    ok = up_store_to(0, 0, "MY VA") == 0;
    host_preset(&trk[0], ENGI_MACRO, 18);
    ok &= up_store_to(1, 0, "MY MALLET") == 0;
    rec[0] = *up_rec(0);
    rec[0].engine = ENGI_SHAPES; memcpy(rec[0].name, "MY SHAPES\0\0\0", 12);
    ok &= up_put(2, &rec[0]) == 0;
    rec[0].engine = ENGI_WHEEL; memcpy(rec[0].name, "MY WHEEL\0\0\0\0", 12);
    ok &= up_put(3, &rec[0]) == 0;
    up_cache_reset();                                /* (as at power-on: the slots' index is read again) */
    for (i = 0; i < 4u; i++) rec[i] = *up_rec(i);
    chk("four user presets stored (MACRO on VA, MACRO on MALLET, SHAPES, WHEEL), all used, of their engines",
        ok && up_used(0) && up_used(1) && up_used(2) && up_used(3) && up_engine(1) == ENGI_MACRO && up_model(1) == 10u && up_model(0) == 0u &&
        up_engine(2) == ENGI_SHAPES && up_engine(3) == ENGI_WHEEL);

    /* 1. no module: what ALL lists */
    chk("no module: ALL lists the user preset on VA", listed(LM_ALL, USER_GENERAL, 0));
    chk("no module: ALL does not list the user preset on MALLET (MACRO, its model not installed)", !listed(LM_ALL, USER_GENERAL, 1));
    chk("no module: ALL does not list the SHAPES user preset", !listed(LM_ALL, USER_GENERAL, 2));
    chk("no module: ALL does not list the WHEEL user preset", !listed(LM_ALL, USER_GENERAL, 3));
    chk("no module: MACRO's factory VA BASS and VA SYNC LEAD listed; MALLET (model 10) and TERRAIN DRONE (model 20) not",
        listed(LM_ALL, ENGI_MACRO, 0) && listed(LM_ALL, ENGI_MACRO, 1) && !listed(LM_ALL, ENGI_MACRO, 18) && !listed(LM_ALL, ENGI_MACRO, 36));
    list_scan(LM_ALL, 0xFFFFFFFFu, 0, 0xFFFFFFFFu, &as, &ak, &total);
    for (i = 0, n = 0; i < total; i++) { src = preset_at(i, &k); n += hidden_without(src, k); }
    chk("no module: no entry of ALL is one that cannot play", total > 0u && n == 0u);

    /* 2. RECENT */
    recent_n = 0;
    recent_push(USER_GENERAL, 3); recent_push(USER_GENERAL, 2); recent_push(USER_GENERAL, 1); recent_push(ENGI_MACRO, 18); recent_push(USER_GENERAL, 0);
    list_scan(LM_RECENT, 0xFFFFFFFFu, 0, 0xFFFFFFFFu, &as, &ak, &total);
    chk("no module: RECENT of five lists only the VA user preset (MALLET, SHAPES, WHEEL and the factory MALLET hidden)",
        total == 1u && listed(LM_RECENT, USER_GENERAL, 0) && !listed(LM_RECENT, USER_GENERAL, 2) && !listed(LM_RECENT, ENGI_MACRO, 18));

    /* 3. FAV */
    up_favorite_set(2, 1); up_favorite_set(0, 1);
    list_scan(LM_FAV, 0xFFFFFFFFu, 0, 0xFFFFFFFFu, &as, &ak, &total);
    chk("no module: FAV with the SHAPES and the VA user presets starred lists the VA one alone", total == 1u && listed(LM_FAV, USER_GENERAL, 0) && !listed(LM_FAV, USER_GENERAL, 2));

    /* 4. the modules registered: everything back; unregistered: hidden again, nothing stored changed */
    modules(1);
    chk("modules registered: the MALLET, SHAPES and WHEEL user presets are listed again in ALL",
        listed(LM_ALL, USER_GENERAL, 1) && listed(LM_ALL, USER_GENERAL, 2) && listed(LM_ALL, USER_GENERAL, 3));
    chk("modules registered: MACRO's MALLET and TERRAIN DRONE factory presets listed", listed(LM_ALL, ENGI_MACRO, 18) && listed(LM_ALL, ENGI_MACRO, 36));
    list_scan(LM_RECENT, 0xFFFFFFFFu, 0, 0xFFFFFFFFu, &as, &ak, &total);
    list_scan(LM_RECENT, 0xFFFFFFFFu, 0, 0, &src, &k, &n);
    chk("modules registered: RECENT lists all five again, in their order", total == 5u && src == USER_GENERAL && k == 0u);
    chk("modules registered: FAV lists the SHAPES user preset again", listed(LM_FAV, USER_GENERAL, 2));
    modules(0);
    list_scan(LM_RECENT, 0xFFFFFFFFu, 0, 0xFFFFFFFFu, &as, &ak, &total);
    for (i = 0, ok = 1; i < 4u; i++) { up_rec_t r = *up_rec(i); r.used = rec[i].used; ok &= up_used(i) && !memcmp(&r, &rec[i], sizeof r); }   /* (the stars set above aside) */
    chk("modules removed: hidden again (ALL, RECENT 1); the four records are stored unchanged",
        !listed(LM_ALL, USER_GENERAL, 1) && !listed(LM_ALL, USER_GENERAL, 2) && !listed(LM_ALL, USER_GENERAL, 3) && total == 1u && ok);
    up_favorite_set(2, 0); up_favorite_set(0, 0);

    /* 5. the 20-click walk on ALL, no module: from its last entry (the walk wraps past the end and back) and from VA BASS */
    list_set(LM_ALL);
    list_scan(LM_ALL, 0xFFFFFFFFu, 0, 0xFFFFFFFFu, &as, &ak, &total);
    walk("ALL, slow clicks (each loads)", total - 1u, 0, 2u);
    walk("ALL, fast clicks (pending, loads at rest)", total - 1u, 1, 2u);
    walk("ALL, slow clicks from the first entry", 0, 0, total <= 20u ? 2u : 1u);
    walk("ALL, fast clicks from the first entry", 0, 1, total <= 20u ? 2u : 1u);

    /* 6. a project's track on a missing engine and on a missing model: kept, MODULE MISSING, a turn goes to its neighbours */
    {
        uint32_t near_r, near_l, t2, first_after = 0xFFFFFFFFu, last_before = 0xFFFFFFFFu, oS = ord(ENGI_SHAPES);
        modules(0);
        eng_register(&h_wheel, &fake_wheel);         /* (WHEEL listed: SHAPES has neighbours on both sides) */
        list_scan(LM_ALL, 0xFFFFFFFFu, 0, 0xFFFFFFFFu, &as, &ak, &total);
        for (i = 0; i < total; i++) {
            src = preset_at(i, &k);
            if (ord(src_engine(src, k)) > oS && first_after == 0xFFFFFFFFu) first_after = i;
            if (ord(src_engine(src, k)) < oS) last_before = i;
        }
        host_preset(&trk[0], ENGI_DRUMX, 0);
        trk[0].eng_req = ENGI_SHAPES; trk[0].preset = 3;
        trk[0].p[P_E1] = 77;
        song.ntrk = 2;
        host_preset(&trk[1], ENGI_MACRO, 18);        /* MALLET: model 10, not installed */
        ok = project_save(0) == 0;
        if (!ok) printf("browse filter: (the save says '%s')\n", ui.msg);
        trk[0].eng_req = 0; trk[0].preset = 0; trk[0].p[P_E1] = 0;
        project_load(0);
        song.sel = 0;
        ui.msg[0] = 0; ui.msg_t = 0; pack_notice(1);
        chk("a project's track on SHAPES (not installed): loaded with its engine, preset and settings, says MODULE MISSING, not in the list",
            ok && trk[0].eng_req == ENGI_SHAPES && trk[0].preset == 3u && trk[0].p[P_E1] == 77 && !strcmp(ui.msg, "MODULE MISSING") &&
            list_cur(&total) == total && first_after < total && last_before < total && first_after != 0u && last_before != total - 1u);
        list_forget(); list_set(LM_ALL);
        click(-1, 0);
        near_l = list_cur(&t2);
        chk("... PRESETS left from it loads the entry before SHAPES' place (MACRO's last listed), not the end of the list", near_l == last_before);
        project_load(0); list_set(LM_ALL); song.sel = 0;
        click(1, 0);
        near_r = list_cur(&t2);
        chk("... PRESETS right from it loads the entry after SHAPES' place (WHEEL's first: the stand-in), not the start of the list",
            near_r == first_after && trk[0].eng_req == ENGI_WHEEL);
        eng_unregister(ENGI_WHEEL);
        song.sel = 1; list_set(LM_ALL);
        ui.msg[0] = 0; ui.msg_t = 0; pack_notice(1);
        chk("a project's MACRO track on MALLET (model not installed): kept (model 10), MODULE MISSING, not in the list",
            trk[1].eng_req == ENGI_MACRO && trk[1].p[P_E0] == 10 && !strcmp(ui.msg, "MODULE MISSING") && list_cur(&total) == total);
        click(1, 0);
        src = browse_shown(&k);
        chk("... PRESETS right loads the next listed after MALLET's place (past MACRO's two VA presets), not the first entry",
            list_cur(&t2) != 0u && !(src == ENGI_MACRO && k < 2u) && !hidden_without(src, k) && ord(src_engine(src, k)) >= ord(ENGI_MACRO));
        host_preset(&trk[1], ENGI_MACRO, 18);
        click(-1, 0);
        src = browse_shown(&k);
        chk("... PRESETS left loads MACRO's VA SYNC LEAD (the listed one before MALLET)", src == ENGI_MACRO && k == 1u);
    }

    /* 7. (review of 497138ce) a stored engine no engine fills (4; 13, SLICE not built) is hidden, not listed as the fallback engine's sound */
    {
        up_rec_t r = rec[0];
        r.engine = 4u; memcpy(r.name, "MY FOUR\0\0\0\0\0", 12);
        ok = up_put(5, &r) == 0;
        r.engine = ENGI_SLICE; memcpy(r.name, "MY SLICE\0\0\0\0", 12);
        ok &= up_put(6, &r) == 0;
        up_cache_reset();
        recent_n = 0; recent_push(USER_GENERAL, 5); recent_push(USER_GENERAL, 6);
        list_scan(LM_RECENT, 0xFFFFFFFFu, 0, 0xFFFFFFFFu, &as, &ak, &total);
        chk("user presets stored on engine 4 and on 13 (SLICE not built): kept, not listed in ALL nor RECENT (were: as the fallback engine's)",
            ok && up_used(5) && up_used(6) && !listed(LM_ALL, USER_GENERAL, 5) && !listed(LM_ALL, USER_GENERAL, 6) && total == 0u);
        up_put(5, 0); up_put(6, 0);
    }

    /* 8. (review of 497138ce) the lists change inside a fast turn's rest: what loads is what was shown */
    {
        uint32_t target, s0, k0, s1, k1, t2;
        modules(0);
        song.sel = 0; list_set(LM_ALL);
        host_preset(&trk[0], ENGI_MACRO, 0);
        target = list_scan(LM_ALL, USER_GENERAL, 0, 0xFFFFFFFFu, &as, &ak, &total);   /* MY VA, after MACRO's two VA presets */
        load_at(target - 2u);
        click(1, 0);
        click(1, 1);
        s0 = browse_shown(&k0);
        eng_models_set(ENGI_MACRO, models, nmodels, oc_host_banks());   /* (the 45 other MACRO presets now come before it) */
        s1 = browse_shown(&k1);
        ok = browse_pending() && s0 == USER_GENERAL && k0 == 0u && s1 == s0 && k1 == k0;
        rest();
        cur_entry(&s1, &k1);
        chk("a fast turn pending on MY VA, the MODELS module registered inside the rest: still shown, and MY VA is what loads",
            ok && s1 == USER_GENERAL && k1 == 0u && preset_pos(&t2) == list_cur(&total));
        list_scan(LM_ALL, ENGI_MACRO, 18, 0xFFFFFFFFu, &as, &ak, &total);
        load_at(list_scan(LM_ALL, ENGI_MACRO, 16, 0xFFFFFFFFu, &as, &ak, &total));
        click(1, 0);
        click(1, 1);                                 /* pending: MALLET (18) */
        s0 = browse_shown(&k0);
        eng_models_clear(ENGI_MACRO);                /* MALLET can no longer play: the pending choice is dropped */
        cur_entry(&s1, &k1);
        ok = s0 == ENGI_MACRO && k0 == 18u && !browse_pending() && s1 == ENGI_MACRO && k1 == 17u;
        rest();
        cur_entry(&s1, &k1);
        chk("... pending on MALLET, the module removed inside the rest: the choice is dropped, the loaded sound stays (nothing else loads)",
            ok && s1 == ENGI_MACRO && k1 == 17u);
    }

    /* 9. the cost: one scan of ALL (the loaded sound's place) with all 384 user slots used, every click does one or two */
    {
        struct timespec a, b;
        uint32_t reps = 2000u;
        double us;
        host_preset(&trk[0], ENGI_MACRO, 0);
        song.sel = 0;
        for (i = 4; i < UP_SLOTS; i++) up_store_to(i, 0, "FILL");
        up_cache_reset();
        list_cur(&total);
        clock_gettime(CLOCK_MONOTONIC, &a);
        for (i = 0; i < reps; i++) list_cur(&total);
        clock_gettime(CLOCK_MONOTONIC, &b);
        us = ((b.tv_sec - a.tv_sec) * 1e9 + (b.tv_nsec - a.tv_nsec)) / 1e3 / reps;
        printf("browse filter: cost: one ALL scan, 384 user slots used (%u listed): %.2f us on the host (sanitizers)\n", total, us);
    }
    printf(bad ? "browse filter: %d FAILED\n" : "browse filter: all passed\n", bad);
    return bad != 0;
}
