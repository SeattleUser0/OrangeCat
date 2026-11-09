/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat (web-r3, Session D): how the editor link reaches the value a page cell shows, for tests/web_tables.c (the web app's
 * page view) and tests/editor_test.c (which compiles editor.c and holds this rule equal to ed_val's for every cell of every page).
 *
 *   "same"   GET / SET / DUMP read and write the very storage the unit's page shows (song.g, the selected track's p[], or a device
 *            setting ed_val maps: A4, BUDGET and, since web-cmds, BOOT, the DRUM channel, the WEIGHTS)
 *   "packed" DUB's view fields (SYNC PING DRIVE WOW), held in the track's ids 46 (MODE) and 47 (TAPE): dub_fields.h
 *   "recq"   TEMPO's QNT (G_TRK_RECQ): the selected track's RECQ
 *   "kit"    DRUM-X's lane pages (SC_DXLANE): the lane's or the kit's value, KIT_GET / KIT_PUT (commands 128, 129), LEVEL included
 *   "no"     the page shows a device setting the link does not read (none today: BOOT, the DRUM channel and the WEIGHTS were, before web-cmds): the app leaves it to the unit
 *
 * Revisions:
 *   web-r3: first version (Session D).
 */
#ifndef WEB_WIRE_H
#define WEB_WIRE_H
static const char *web_wire(const page_t *pg, uint32_t k, const int16_t *vp)
{
    uint32_t id = pg->id[k];
    if (pg->scope == SC_DXLANE)
        return "kit";
    if (pg->scope == SC_GLOBAL && id == G_TRK_RECQ)
        return vp == &TSEL->p[P_RECQ] ? "recq" : "no";
    if (pg->scope == SC_GLOBAL && id < (uint32_t)G_COUNT)
        return vp == &song.g[id] || (id == G_A4 && vp == &tuning_a4) || (id == G_BUDGET && vp == &settings_budget) ||
               (id == G_BOOT && vp == &boot_cell) || (id == G_DRUMCH && vp == &drumch_cell) || (id >= G_WP5 && id <= G_WDRUM && vp == &settings_vpoints[id - G_WP5]) ||
               (id == G_SAFETY && vp == &settings_safety) ? "same" : "no";
    if (pg->scope == SC_TRACK && id >= DUBF_SYNC && id <= DUBF_WOW)
        return "packed";
    if (pg->scope == SC_TRACK && id < (uint32_t)P_COUNT)
        return vp == &TSEL->p[id] ? "same" : "no";
    return "no";
}
/* the ids the editor link still refuses to write (editor.c ed_may_set): the reserved ones, except the HOME macros (written for a track whose own table has the macro) and SLIDE TIME;
 * the reserved globals, except the WEIGHTS (the device's settings) */
static int web_tp_refused(uint32_t id) { return param_reserved(id) && !(id >= P_MAC0 && id <= P_MAC3); }
static int web_gp_refused(uint32_t id) { return global_reserved(id) && !(id >= G_WP5 && id <= G_SAFETY); }
#endif
