/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat: the arrangement (chains, the song, the scenes) read and written whole by the web app (web/app, Arrange tab).
 * Command 102 ARRANGE (web/EDITOR_PROTOCOL.md, "Orange Cat: the arrangement"). Included by editor.c after editor_backup.c.
 *
 * Why it exists: a project as the backup objects carry it (FBKK, pattern_store.c) has no room for Orange Cat's lists
 * (pattern.c ocl_lists_t: 64 chains of 16 steps, the 64-row song, 8 scenes, each track's groove). They travel beside the
 * record only inside the unit (proj_lists, tagged with the record's hash), so a record written back from a page comes
 * in as a new project's lists. This command reads the lists of the music now loaded (the runtime) and writes them back,
 * whole, the way a backup object is written: begin (size and CRC-32), data in order, commit (checked, then applied).
 *
 * What a write changes: the chains, the song, the scenes and which are kept, each track's groove and amount, LAUNCH. What
 * it keeps: where each track is (its chain, the step it is on, its song row) and the mode PLAY last started. After the
 * write, a track whose step now names a phrase other than the one its playing bank holds goes to it, as when a step is
 * gone to on the CHAIN page (ocl_go: what the track had is kept first, a flash write, stopped only). So PLAY plays the
 * new order at once. Nothing else is written to flash: SAVE (or PROJECT save, command 9) keeps the lists with the project.
 *
 * Refused while the transport runs (rc 3): play, a count-in, a Melodee song or chain. The staging is the main loop's
 * project scratch (proj_wire_borrow), as a backup restore's is, so a project save, load or backup in between makes the
 * write stale (rc 5) and it starts again from begin.
 *
 * Request: op, then
 *   0 INFO     (nothing)                    -> 0, rc 0, version 1, size (2 x 7 bits), NPOOL, NCHAIN, NENT, NROW, NSCENE, NTRK,
 *                                              tracks in use, busy (1: the transport runs), CRC-32 of the lists (5 x 7 bits),
 *                                              then for each pool phrase 01..64: flags (1 it holds steps, 2 a bank of the
 *                                              music loaded holds it, 4 a saved project names it, 8 a chain of the music
 *                                              loaded names it), its LEN (0 empty) and the CRC-32 of its bytes in the pool
 *                                              (5 x 7 bits, 0 empty: the page compares it to tell whether lists it kept still
 *                                              name the same phrases)
 *   1 GET      offset (2 x 7), count (2 x 7, 1..256) -> 1, rc, offset (2 x 7), count (2 x 7), pack7 data
 *   2 BEGIN    size (2 x 7, = the size INFO gives), CRC-32 (5 x 7) -> 2, rc
 *   3 DATA     offset (2 x 7, where the last ended), pack7 data (up to 256 bytes) -> 3, rc
 *   4 COMMIT   (nothing)                    -> 4, rc, the first track that could not go to its step (127 none)
 * rc: 0 done; 1 refused (arguments, an offset out of order, a size that is not the lists'; INFO with arguments answers
 * 0, 1); 2 the CRC, the length or a value out of range (nothing is changed); 3 the transport runs; 4 written, but a track
 * did not go to its new step (ocl_go: a flash error, no free bank, a phrase that does not read, no room for its motion or
 * timed notes: the track stays where it was; the reply names it); 5 stale (no begin, or the project scratch reused since:
 * a project save, the autosave, a load or rename, a backup, the card browser's list, a track removed, an FM6 or update
 * transfer; a USB reset; 15 s with no request); 6 (review) the banks' unkept steps could not be kept first (a flash error,
 * PHRASES FULL): nothing is changed. A firmware without the command never answers.
 * Before the lists change, every bank's unkept steps are kept (ocl_keep_all, as a SAVE does), so a phrase that keep names
 * goes into the unit's lists first and is then replaced by the page's: read the lists again after a write (INFO's CRC).
 *
 * Revisions:
 *   0.14.2: first version (branch web-app, Session D). Review: banks kept before the lists change (rc 6), INFO's flags in one pass.
 */
enum { ED_ARRANGE = 102 };
#define ED_AR_VERSION 1u
#define ED_AR_SIZE ((uint32_t)sizeof(ocl_lists_t))
_Static_assert(sizeof(ocl_lists_t) < 16384u, "the lists' size travels in 2 x 7 bits");
_Static_assert(23u + OCL_NPOOL * 7u + 1u < sizeof ed_out, "INFO fits one reply");
static uint8_t ed_ar_put;                      /* a write begun */
static uint32_t ed_ar_len, ed_ar_crc, ed_ar_pos, ed_ar_gen, ed_ar_usb, ed_ar_ms;

static uint32_t ed_ar_n2(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 7; }
static void ed_ar_b2(uint32_t v) { ed_b(v & 127u); ed_b((v >> 7) & 127u); }

/* the lists in l, every value a write may change, in range: 1, else 0 (the unit's own fields are not looked at) */
static int ed_ar_valid(const ocl_lists_t *l)
{
    for (uint32_t c = 0; c < OCL_NCHAIN; c++)
        for (uint32_t e = 0; e < OCL_NENT; e++)
            if (l->chain[c][e][0] > OCL_NPOOL || l->chain[c][e][1] > 15u) return 0;
    for (uint32_t r = 0; r < OCL_NROW; r++)
        for (uint32_t k = 0; k < NTRK; k++)
            if (l->song[r][k] > OCL_NCHAIN) return 0;
    for (uint32_t s = 0; s < OCL_NSCENE; s++)
        for (uint32_t k = 0; k < NTRK; k++)
            if (l->scene[s][k][0] > OCL_NCHAIN || (l->scene[s][k][1] & 127u) >= OCL_NENT) return 0;
    for (uint32_t k = 0; k < NTRK; k++)
        if (l->groove[k] >= GROOVE_N || l->gamt[k] > 100u) return 0;
    return l->launch < OCL_NLAUNCH && !(l->scene_used >> OCL_NSCENE);
}

/* the staged lists applied (stopped): the arrangement taken, each track's place kept; then each track to the phrase
 * its step names. The first track that did not go (NTRK none) */
static uint32_t ed_ar_apply(const ocl_lists_t *in)
{
    uint32_t bad = NTRK;
    memcpy(lists.chain, in->chain, sizeof lists.chain);
    memcpy(lists.song, in->song, sizeof lists.song);
    memcpy(lists.scene, in->scene, sizeof lists.scene);
    lists.scene_used = in->scene_used;
    memcpy(lists.groove, in->groove, sizeof lists.groove);
    memcpy(lists.gamt, in->gamt, sizeof lists.gamt);
    lists.launch = in->launch;
    for (uint32_t k = 0; k < trk_n(); k++) {
        uint32_t e = lists.cur[k] % OCL_NENT, want = OCL_ENT(k, e)[0];
        if (!want || bank_pid[k][trk[k].pattern] == want) continue;   /* (a rest, or already the phrase it names) */
        if (ocl_go(k, e) && bad == NTRK) bad = k;
    }
    sync_reload = 1; ui.force = 1;
    return bad;
}

/* each pool phrase's INFO flags, in one pass over the banks and the chains (review: not 64 passes) */
static void ed_ar_flags(uint8_t f[OCL_NPOOL])
{
    uint32_t k, b, c, e, v;
    for (v = 0; v < OCL_NPOOL; v++) f[v] = (uint8_t)(((ocp.len[v] || ocp.hash[v]) ? 1u : 0u) | (ocj_refs(v, 4u) ? 4u : 0u));
    for (k = 0; k < NTRK; k++)
        for (b = 0; b < NPAT; b++)
            if ((v = bank_pid[k][b]) != 0u && v <= OCL_NPOOL) f[v - 1u] |= 2u;
    for (c = 0; c < OCL_NCHAIN; c++)
        for (e = 0; e < OCL_NENT; e++)
            if ((v = lists.chain[c][e][0]) != 0u && v <= OCL_NPOOL) f[v - 1u] |= 8u;
}

static int ed_arrange_handle(uint32_t cmd, const uint8_t *a, uint32_t n)
{
    uint32_t op, rc = 0;
    if (cmd != ED_ARRANGE || !n || a[0] > 4u) return 0;   /* (another command, or a request that is not one: no reply) */
    op = a[0];
    ed_b(op);
    if (op == 0u) {                            /* INFO */
        if (n != 1u) { ed_b(1); return 1; }
        ed_b(0); ed_b(ED_AR_VERSION); ed_ar_b2(ED_AR_SIZE);
        ed_b(OCL_NPOOL); ed_b(OCL_NCHAIN); ed_b(OCL_NENT); ed_b(OCL_NROW); ed_b(OCL_NSCENE); ed_b(NTRK); ed_b(trk_n());
        ed_b(transport_busy() ? 1u : 0u);
        ed_bk_u32(st_crc32(&lists, ED_AR_SIZE));
        uint8_t fl[OCL_NPOOL];
        ed_ar_flags(fl);
        for (uint32_t id = 0; id < OCL_NPOOL; id++) {
            ed_b(fl[id]); ed_b(ocp.len[id] > 127u ? 127u : ocp.len[id]); ed_bk_u32(ocp.hash[id]);
        }
        return 1;
    }
    if (op == 1u) {                            /* GET */
        uint32_t off = n == 5u ? ed_ar_n2(a + 1) : 0u, count = n == 5u ? ed_ar_n2(a + 3) : 0u;
        rc = n != 5u || !count || count > 256u || off > ED_AR_SIZE || count > ED_AR_SIZE - off ? 1u : 0u;
        ed_b(rc); ed_ar_b2(off); ed_ar_b2(rc ? 0u : count);
        if (!rc) ed_bk_pack((const uint8_t *)&lists + off, count);
        return 1;
    }
    if (op == 2u) {                            /* BEGIN */
        if (n != 8u || a[7] > 15u || ed_ar_n2(a + 1) != ED_AR_SIZE) rc = 1;
        else if (transport_busy()) rc = 3;
        else {
            (void)proj_wire_borrow();          /* (the staging: a backup or a project save since makes this stale) */
            ed_ar_put = 1; ed_ar_len = ED_AR_SIZE; ed_ar_crc = ed_bk_r32(a + 3); ed_ar_pos = 0;
            ed_ar_gen = proj_wire_gen; ed_ar_usb = usb.resets; ed_ar_ms = fm1_ms;
            ed_bk_put = 0;                     /* (a backup restore staged there is gone) */
        }
        ed_b(rc);
        return 1;
    }
    if (!ed_ar_put || ed_ar_gen != proj_wire_gen || ed_ar_usb != usb.resets || fm1_ms - ed_ar_ms > 15000u) {
        ed_ar_put = 0;
        ed_b(5); if (op == 4u) ed_b(127);
        return 1;
    }
    ed_ar_ms = fm1_ms;
    if (op == 3u) {                            /* DATA */
        uint32_t got;
        if (n < 4u || ed_ar_n2(a + 1) != ed_ar_pos ||
            !(got = ed_unpack7(a + 3, n - 3u, ed_smp_buf, 256u)) || got > ed_ar_len - ed_ar_pos) rc = 1;
        else { memcpy(proj_wire_u.raw + ed_ar_pos, ed_smp_buf, got); ed_ar_pos += got; }
        ed_b(rc);
        return 1;
    }
    {                                          /* COMMIT */
        uint32_t bad = NTRK;
        const ocl_lists_t *in = (const ocl_lists_t *)(const void *)proj_wire_u.raw;
        if (n != 1u) rc = 1;
        else if (transport_busy()) rc = 3;
        else if (ed_ar_pos != ed_ar_len || st_crc32(proj_wire_u.raw, ed_ar_len) != ed_ar_crc || !ed_ar_valid(in)) rc = 2;
        else if (ocl_keep_all()) rc = 6;       /* (review: what the banks hold unkept is kept first, before the lists change: a keep names a new
                                                * phrase in a chain step (ocl_name), which must land in the unit's lists, not in the page's) */
        else { bad = ed_ar_apply(in); rc = bad < NTRK ? 4u : 0u; }
        if (rc != 3u) ed_ar_put = 0;           /* (refused while playing: the staging stays, COMMIT again once stopped) */
        ed_b(rc); ed_b(bad < NTRK ? bad : 127u);
        return 1;
    }
}
