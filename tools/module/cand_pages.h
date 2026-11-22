/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 *
 * Orange Cat (engines-b, Session B, 2026-11-21): the page descriptor (fw_pages_t version 3, docs/orangecat/MODULE_FORMAT_R3.md 4 and 5) of a candidate
 * module whose controls are the track's eight edit values P_E0..P_E7 (every engine candidate: none has a native block). One header, so each module unit
 * says only its rows and its four knobs.
 *
 *   - the panel IS the engine's own edit[] (engine_t.edit, in the module's image): npanel = the controls in use, 1..8, the first ones; panel index k is
 *     P_E0 + k, and the pid table says so (id k + 1, legacy slot P_E0 + k: what the knob registry resolves a knob's target through, oc_modui.c);
 *   - the rows mirror the firmware's EDIT 1 and EDIT 2 (page_title and edit[]: what the unit shows today for an engine without a descriptor). No native
 *     block and no hooks: params.c shows SC_MOD rows only for the engines of MODPAGE_ENG, and reads them from a native block; the candidates are in
 *     neither, so the unit keeps its EDIT 1 and EDIT 2 for them, and these rows are the same pages written as data (for the editor's dump and the day a
 *     P_E-backed module page exists). A row's id[] are panel indices;
 *   - fw_ui_t: four macro knobs (kind 2) and no picture (scene_len 0: a picture can be added later without another identifier move). A target names an
 *     edit index 0..7 (CAND_T); the registry keeps only P_E targets and cuts each range to the slot's own (MODULE_FORMAT_R3.md 14, item 9);
 *   - knob[] (the old EDIT-knob map) = the first four controls, cc[] none.
 *
 * Use, after the engine's edit[] is final and before its engine_t (which points back here: .pages = &NAME_PAGES):
 *   static const engine_t ENG_X;                                         (a tentative definition: the descriptor takes the address of its edit[])
 *   static const page_t X_ROWS[] = {CAND_ROW("SAW", 0, 1, 2, 3)};
 *   static const fw_kdef_t X_KDEF[4] = {CAND_MACRO("WIDTH", CAND_T(0, CAND_LIN, 127), CAND_T0, CAND_T0, CAND_T0), ...};
 *   CAND_PAGES(X, ENG_X, X_ROWS, 4, X_KDEF)                              (4: the controls in use)
 */
#ifndef CAND_PAGES_H
#define CAND_PAGES_H 1
#include "fw_pages.h"
enum { CAND_LIN = 0, CAND_UP = 1, CAND_DOWN = 2 };      /* oc_knobs.c KS_LIN, KS_UP, KS_DOWN (the wire's shape codes) */
#define CAND_PID0 1u                                    /* the stable id of edit index 0; k + 1 for edit index k (0 is a fine id; 1.. reads better in a dump) */
#define CAND_T(k, shape, depth) {(uint16_t)(CAND_PID0 + (k)), (uint8_t)(shape), (int8_t)(depth)}
#define CAND_T0 {FW_PID_NONE, 0, 0}
#define CAND_MACRO(lbl, a, b, c, d) {lbl, 2, 0, FW_PID_NONE, {a, b, c, d}, 0}
#define CAND_ROW(title, a, b, c, d) {title, FAM_EDIT, SC_MOD, GR_NONE, {a, b, c, d}}
static const fw_pid_t CAND_PID[8] = {{1, P_E0}, {2, P_E1}, {3, P_E2}, {4, P_E3}, {5, P_E4}, {6, P_E5}, {7, P_E6}, {8, P_E7}};
#define CAND_PAGES(NAME, ENG, ROWS, NPANEL, KDEF)                                                                                       \
    static const fw_ui_t NAME##_UI = {FW_UI_MAGIC, FW_UI_WIRE, 1, 0, 4, 0, 0, KDEF};                                    \
    static const fw_pages_t NAME##_PAGES = {                                                                                           \
        .magic = FW_PAGES_MAGIC, .size = FW_PAGES_WIRE, .version = FW_PAGES_VERSION,                                    \
        .npages = (uint16_t)(sizeof ROWS / sizeof ROWS[0]), .npanel = (NPANEL), .rows = ROWS, .panel = (ENG).edit,                       \
        .knob = {0, (NPANEL) > 1 ? 1 : 0xFF, (NPANEL) > 2 ? 2 : 0xFF, (NPANEL) > 3 ? 3 : 0xFF}, .cc = {0xFF, 0xFF, 0xFF, 0xFF},            \
        .ui = &NAME##_UI, .pid = CAND_PID, .ext = 0};                                                                                 \
    _Static_assert((NPANEL) >= 1 && (NPANEL) <= 8, "the panel is the first 1..8 edit values");
#endif
