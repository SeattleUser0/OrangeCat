/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat: SEQ tools on the phrase a track plays (build doc 4e part 1; Skiff 1.3.3's SEQ TOOLS): shift a step later
 * or earlier (round the loop) and reverse (each note kept whole, its ties after it: a note of 3 steps starting on step 2
 * of 16 ends up starting on step 12). Its motion moves with its steps, and (Round 25, the owner's decision) so do its control
 * notes (DUB's KICK HOLD DIVE RISE) and its ordinary recorded (timed) notes, with their cells (recording.c recording_ctl_move):
 * nothing is left behind. Undoable (EDIT held), as a load; on a bank with recorded notes each tool is a level of its own and
 * its undo moves them back (ui.c undo_level). From the pattern's sheet (NOTES, TRACKER, PHRASE: OCT+
 * held). Included by ui.c (after note_tools.c).
 *
 * Revisions:
 *   0.4.6: first version.
 *   0.14.2: a drum lane's figure copied onto another lane (Skiff 2.0's grid copy): the grid's sheet's Copy lane, Paste lane.
 *   Round 25: SHIFT and REVERSE move the bank's recorded notes (control and ordinary) with the steps and the motion; their undo moves them back.
 */
enum { ST_SHIFT_R, ST_SHIFT_L, ST_REVERSE };
/* (Round 25) where tool `kind` takes each cell of steps s (LEN len): a step later or earlier round the loop; REVERSE: a note with its ties to len - i - l, a rest
 * (or an orphan tie, which goes) to its mirror len - 1 - i. The steps below and the recorded notes (recording.c recording_note_target) follow it; ui.c undo_level
 * builds it again from the steps as they are, so the undo of REVERSE is the map of the reversed steps (its inverse) */
static void seq_tool_map(const step_t *s, uint32_t len, uint32_t kind, uint8_t *map)
{
    uint32_t i, j;
    for (i = 0; i < len; i++) map[i] = (uint8_t)(kind == ST_SHIFT_R ? (i + 1u) % len : kind == ST_SHIFT_L ? (i + len - 1u) % len : len - 1u - i);
    if (kind != ST_REVERSE) return;
    for (i = 0; i < len; i++) {
        uint32_t l = 1, ns;
        if (!step_on(&s[i])) continue;
        while (l < len && s[(i + l) % len].time == ST_TIE) l++;
        ns = (2u * len - i - l) % len;
        for (j = 0; j < l; j++) map[(i + j) % len] = (uint8_t)((ns + j) % len);
    }
}
/* (Round 25) the tool's move of the steps (each note with its ties, a rest as it is) and the motion of track t's bank inside LEN; map: where each cell went (for
 * the recorded notes). Also ui.c undo_level's undo of a tool when the steps changed after it (a take wrote a cell): the inverse tool on the steps as they are.
 * Caller: audio interrupts paused */
static void seq_tool_apply(track_t *t, uint32_t len, uint32_t kind, uint8_t *map)
{
    static step_t tmp[NSTEP];
    uint32_t k = trk_index(t), i, j;
    memcpy(tmp, t->step, sizeof tmp);
    seq_tool_map(tmp, len, kind, map);
    if (kind != ST_REVERSE) {
        for (i = 0; i < len; i++) t->step[map[i]] = tmp[i];
    } else {
        for (i = 0; i < len; i++)                       /* (Round 25) a rest mirrored as it is (its bytes kept: REVERSE twice is the phrase byte for byte, and the take's undo signature with it) */
            if (!step_on(&tmp[i]) && tmp[i].time != ST_TIE) t->step[len - 1u - i] = tmp[i];
            else step_clear(&t->step[len - 1u - i]);
        for (i = 0; i < len; i++) {
            uint32_t l = 1;
            if (!step_on(&tmp[i])) continue;      /* (a rest, a tie: placed with its note; an orphan tie: gone) */
            while (l < len && tmp[(i + l) % len].time == ST_TIE) l++;
            for (j = 0; j < l; j++) t->step[map[(i + j) % len]] = tmp[(i + j) % len];
        }
    }
    for (i = 0; i < motion.count; i++)
        if ((motion.event[i].place >> 6) == k && motion_pattern[i] == t->pattern && (motion.event[i].place & 63u) < len)
            motion.event[i].place = (uint8_t)((k << 6) | (kind != ST_REVERSE ? map[motion.event[i].place & 63u] : len - 1u - (motion.event[i].place & 63u)));
}
/* tool `kind` on track t's phrase playing: 0 done; 2 a Melodee song runs; 3 nothing in it; 4 (Round 25) a control note has no place to go (recording.c
 * recording_ctl_move: one no writer makes), nothing changed. (Round 25, the owner) the bank's recorded notes, ordinary and control, move with the steps and the
 * motion: nothing is left behind; on a bank with recorded notes each tool is an undo level of its own, and its undo moves them back */
static int seq_tool(track_t *t, uint32_t kind)
{
    static uint8_t map[NSTEP];
    static motion_store_t mo __attribute__((section(".pool")));
    uint32_t k = trk_index(t), len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), i, f, any = 0, ev;
    int dir = kind == ST_SHIFT_R ? 1 : kind == ST_SHIFT_L ? -1 : 0;
    if (chain_busy()) return 2;
    ev = (uint32_t)(recording_holds(recording_owner(t), 0) || recording_holds(recording_owner(t), 1));   /* (Round 25) the recorded notes move with the steps */
    for (i = 0; i < len; i++) any |= step_on(&t->step[i]) || t->step[i].time == ST_TIE;
    for (i = 0; i < motion.count; i++) any |= (motion.event[i].place >> 6) == k && motion_pattern[i] == t->pattern;
    if (!any && !ev) return 3;
    if (ev && !recording_ctl_move(t, len, dir, map, 0)) return 4;
    if (ev) undo_seal();                                /* (Round 25) a level of its own: its undo moves the recorded notes back by this tool alone (ui.c undo_level) */
    motion_snapshot_track(t, &mo);
    load_begin(t, UNDO_PAT);
    f = motion_guard();
    motion_replace_track(t, &mo);                       /* (Round 25) load_begin clears the bank's motion (a load's): the tool keeps it, to move it (it was wiped) */
    seq_tool_apply(t, len, kind, map);
    if (ev) recording_ctl_move(t, len, dir, map, 1);
    motion.rsv[0] = 1;
    motion_unguard(f);
    load_end(t);
    if (ev && undo_top && undo_last()->trk == k + 1u) {
        undo_last()->ctl_tool = (uint8_t)(kind + 1u);
        undo_last()->ctl_len = (uint8_t)len;
        undo_seal();
    }
    return 0;
}
static void seq_tool_ui(uint32_t kind)
{
    static const char *const DONE[3] = {"SHIFTED RIGHT", "SHIFTED LEFT", "REVERSED"};
    int rc;
    if (frz_locked()) return;                           /* (Orange Cat 0.5.6) a frozen phrase */
    rc = seq_tool(TSEL, kind);
    ui_message(!rc ? DONE[kind % 3u] : rc == 2 ? "STOP TO EDIT" : rc == 4 ? "CONTROL NOTE CANNOT MOVE" : "NOTHING THERE");
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
    if (recording_holds(recording_owner(t), 0)) { ui_message("RECORDED NOTES: USE NOTES"); return; }   /* (Round 24 review) as seq_tool: not for a bank whose only events are control notes */
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
