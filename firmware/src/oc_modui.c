/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* Release 3, step 3 (docs/orangecat/MODULE_FORMAT_R3.md 5): a module's picture and HOME's four knobs, from its fw_pages_t version 3 (fw_ui_t, fw_kdef_t, fw_pid_t), into
 * the RAM registry of oc_knobs.c. Included by oc_module.c; kept apart from it so that the module area's changes (flash map step 1) and this one merge as two edits.
 *   modui_publish   at REGISTRATION (the main loop; oc_mod_boot, after the engine is registered): fw_ui_check, then each knob declaration resolved to a track parameter:
 *                   the pid's legacy slot (fw_pid_t.legacy), the range copied from the panel entry. A picture or knob table that fails is NOT a refusal: the record says
 *                   why (modui[].why, FWU_ codes) and the engine keeps the old HOME. A pid the table lacks (or one with no legacy slot) is dropped here, never later: a
 *                   macro keeps its other targets, a plain knob with no target is dropped whole (kind none).
 *   modui_retire    before a module's flash is erased or replaced (oc_mod_unreg): the record cleared under motion_guard, so the interrupt never reads a retired one.
 * Plain knobs are resolved to track parameters as macro targets are (HOME edits P_ ids); a plain knob on a native-panel byte waits for the project format and for
 * Session A's parameter-copy rule (MODULE_FORMAT_R3.md 5, "Native and effect-store macro targets wait").
 * The reader: rd(ctx, addr, buf, n) as fw_pages_check's (addresses of the module's own space). `img` and `link`: the scene's pointer is img + (address - link) (on the
 * unit img = link: the module runs in place). */
#if OC_HOME_PIC
static void modui_retire(uint32_t e)
{
    uint32_t f, i;
    f = motion_guard();                                 /* (the interrupt not in between) */
    for (i = 0; i < OC_MODUI_N; i++)
        if (modui[i].used && modui[i].eng == e) {
            modui[i].used = 0;
            modui[i].nk = 0;
            modui[i].scene = 0;
            modui[i].scene_len = 0;
            modui_gen++;
        }
    motion_unguard(f);
}
#ifdef OC_SCENE_C
static int modui_scene_ok(const uint8_t *s, uint32_t n) { return scene_ok(s, n); }
#else
static int modui_scene_ok(const uint8_t *s, uint32_t n) { (void)s; (void)n; return 0; }   /* (a unit under test without the interpreter: no picture is drawable) */
#endif
/* returns the picture's code (0 = in use; else fw_ui_check's, or FWU_FULL); the record is published either way when there is room */
static uint32_t modui_publish(uint32_t e, fw_pages_rd rd, void *ctx, uint32_t desc, uint32_t lo, uint32_t hi, const uint8_t *img, uint32_t link)
{
    modui_t m;                                          /* (the record is built here, then copied in under the guard) */
    uint8_t d[FW_PAGES_WIRE], k[FW_KDEF_WIRE], q[FW_PID_WIRE], p[FW_PAGES_DESC_WIRE];
    fw_ui_info_t ui;
    uint32_t i, j, t, np, pidp, panel, code, slot, f;
    memset(&m, 0, sizeof m);
    m.eng = (uint8_t)e;
    if (!fwp_in(desc, FW_PAGES_WIRE, lo, hi) || rd(ctx, desc, d, FW_PAGES_WIRE))
        return FWU_READ;                                /* (the check ran before; a read that fails now: nothing published) */
    np = d[10] | d[11] << 8; panel = fwp_u32(d + 16); pidp = fwp_u32(d + 72);
    code = (uint32_t)fw_ui_check(rd, ctx, fwp_u32(d + 68), lo, hi, modui_scene_ok, &ui);
    if (!code && ui.nk) {
        for (i = 0; i < FW_KDEF_N; i++) {
            if (rd(ctx, ui.kdef + i * FW_KDEF_WIRE, k, FW_KDEF_WIRE)) { code = FWU_READ; break; }
            for (j = 0; j < 5u; j++) m.lbl[i][j] = (char)k[j];
            m.kd[i].label = m.lbl[i];
            m.kd[i].kind = KD_NONE;
            for (t = 0; t < KD_TGT; t++) { m.kd[i].t[t].id = KD_NOT; m.kd[i].t[t].shape = 0; m.kd[i].t[t].depth = 0; }
            if (k[6] == 1u || k[6] == 2u) {
                for (t = 0; t < (k[6] == 1u ? 1u : KD_TGT); t++) {
                    uint32_t want = k[6] == 1u ? (uint32_t)(k[8] | k[9] << 8) : (uint32_t)(k[10 + 4 * t] | k[11 + 4 * t] << 8), at = np, leg;
                    if (want == FW_PID_NONE || !pidp) continue;
                    for (j = 0; j < np; j++) {          /* the entry of that id; its legacy slot and its range */
                        if (rd(ctx, pidp + j * FW_PID_WIRE, q, FW_PID_WIRE)) break;
                        if ((uint32_t)(q[0] | q[1] << 8) == want) { at = j; break; }
                    }
                    if (at >= np) continue;             /* (a pid the table lacks: dropped here, never at draw or interrupt time) */
                    leg = q[2] | q[3] << 8;
                    if (leg == FW_PID_NONE || leg < P_E0 || leg > P_E7 ||   /* (review: only the engine's own E0 .. E7 may be a knob's target; a legacy slot is any track slot, and a knob never switches what the sound is) */
                        rd(ctx, panel + at * FW_PAGES_DESC_WIRE, p, FW_PAGES_DESC_WIRE)) continue;
                    if (k[6] == 1u) {
                        m.kd[i].kind = KD_PLAIN;
                        m.kd[i].id = (uint8_t)leg;
                    } else {
                        const param_desc_t *pd = param_desc_of(e, leg);   /* (review) the range is the panel entry's cut to the slot's own, so a module cannot widen it */
                        int32_t mn = (int16_t)(p[6] | p[7] << 8), mx = (int16_t)(p[8] | p[9] << 8);
                        if (mn < pd->min) mn = pd->min;
                        if (mx > pd->max) mx = pd->max;
                        if (mn > mx) continue;
                        m.kd[i].kind = KD_MACRO;
                        m.kd[i].t[t].id = (uint8_t)leg;
                        m.kd[i].t[t].shape = k[12 + 4 * t];
                        m.kd[i].t[t].depth = (int8_t)k[13 + 4 * t];
                        m.rng[i * KD_TGT + t][0] = (int16_t)mn;
                        m.rng[i * KD_TGT + t][1] = (int16_t)mx;
                    }
                }
                if (k[6] == 2u && m.kd[i].kind != KD_MACRO) m.kd[i].kind = KD_NONE;   /* (a macro with no target left) */
            }
        }
    }
    if (code) { m.nk = 0; m.scene = 0; m.scene_len = 0; }
    else if (ui.nk) {
        m.nk = FW_KDEF_N;
        if (ui.scene_len) { m.scene = img + (ui.scene - link); m.scene_len = ui.scene_len; }
    }
    m.why = (uint8_t)code;
    if (!m.nk && !code) return 0;                       /* (no picture and no knobs, as declared: the old HOME; nothing to publish) */
    f = motion_guard();
    for (slot = 0; slot < OC_MODUI_N && modui[slot].used; slot++) ;
    if (slot < OC_MODUI_N) { m.used = 1; modui[slot] = m; modui[slot].kd[0].label = modui[slot].lbl[0];
        for (i = 0; i < 4u; i++) modui[slot].kd[i].label = modui[slot].lbl[i];
        modui_gen++; }
    motion_unguard(f);
    if (slot >= OC_MODUI_N) return FWU_FULL;
    return code;
}
#else
static void modui_retire(uint32_t e) { (void)e; }
#endif
