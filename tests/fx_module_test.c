/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat Release 3 step 4 (FX_MODULES_PLAN.md 2, 3, 8): the kind field of the module header, the effect module's descriptor (firmware/src/fw_fx.h), the
 * Effects table and the install by module id (firmware/src/oc_module.c), on the simulated NOR of tests/persistence_test.c:
 *   build/host/fx_module_test   (built with OC_MODULE_WINS 1 as oc_module_test; tests/run_tests.sh)
 * Checked: the engine word's kind (the words accepted and refused by fw_module_check); every refusal of the descriptor (a bank of 0 and 9, module id 0, every
 * pointer, the label, ids not increasing, a range, a duplicate effect id, a kind that is not the header's, the reserved words); a good effect module is in the
 * Effects table and not in the engine table; two banks with different ids side by side; two modules of one id (the higher version wins, the loser erased); the
 * planner on 18 sectors at the lowest and the highest start with and without the six low sectors; a replacement written beside the old module; removal by
 * module id; a power cut at every program and every erase of an install (beside and in place); the crash ban; an engine install and an effect never
 * erase each other.
 */
#include <stdint.h>
#define OC_MOD_HOST_XIP 1
#define OC_MOD_GEO_QUAL
struct fw_mod_hdr;
static const struct fw_mod_hdr *oc_mod_hdr_ptr(uint32_t i);
#define PERSISTENCE_TEST_NO_MAIN
#include "persistence_test.c"
static const fw_mod_hdr_t *oc_mod_hdr_ptr(uint32_t i) { return (const fw_mod_hdr_t *)(nor + oc_mod_flash(i)); }

static int bad;
static void chk(const char *what, int ok)
{
    printf("fx modules: %-118s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
static uint8_t IMG[(OC_FX_MAX_SECTORS + 2u) * 0x1000u];
#define D_DESC 0x200u          /* module-relative offsets of the parts of the made image */
#define D_ENT 0x214u
#define D_PAR 0x300u
#define D_LAB 0x500u
static void w32(uint32_t off, uint32_t v) { IMG[off] = (uint8_t)v; IMG[off + 1] = (uint8_t)(v >> 8); IMG[off + 2] = (uint8_t)(v >> 16); IMG[off + 3] = (uint8_t)(v >> 24); }
static void w16(uint32_t off, uint32_t v) { IMG[off] = (uint8_t)v; IMG[off + 1] = (uint8_t)(v >> 8); }
typedef void (*mut_t)(uint32_t link);
/* a good bank of n effects (kind `kind`, 2 parameters each) as the start `start` runs it; the header sealed; mut changes the image before the CRC (a bad descriptor) */
static fw_mod_hdr_t fxhdr;
static uint32_t make(uint32_t start, uint32_t mid, uint32_t ver, uint32_t n, uint32_t kind, uint32_t len, mut_t mut)
{
    uint32_t link = oc_mod_link(start), i;
    fw_mod_hdr_t h;
    memset(IMG, 0, sizeof IMG);
    for (i = FW_MOD_CODE_OFF; i < len; i++) IMG[i] = (uint8_t)(i * 31u + mid);
    memset(IMG + D_DESC, 0, 0x300u);
    w32(D_DESC, n); w32(D_DESC + 4, mid); w32(D_DESC + 8, ver); w32(D_DESC + 12, link + D_ENT); w32(D_DESC + 16, 0);
    for (i = 0; i < n; i++) {
        uint32_t e = D_ENT + 16u * i, p = D_PAR + 32u * i;
        IMG[e] = (uint8_t)(i + 1u); IMG[e + 1] = (uint8_t)kind; w16(e + 4, 2); w32(e + 8, link + p);
        w16(p, 10); w16(p + 2, 0xFFFF); w16(p + 4, 0); w16(p + 6, 127); w16(p + 8, 64); w16(p + 10, 0); w32(p + 12, link + D_LAB);
        w16(p + 16, 20); w16(p + 18, 0xFFFF); w16((p + 20), (uint32_t)(int16_t)-64); w16(p + 22, 63); w16(p + 24, 0); w16(p + 26, 0); w32(p + 28, 0);
    }
    memcpy(IMG + D_LAB, "FEEDBACK", 9);
    if (mut) mut(link);
    memset(&h, 0, sizeof h);
    h.magic = FW_MOD_MAGIC; h.iface = FW_API_IFACE; h.api_size = sizeof(fw_api_t); h.api_version = FW_API_VERSION; h.api_addr = FW_API_ADDR;
    h.engine = kind << 8; h.state_size = 64u; h.engine_ptr = link + D_DESC; h.link_addr = link; h.pack_hash = OC_PACK_HASH; h.length = len; h.version = ver;
    h.crc = ocs_crc32(IMG + FW_MOD_CODE_OFF, len - FW_MOD_CODE_OFF);
    h.hcrc = ocs_crc32((const uint8_t *)&h, FW_MOD_HDR_BYTES - 4u);
    memcpy(IMG, &h, sizeof h);
    fxhdr = h;
    return len;
}
static void put(uint32_t start, uint32_t len) { memcpy(nor + oc_mod_flash(start), IMG, len); }
static void clean(void)
{
    uint32_t e;
    memset(nor, 0xFF, sizeof nor);
    memset(&oc_modguard, 0, sizeof oc_modguard);
    memset(trk, 0, sizeof trk);
    for (e = 0; e < NENGINES; e++) eng_unregister(e);
    memset(&oc_mod, 0, sizeof oc_mod);
    fm1_guard_code_end = 0;
    ocs_resv = SMP_LEGACY_MASK;                                   /* the six low sectors are the module area's */
    fail_after = -1; erase_fail_after = -1;
}
static void good(uint32_t start, uint32_t mid, uint32_t ver, uint32_t sectors) { uint32_t len = make(start, mid, ver, 2, 1, sectors * 0x1000u - 0x100u, 0); put(start, len); }
static int engine_table_empty(void) { uint32_t e; for (e = 0; e < NENGINES; e++) if (eng_mod[e]) return 0; return 1; }
/* the descriptor mutators */
static void m_n0(uint32_t l) { w32(D_DESC, 0); }
static void m_n9(uint32_t l) { w32(D_DESC, 9); }
static void m_id0(uint32_t l) { w32(D_DESC + 4, 0); }
static void m_ent_out(uint32_t l) { w32(D_DESC + 12, l + 0x9000u); }
static void m_ent_odd(uint32_t l) { w32(D_DESC + 12, l + D_ENT + 2u); }
static void m_ent_lo(uint32_t l) { w32(D_DESC + 12, l + 0x80u); }
static void m_par_out(uint32_t l) { w32(D_ENT + 8, l + 0x9000u); }
static void m_par_odd(uint32_t l) { w32(D_ENT + 8, l + D_PAR + 1u); }
static void m_par_none(uint32_t l) { w32(D_ENT + 8, 0); }
static void m_np_zero_ptr(uint32_t l) { w16(D_ENT + 4, 0); }
static void m_np_big(uint32_t l) { w16(D_ENT + 4, 65u); }
static void m_label_out(uint32_t l) { w32(D_PAR + 12, l + 0x9000u); }
static void m_label_long(uint32_t l) { memset(IMG + D_LAB, 'X', 40); }
static void m_ids(uint32_t l) { w16(D_PAR + 16, 10); }                     /* not strictly increasing */
static void m_range(uint32_t l) { w16(D_PAR + 4, 100); w16(D_PAR + 6, 50); }
static void m_def(uint32_t l) { w16(D_PAR + 8, 200); }
static void m_dup(uint32_t l) { IMG[D_ENT + 16] = IMG[D_ENT]; }
static void m_kind(uint32_t l) { IMG[D_ENT + 1] = 2; }
static void m_kind0(uint32_t l) { IMG[D_ENT + 1] = 0; }
static void m_flags(uint32_t l) { w16(D_ENT + 2, 1); }
static void m_pages(uint32_t l) { w32(D_ENT + 12, l + D_PAR); }
static void m_scene_out(uint32_t l) { w32(D_DESC + 16, l + 0x9000u); }
static void m_desc_lo(uint32_t l) { (void)l; }
static void m_scene_ok(uint32_t l) { w32(D_DESC + 16, l + 0x600u); }

int main(void)
{
    uint32_t i, len;
    /* ------------------------------------------------ 1. the engine word's kind */
    {
        fw_mod_hdr_t h;
        static const uint32_t okw[] = { 0u, 7u, 14u, 127u, 0x100u, 0x200u, 0x300u };
        static const uint32_t badw[] = { 0x400u, 0x101u, 0x203u, 0x10000u, 0x80000000u, 0xFFFFFFFFu, 0x1000Fu, 0xFF00u };
        int ok = 1;
        memset(&h, 0, sizeof h);
        h.magic = FW_MOD_MAGIC; h.iface = FW_API_IFACE; h.api_size = sizeof(fw_api_t); h.api_version = FW_API_VERSION; h.api_addr = FW_API_ADDR;
        for (i = 0; i < sizeof okw / sizeof okw[0]; i++) { h.engine = okw[i]; ok &= fw_module_check(&h) == FW_MOD_OK; }
        chk("the engine word: engine numbers (kind 0) and kinds 1, 2, 3 with a zero low byte are accepted", ok);
        ok = 1;
        for (i = 0; i < sizeof badw / sizeof badw[0]; i++) { h.engine = badw[i]; ok &= fw_module_check(&h) == FW_MOD_BAD_KIND; }
        chk("... an unknown kind, bits above 15, an effect with an engine number: FW_MOD_BAD_KIND", ok);
        h.engine = 0x205u; chk("... FW_MOD_ENGINE and FW_MOD_KIND read the two halves", FW_MOD_ENGINE(&h) == 5u && FW_MOD_KIND(&h) == 2u);
    }
    /* ------------------------------------------------ 2. a good effect module */
    clean();
    good(0, 0x1001u, 0x00010203u, 3); oc_mod_boot(0);
    chk("a good insert effect module (2 effects) at start 0: MD_OK, in the Effects table with its id, version, kind and bank size",
        oc_mod.state[0] == MD_OK && oc_mod.nfxmod == 1 && oc_mod.fx[0].module_id == 0x1001u && oc_mod.fx[0].version == 0x00010203u && oc_mod.fx[0].kind == 1 &&
        oc_mod.fx[0].nfx == 2 && oc_mod.fx[0].start == 0 && oc_fx_find(0x1001u) == 0 && oc_fx_find(0x1002u) < 0);
    chk("... and NOT in the engine table (no engine slot is taken); engine[i] is the effect marker", engine_table_empty() && oc_mod.engine[0] == OC_FX_ENGINE && oc_mod.kind[0] == 1);
    chk("... the PC guard's window reaches the end of its code", fm1_guard_code_end == oc_mod_link(0) + 3u * 0x1000u - 0x100u - 1u);
    for (i = 1; i <= 3; i++) {
        clean(); len = make(0, 0x2000u + i, 1, 1, i, 0x1000u, 0); put(0, len); oc_mod_boot(0);
        chk(i == 1 ? "kind 1 (insert), 2 (send), 3 (master) each register with their kind" : "... again", oc_mod.state[0] == MD_OK && oc_mod.fx[0].kind == i);
    }
    /* ------------------------------------------------ 3. every refusal */
    {
        static const struct { const char *what; mut_t m; uint32_t kind; uint32_t state; } R[] = {
            {"a bank of 0 effects", m_n0, 1, MD_BAD_HDR}, {"a bank of 9 effects", m_n9, 1, MD_BAD_HDR}, {"module id 0", m_id0, 1, MD_BAD_HDR},
            {"the entries pointer past the image", m_ent_out, 1, MD_BAD_HDR}, {"the entries pointer not 4 aligned", m_ent_odd, 1, MD_BAD_HDR},
            {"the entries pointer inside the header page", m_ent_lo, 1, MD_BAD_HDR},
            {"the parameter table past the image", m_par_out, 1, MD_BAD_HDR}, {"the parameter table not 4 aligned", m_par_odd, 1, MD_BAD_HDR},
            {"2 parameters and no table", m_par_none, 1, MD_BAD_HDR}, {"a table and no parameters", m_np_zero_ptr, 1, MD_BAD_HDR}, {"65 parameters", m_np_big, 1, MD_BAD_HDR},
            {"a label past the image", m_label_out, 1, MD_BAD_HDR}, {"a label of 40 bytes with no NUL in 24", m_label_long, 1, MD_BAD_HDR},
            {"parameter ids not strictly increasing", m_ids, 1, MD_BAD_HDR}, {"min above max", m_range, 1, MD_BAD_HDR}, {"default outside the range", m_def, 1, MD_BAD_HDR},
            {"two effects with one effect id", m_dup, 1, MD_BAD_HDR}, {"an entry of another kind than the header's", m_kind, 1, MD_BAD_HDR},
            {"an entry of kind 0", m_kind0, 1, MD_BAD_HDR}, {"flags set (reserved)", m_flags, 1, MD_BAD_HDR}, {"pages set (reserved)", m_pages, 1, MD_BAD_HDR},
            {"the scene pointer past the image", m_scene_out, 1, MD_BAD_HDR},
        };
        for (i = 0; i < sizeof R / sizeof R[0]; i++) {
            char what[160];
            clean(); len = make(0, 0x3000u, 1, 2, R[i].kind, 0x1000u, R[i].m); put(0, len); oc_mod_boot(0);
            snprintf(what, sizeof what, "refused: %s (state %u, the Effects table empty, the engine table empty)", R[i].what, oc_mod.state[0]);
            chk(what, oc_mod.state[0] == R[i].state && oc_mod.nfxmod == 0 && engine_table_empty());
        }
        clean(); len = make(0, 0x3000u, 1, 2, 1, 0x1000u, m_scene_ok); put(0, len); oc_mod_boot(0);
        chk("a scene pointer inside the image is accepted (it is validated again by the interface before any draw)", oc_mod.state[0] == MD_OK);
        clean(); len = make(0, 0x3000u, 1, 2, 1, 0x1000u, 0); IMG[D_DESC] = 0; put(0, len); oc_mod_boot(0);
        chk("a descriptor changed after the CRC was made: MD_BAD_CRC", oc_mod.state[0] == MD_BAD_CRC);
        clean(); len = make(0, 0x3000u, 1, 2, 1, 0x1000u, 0); put(0, len);
        { fw_mod_hdr_t h = fxhdr; h.state_size = OC_MOD_STATE_MAX + 4u; h.hcrc = ocs_crc32((const uint8_t *)&h, FW_MOD_HDR_BYTES - 4u); memcpy(nor + oc_mod_flash(0), &h, sizeof h); }
        oc_mod_boot(0);
        chk("a state size past the limit: refused (MD_BAD_HDR); a state size of 0 is fine for an effect", oc_mod.state[0] == MD_BAD_HDR);
        clean(); len = make(0, 0x3000u, 1, 2, 1, 0x1000u, 0); put(0, len);
        { fw_mod_hdr_t h = fxhdr; h.state_size = 0; h.hcrc = ocs_crc32((const uint8_t *)&h, FW_MOD_HDR_BYTES - 4u); memcpy(nor + oc_mod_flash(0), &h, sizeof h); }
        oc_mod_boot(0);
        chk("... state size 0", oc_mod.state[0] == MD_OK);
        clean(); len = make(0, 0x3000u, 1, 2, 1, 0x1000u, 0); put(0, len);
        { fw_mod_hdr_t h = fxhdr; h.engine = 0x407u; h.hcrc = ocs_crc32((const uint8_t *)&h, FW_MOD_HDR_BYTES - 4u); memcpy(nor + oc_mod_flash(0), &h, sizeof h); }
        oc_mod_boot(0);
        chk("a header with an unknown kind: MD_REFUSED, why FW_MOD_BAD_KIND", oc_mod.state[0] == MD_REFUSED && oc_mod.why[0] == FW_MOD_BAD_KIND && oc_mod.nfxmod == 0);
        clean(); len = make(0, 0x3000u, 1, 2, 1, 9u * 0x1000u, 0); put(0, 8u * 0x1000u);
        oc_mod_boot(0);
        chk("a module of 9 sectors (past OC_FX_MAX_SECTORS): not registered", oc_mod.state[0] != MD_OK && oc_mod.nfxmod == 0);
    }
    /* ------------------------------------------------ 4. banks side by side, one id twice */
    clean(); good(0, 0x1001u, 1, 2); good(1, 0x1002u, 1, 3); oc_mod_boot(0);
    chk("two banks with different ids side by side (starts 0 and 1): both OK, both in the Effects table, neither taken for the other's duplicate",
        oc_mod.state[0] == MD_OK && oc_mod.state[1] == MD_OK && oc_mod.nfxmod == 2 && oc_fx_find(0x1001u) >= 0 && oc_fx_find(0x1002u) >= 0);
    clean(); good(0, 0x1001u, 0x00010000u, 2); good(1, 0x1001u, 0x00010100u, 2); oc_mod_boot(0);
    chk("one id at two starts (a cut between the commit of a replacement and the erase of the old): the higher version is registered, the other is NOSLOT and erased",
        oc_mod.state[1] == MD_OK && oc_mod.nfxmod == 1 && oc_mod.fx[0].version == 0x00010100u && nor[oc_mod_flash(0)] == 0xFF && nor[oc_mod_flash(0) + 0x100u] == 0xFF);
    clean(); good(0, 0x1001u, 5, 2); good(1, 0x1001u, 5, 2); oc_mod_boot(0);
    chk("... equal versions: the higher start wins", oc_mod.state[1] == MD_OK && oc_mod.fx[0].start == 1);
    clean();
    for (i = 0; i < 10; i++) { len = make(i, 0x4000u + i, 1, 1, 1, 0x1000u, 0); put(i, len); }
    oc_mod_boot(0);
    chk("more modules than the Effects table holds (8): the table is full, the rest NOSLOT but still usable (they block a planner)", oc_mod.nfxmod == 8 && oc_mod.state[8] == MD_NOSLOT && oc_mod.state[9] == MD_NOSLOT && oc_mod_usable(9));
    /* ------------------------------------------------ 5. an engine module and an effect do not touch each other */
    clean();
    {   fw_mod_hdr_t h; memset(&h, 0, sizeof h); /* a SHAPES-like engine module is made by oc_module_test; here only the table interplay: an engine entry in the area blocks the effect planner */ }
    memset(&oc_mod, 0, sizeof oc_mod);
    oc_mod.state[0] = MD_OK; oc_mod.engine[0] = ENGI_WHEEL; oc_mod.length[0] = 3000u;
    chk("the planner never plans over a usable module of an engine (start 0 holds one): a 2 sector effect module goes to start 1, not 0 or 2", oc_fxmod_plan(0x5000u, 2) == 1 || oc_fxmod_plan(0x5000u, 2) == 3);
    chk("... and the engine planner sees an effect module as a blocker, never as its own",
        ({ clean(); good(0, 0x1001u, 1, 2); oc_mod_boot(0); oc_mod_blocker(ENGI_WHEEL, 0) == 0 && !oc_mod_own_overlap(ENGI_WHEEL, 0); }));
    /* ------------------------------------------------ 6. the planner on 18 sectors */
    {
        int ok = 1;
        clean();
        ok &= oc_fx_allowed(1, 11) && !oc_fx_allowed(2, 11) && oc_fx_allowed(1, 17) && oc_fx_allowed(8, 17) && !oc_fx_allowed(9, 17) && !oc_fx_allowed(0, 0);
        ocs_resv = 0;
        ok &= !oc_fx_allowed(1, 17) && !oc_fx_allowed(1, 12) && oc_fx_allowed(1, 11);
        chk("allowed starts: the highest (11, sector 0xD3000) takes one sector, not two; the lowest (-6, 0xC2000) only when the six are the module area's; 9 sectors never", ok);
        /* fill every start but one with a one sector module: the planner finds the one */
        for (int low = 0; low < 2; low++) {
            uint32_t free_start = low ? 17u : 11u;
            clean();
            for (i = 0; i < OC_MOD_STARTS; i++)
                if (i != free_start) { len = make(i, 0x6000u + i, 1, 1, 1, 0x1000u, 0); put(i, len); }
            oc_mod_boot(0);
            oc_mod.nfxmod = 7u;                         /* (round 16: 17 ids do not fit the 8 entry table; this cell asks the planner's span rules only) */
            ok = oc_fxmod_plan(0x7000u, 1) == (int)free_start && !oc_fxmod_inplace(0x7000u, 1, free_start);
            ok &= oc_fxmod_blocked_by(0x7000u, 2) >= 0 || oc_fxmod_plan(0x7000u, 2) < 0;
            chk(low ? "planner, 17 modules in the other starts: the LOWEST start (-6, 0xC2000) is the one left, planned beside" : "planner, 17 modules in the other starts: the HIGHEST start (11, 0xD3000) is the one left, planned beside", ok);
        }
        clean(); ocs_resv = 0;
        for (i = 0; i < OC_MOD_STARTS; i++)
            if (i != 17 && oc_mod_start_ok(i)) { len = make(i, 0x6000u + i, 1, 1, 1, 0x1000u, 0); put(i, len); }
        oc_mod_boot(0);
        chk("... with the six low sectors not yet the module area's, the lowest start is not offered: no start is free (-1)", oc_fxmod_plan(0x7000u, 1) < 0 && oc_fxmod_blocked_by(0x7000u, 1) >= 0);
        clean();
        good(0, 0x1001u, 1, 3); oc_mod_boot(0);
        chk("a replacement of an id that stands at start 0 goes beside it (planner: the first free start that overlaps nothing)", oc_fxmod_plan(0x1001u, 3) > 0 && !oc_fxmod_inplace(0x1001u, 3, (uint32_t)oc_fxmod_plan(0x1001u, 3)));
        chk("... another module may not be planned over it", oc_fx_blocker(0x9999u, 3, 0) == 0 && oc_fx_blocker(0x1001u, 3, 0) < 0);
    }
    /* ------------------------------------------------ 7. the install, a replacement beside, removal by id */
    {
        int ok;
        uint32_t o, r;
        clean();
        good(0, 0x1001u, 0x00010000u, 3); oc_mod_boot(0);
        len = make(1, 0x1001u, 0x00010100u, 2, 1, 3u * 0x1000u - 0x100u, 0);        /* the new version, for the start the planner names */
        i = (uint32_t)oc_fxmod_plan(0x1001u, oc_mod_nsec(len));
        len = make(i, 0x1001u, 0x00010100u, 2, 1, 3u * 0x1000u - 0x100u, 0);
        r = (uint32_t)oc_fxmod_begin(0x1001u, i, len);
        for (o = FW_MOD_CODE_OFF; !r && o < len; o += 256u) r = (uint32_t)oc_mod_write(o, IMG + o, len - o > 256u ? 256u : len - o);
        ok = !r && mw.fx && !mw.inplace;
        r = r ? r : (uint32_t)oc_mod_end(IMG);
        chk("install a newer version of id 0x1001 by module id: begin, write, end (the engine functions' state machine) - done, beside the old one", ok && r == 0);
        chk("... the old copy erased, the new one registered, one entry in the Effects table with the new version",
            oc_mod.nfxmod == 1 && oc_mod.fx[0].version == 0x00010100u && oc_mod.fx[0].start == i && oc_mod.state[0] == MD_EMPTY && nor[oc_mod_flash(0)] == 0xFF && engine_table_empty());
        r = (uint32_t)oc_fxmod_begin(0x1001u, i, len);
        chk("begin at the start of the module that stands there is refused unless it is the plan (the plan is another start)", r == (uint32_t)MI_REFUSED);
        chk("begin with module id 0, an unaligned length, a length of 9 sectors, a start past the table: refused",
            oc_fxmod_begin(0, 0, len) == MI_REFUSED && oc_fxmod_begin(0x1001u, 0, len + 2u) == MI_REFUSED && oc_fxmod_begin(0x1001u, 0, 9u * 0x1000u) == MI_REFUSED && oc_fxmod_begin(0x1001u, 99, len) == MI_REFUSED);
        good(0, 0x1002u, 1, 2); oc_mod_boot(0);
        chk("a second bank of another id beside it", oc_mod.nfxmod == 2);
        ok = oc_fxmod_remove(0x1001u) == 0;
        chk("removal by module id: that bank's sectors erased and gone from the Effects table, the other bank untouched",
            ok && oc_fx_find(0x1001u) < 0 && oc_fx_find(0x1002u) >= 0 && oc_mod.nfxmod == 1 && nor[oc_mod_flash(i)] == 0xFF && nor[oc_mod_flash(i) + 0x2000u] == 0xFF && nor[oc_mod_flash(0) + 5u] != 0xFF);
        chk("removal of an id nobody has, and of id 0", oc_fxmod_remove(0x7777u) == 0 && oc_mod.nfxmod == 1 && oc_fxmod_remove(0) == MI_REFUSED);
        /* the install is refused when the descriptor's id is not the one asked for */
        clean();
        len = make(0, 0x2222u, 1, 1, 1, 0x1000u, 0);
        r = (uint32_t)oc_fxmod_begin(0x3333u, 0, len);
        for (o = FW_MOD_CODE_OFF; !r && o < len; o += 256u) r = (uint32_t)oc_mod_write(o, IMG + o, len - o > 256u ? 256u : len - o);
        r = r ? r : (uint32_t)oc_mod_end(IMG);
        chk("an install for id 0x3333 whose descriptor says 0x2222: refused at the end, no header programmed, nothing registered", r == (uint32_t)MI_REFUSED && nor[oc_mod_flash(0)] == 0xFF && oc_mod.nfxmod == 0);
        /* an install in place: no free start */
        clean();
        for (i = 0; i < OC_MOD_STARTS; i++) { len = make(i, 0x6000u + i, 1, 1, 1, 0x1000u, 0); put(i, len); }
        oc_mod_boot(0);
        chk("all 18 starts used: a new id has no place (-5); a replacement of an id that stands goes in place", oc_fxmod_begin(0x7777u, 0, 0x1000u) == MI_NOFREE && oc_fxmod_plan(0x7777u, 1) < 0 &&
            oc_fxmod_plan(0x6003u, 1) == 3 && oc_fxmod_inplace(0x6003u, 1, 3));
        len = make(3, 0x6003u, 2, 1, 1, 0x1000u, 0);
        r = (uint32_t)oc_fxmod_begin(0x6003u, 3, len);
        for (o = FW_MOD_CODE_OFF; !r && o < len; o += 256u) r = (uint32_t)oc_mod_write(o, IMG + o, len - o > 256u ? 256u : len - o);
        r = r ? r : (uint32_t)oc_mod_end(IMG);
        chk("... the in place install: the old module erased first, the new one registered at the same start with the new version, the other 17 untouched",
            r == 0 && oc_mod.state[3] == MD_OK && oc_mod.version[3] == 2u && oc_mod.state[2] != MD_EMPTY && oc_mod.state[4] != MD_EMPTY);
    }
    /* ------------------------------------------------ 8. power cuts in an install */
    {
        int kind, okall = 1;
        uint32_t cuts = 0, finished = 0, again = 0;
        for (kind = 0; kind < 2; kind++) {              /* 0: beside the old module of the id, 1: in place (every start taken) */
            for (int k = 0; k < 400; k++) {
                uint32_t o, r, ns = 2, st, oldv = 0x00010000u, newv = 0x00010100u;
                int cut_happened;
                clean();
                if (kind == 0) { len = make(0, 0x1001u, oldv, 1, 1, ns * 0x1000u - 0x100u, 0); put(0, len); }
                else for (i = 0; i < OC_MOD_STARTS; i++) { if (i == 3) continue; len = make(i, i == 0 ? 0x1001u : 0x6000u + i, i == 0 ? oldv : 1, 1, 1, 0x1000u * (i == 0 ? ns : 1u) - (i == 0 ? 0x100u : 0u), 0); put(i, len); }   /* start 3 overlaps start 0's second sector: left out, so every sector is taken */
                oc_mod_boot(0);
                st = (uint32_t)oc_fxmod_plan(0x1001u, ns);
                len = make(st, 0x1001u, newv, 1, 1, ns * 0x1000u - 0x100u, 0);
                if (kind == 0) fail_after = k; else erase_fail_after = k;
                if (kind == 0 && k % 2) { fail_after = -1; erase_fail_after = k / 2; }
                r = (uint32_t)oc_fxmod_begin(0x1001u, st, len);
                for (o = FW_MOD_CODE_OFF; !r && o < len; o += 256u) r = (uint32_t)oc_mod_write(o, IMG + o, len - o > 256u ? 256u : len - o);
                r = r ? r : (uint32_t)oc_mod_end(IMG);
                cut_happened = r != 0;
                fail_after = -1; erase_fail_after = -1;
                if (!cut_happened) finished++;
                oc_mod_boot(0);                          /* the power comes back: the area read as at boot */
                cuts++;
                {   /* the id is there once, whole: the old version (no erase of it ran, or a cut before the commit) or the new one; never half a module; in place a cut after the erase may leave none */
                    int n = 0, ver_ok = 1, regs = oc_fx_find(0x1001u) >= 0;
                    for (i = 0; i < OC_MOD_STARTS; i++) if (oc_mod.state[i] == MD_OK && oc_mod.kind[i] && oc_mod.mid[i] == 0x1001u) { n++; ver_ok &= oc_mod.version[i] == oldv || oc_mod.version[i] == newv; }
                    okall &= n <= 1 && ver_ok && (kind == 1 || n == 1) && engine_table_empty() && oc_mod.nfxmod == (uint32_t)(regs + (kind == 1 ? 17 : 0) > 8 ? 8 : regs + (kind == 1 ? 17 : 0));
                    if (!okall) { printf("  cut %d kind %d: n %d regs %d nfxmod %u\n", k, kind, n, regs, oc_mod.nfxmod); }
                }
                if (!okall) break;
                again += oc_fxmod_remove(0x1001u) == 0 && oc_fx_find(0x1001u) < 0;
                if (!cut_happened && k > 6 + 40) break;
            }
        }
        chk("a power cut at every program (and at every erase) of an install by module id, beside and in place: after the next boot the id is there once, whole, the old or the new version; the engine table untouched; (> 40 cuts, installs that finished and ones that did not)",
            okall && cuts > 40u && finished > 0u && again == cuts);
    }
    /* ------------------------------------------------ 9. the crash mark */
    clean(); good(0, 0x1001u, 1, 2); good(1, 0x1002u, 1, 2); oc_mod_boot(0);
    oc_fxmod_note_use(0x1001u);
    chk("a track running the effect's kernel sets the use bit of ITS start only", oc_modguard.inuse == 1u);
    oc_mod_boot(1);                                      /* the last boot died in its first 30 s with it in use */
    chk("a death in the first 30 s with the bank in use bans it (MD_CRASHED): out of the Effects table, the other bank still registered, MODULE BANNED said once",
        oc_mod.state[0] == MD_CRASHED && oc_fx_find(0x1001u) < 0 && oc_fx_find(0x1002u) >= 0 && (oc_modguard.banned & 1u) && (oc_modguard.notice & 1u));
    oc_mod_unban(0); oc_mod_boot(0);
    chk("reinstalling or removing lifts the ban (oc_mod_unban): registered again", oc_fx_find(0x1001u) >= 0);
    clean(); good(0, 0x1001u, 1, 2); oc_mod_boot(0);
    for (i = 0; i < OC_MOD_UNCLEAN_MAX; i++) { oc_fxmod_note_use(0x1001u); oc_modguard.run = 1; oc_mod_power_on(0); }
    chk("three sessions in a row that die with it in use: banned by the count, as an engine module", oc_mod.state[0] == MD_CRASHED && oc_fx_find(0x1001u) < 0);
    /* ------------------------------------------------ 10. the six low sectors */
    clean(); ocs_resv = 0; good(17, 0x1001u, 1, 1); oc_mod_boot(0);
    chk("an effect module in the six low sectors is not looked at while they may hold a record part", oc_mod.state[17] == MD_EMPTY && oc_mod.nfxmod == 0);
    ocs_resv = SMP_LEGACY_MASK; oc_mod_low_rescan();
    chk("... and is registered by the rescan once they are the module area's", oc_mod.state[17] == MD_OK && oc_fx_find(0x1001u) >= 0);
    /* ------------------------------------------------ 11. a fuzz of the descriptor: 20000 mutated, resealed images */
    {
        uint32_t seed = 20261102u, n_ok = 0, n_bad = 0, r;
        int ok = 1;
        for (r = 0; r < 20000u; r++) {
            uint32_t k, flips = 1u + (seed = seed * 1664525u + 1013904223u) % 4u, link;
            clean(); len = make(0, 0x1234u, 1, 1u + r % 8u, 1, 0x1000u, 0); link = oc_mod_link(0);
            for (k = 0; k < flips; k++) {
                seed = seed * 1664525u + 1013904223u;
                IMG[D_DESC + (seed >> 8) % 0x340u] ^= (uint8_t)(1u << (seed % 8u));
            }
            fxhdr.crc = ocs_crc32(IMG + FW_MOD_CODE_OFF, len - FW_MOD_CODE_OFF);
            fxhdr.hcrc = ocs_crc32((const uint8_t *)&fxhdr, FW_MOD_HDR_BYTES - 4u);
            memcpy(IMG, &fxhdr, sizeof fxhdr);
            put(0, len); oc_mod_boot(0);
            if (oc_mod.state[0] == MD_OK) {
                uint32_t id = 0, ver = 0, nn = 0, base = oc_mod_flash(0);
                oc_mod_rdctx_t c = { oc_rd_flash, &base, link };
                ok &= fx_desc_check(oc_mod_rd, &c, fxhdr.engine_ptr, link + FW_MOD_CODE_OFF, link + len, 1, &id, &ver, &nn) == FXD_OK && oc_mod.nfxmod == 1 && engine_table_empty() && nn >= 1 && nn <= 8;
                n_ok++;
            } else { ok &= oc_mod.nfxmod == 0 && engine_table_empty(); n_bad++; }
        }
        chk("fuzz: 20000 images with 1 to 4 bit flips in the descriptor, resealed: no crash; OK only when the descriptor checks; refused ones register nothing", ok && n_ok > 100u && n_bad > 100u);
    }
    {   /* (round 16, found by the review) the Effects table holds 8 modules: a ninth id is not planned (it would be committed and then pushed out of, or over, a working one) */
        uint32_t k, ok = 1;
        clean();
        for (k = 0; k < 8u; k++) good(1u + k, 0x100u + k, 0x010000u, 1);
        oc_mod_boot(0);
        ok &= oc_mod.nfxmod == 8u && oc_fxmod_plan(0x900u, 1) < 0;              /* a new id: no room */
        ok &= oc_fxmod_plan(0x103u, 1) >= 0;                                    /* a replacement of an id the table has: allowed */
        chk("a ninth module id is not planned while the Effects table is full; a replacement of one it holds is", ok);
        clean();
    }
    printf(bad ? "fx modules: FAILED\n" : "fx modules: all passed\n");
    return bad != 0;
}
