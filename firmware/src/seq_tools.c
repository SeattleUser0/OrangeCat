/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat: SEQ tools on the phrase a track plays (build doc 4e part 1; Skiff 1.3.3's SEQ TOOLS): shift a step later
 * or earlier (round the loop) and reverse (each note kept whole, its ties after it: a note of 3 steps starting on step 2
 * of 16 ends up starting on step 12). Its motion moves with its steps. A phrase holding recorded (timed) notes is left
 * alone: NOTES moves those. Undoable (EDIT held), as a load. From the pattern's sheet (NOTES, TRACKER, PHRASE: OCT+
 * held). Included by ui.c (after note_tools.c).
 *
 * Revisions:
 *   0.4.6: first version.
 *   0.14.2: a drum lane's figure copied onto another lane (Skiff 2.0's grid copy): the grid's sheet's Copy lane, Paste lane.
 */
enum { ST_SHIFT_R, ST_SHIFT_L, ST_REVERSE };
/* tool `kind` on track t's phrase playing: 0 done; 1 it holds recorded notes; 2 a Melodee song runs; 3 nothing in it */
static int seq_tool(track_t *t, uint32_t kind)
{
    static step_t tmp[NSTEP];
    uint32_t k = trk_index(t), len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), i, j, f, any = 0;
    if (chain_busy()) return 2;
    if (recording_head[recording_owner(t)] < RECORD_MAX) return 1;
    for (i = 0; i < len; i++) any |= step_on(&t->step[i]) || t->step[i].time == ST_TIE;
    for (i = 0; i < motion.count; i++) any |= (motion.event[i].place >> 6) == k && motion_pattern[i] == t->pattern;
    if (!any) return 3;
    load_begin(t, UNDO_PAT);
    f = motion_guard();
    memcpy(tmp, t->step, sizeof tmp);
    if (kind != ST_REVERSE) {
        uint32_t d = kind == ST_SHIFT_R ? 1u : len - 1u;
        for (i = 0; i < len; i++) t->step[(i + d) % len] = tmp[i];
        for (i = 0; i < motion.count; i++)
            if ((motion.event[i].place >> 6) == k && motion_pattern[i] == t->pattern && (motion.event[i].place & 63u) < len)
                motion.event[i].place = (uint8_t)((k << 6) | (((motion.event[i].place & 63u) + d) % len));
    } else {
        for (i = 0; i < len; i++) step_clear(&t->step[i]);
        for (i = 0; i < len; i++) {
            uint32_t l = 1, ns;
            if (!step_on(&tmp[i])) continue;      /* (a rest, a tie: placed with its note; an orphan tie: gone) */
            while (l < len && tmp[(i + l) % len].time == ST_TIE) l++;
            ns = (2u * len - i - l) % len;
            for (j = 0; j < l; j++) t->step[(ns + j) % len] = tmp[(i + j) % len];
        }
        for (i = 0; i < motion.count; i++)
            if ((motion.event[i].place >> 6) == k && motion_pattern[i] == t->pattern && (motion.event[i].place & 63u) < len)
                motion.event[i].place = (uint8_t)((k << 6) | (len - 1u - (motion.event[i].place & 63u)));
    }
    motion.rsv[0] = 1;
    motion_unguard(f);
    load_end(t);
    return 0;
}
static void seq_tool_ui(uint32_t kind)
{
    static const char *const DONE[3] = {"SHIFTED RIGHT", "SHIFTED LEFT", "REVERSED"};
    int rc;
    if (frz_locked()) return;                           /* (Orange Cat 0.5.6) a frozen phrase */
    rc = seq_tool(TSEL, kind);
    ui_message(!rc ? DONE[kind % 3u] : rc == 1 ? "RECORDED NOTES: USE NOTES" : rc == 2 ? "STOP TO EDIT" : "NOTHING THERE");
    ui.force = 1;
}
/* (Orange Cat 0.14.2) a drum lane's figure, copied and pasted onto another (Skiff 2.0's grid copy, its LANE's): the lane shown, its hits
 * and accents over the phrase's steps (a note on the lane counts as its hit, as the grid shows it). The paste puts them on the lane shown
 * then, from step 1 over the copied length, in place of what that lane had there; the other lanes, notes and step values stay. One undo
 * (EDIT held), as the tools. A phrase with recorded notes is left alone (NOTES moves those). From the grid's sheet (OCT+ held) */
static struct { uint8_t on, len; uint64_t hit, acc; } lane_clip;
static void st_lane_name(char *m, const char *verb, uint32_t lane, uint32_t n)
{
    str_cpy(m, verb, 24);
    str_cpy(m + str_len(m), drum_lane_name(TSEL, lane), 24 - str_len(m));
    if (n) {
        str_cpy(m + str_len(m), " ", 24 - str_len(m));
        fmt_int(m + str_len(m), (int32_t)n);
        str_cpy(m + str_len(m), n == 1u ? " HIT" : " HITS", 24 - str_len(m));
    }
}
static void st_lane_copy(void)
{
    track_t *t = TSEL;
    uint32_t len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), i, n = 0, l = ui.lane % NLANE;
    char m[24];
    lane_clip.hit = lane_clip.acc = 0;
    for (i = 0; i < len; i++) {
        lane_clip.hit |= (uint64_t)((step_lanes(&t->step[i]) >> l) & 1u) << i;
        lane_clip.acc |= (uint64_t)((step_accents(&t->step[i]) >> l) & 1u) << i;
    }
    for (uint64_t h = lane_clip.hit; h; h &= h - 1u) n++;
    lane_clip.on = (uint8_t)(n != 0u);
    lane_clip.len = (uint8_t)len;
    if (!n) {
        st_lane_name(m, "NO HITS ON ", l, 0);
        ui_message(m);
        return;
    }
    st_lane_name(m, "COPIED ", l, n);
    ui_message(m);
}
static void st_lane_paste(void)
{
    track_t *t = TSEL;
    uint32_t len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), i, l = ui.lane % NLANE;
    char m[24];
    if (!lane_clip.on) { ui_message("COPY A LANE FIRST"); return; }
    if (frz_locked()) return;
    if (chain_busy()) { ui_message("STOP TO EDIT"); return; }
    if (recording_head[recording_owner(t)] < RECORD_MAX) { ui_message("RECORDED NOTES: USE NOTES"); return; }
    load_begin(t, UNDO_PAT);
    for (i = 0; i < len && i < lane_clip.len; i++) {
        uint32_t h = (uint32_t)(lane_clip.hit >> i) & 1u;
        grid_hit(t, i, l, h);
        if (h)
            grid_acc(t, i, l, (uint32_t)(lane_clip.acc >> i) & 1u);
    }
    load_end(t);
    st_lane_name(m, "PASTED ON ", l, 0);
    ui_message(m);
    ui.force = 1;
}
static void st_shift_r(void) { seq_tool_ui(ST_SHIFT_R); }
static void st_shift_l(void) { seq_tool_ui(ST_SHIFT_L); }
static void st_reverse(void) { seq_tool_ui(ST_REVERSE); }
