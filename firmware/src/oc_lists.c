/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat: phrases, chains and the song (build doc 4c; Skiff 1.3.3 bank.c and the M8 / LSDJ trackers are the
 * models). Included by project.c, after oc_project.c. The data: pattern.c (ocl_lists_t).
 *
 * A phrase is a pool pattern, 01..64 (oc_pool.c), kept in flash and shared by every project. A chain is 16 steps,
 * A..P, each naming a phrase and its REPS. The song is 64 rows, each naming a chain for every track. A track is on a
 * chain and one of its steps, and plays that step's phrase. The track's 8 Melodee banks are a cache of the pool
 * (pattern.c bank_pid: the phrase each bank holds). Going to a step plays its phrase from the bank that has it, else
 * loads it into the least recently used bank of the track first.
 *
 * Keeping (Skiff's keep): a bank whose phrase changed since it came in (bank_sig) is written back to the pool pattern
 * it is; steps on no phrase yet become a new phrase, the first free number, named by the chain step the track is on
 * (or the chain's first empty step). Kept while stopped only (a flash write mutes the audio): going to a step, a SAVE,
 * the autosave. Two tracks on the same phrase each play their own copy; the one kept last is the phrase.
 *
 * Playing (lists.mode, set by where PLAY was pressed: ui_input.c):
 *   OCL_PHRASE  each track loops the phrase it is on;
 *   OCL_CHAIN   each track goes down its chain, a step's REPS passes each, after the last named step the first;
 *   OCL_SONG    each track goes down its column of the song at its own pace (as a tracker's channels): down its row's
 *               chain, then the next row's; an empty cell or the song's end sends it back to the top of its block of
 *               rows. A track whose start row is empty stays silent.
 * The next phrase is staged ahead: loaded into a bank (flash reads only; a bank holding unkept edits is never reused
 * while playing), then Melodee's own queued switch (pattern_next) swaps it in at the track's loop end once the step
 * playing has made its REPS passes (seq.c, pattern_reps).
 *
 * Revisions:
 *   0.3.3: first version (lists, the cache, keep, going to an entry while stopped).
 *   0.3.4: playing: the song (SONG on: each track down its list, REPS passes an entry) and launches (ocl_stage).
 *   0.3.5: rests (an empty entry between the list's first and last patterns: silence, Skiff 1.2.1); entry edits.
 *   0.3.6: (UI) PATTERNS knobs step one entry; SONG MODE a switch (ui_popup.c); Melodee projects get lists
 *          (pattern_store.c bank_restore).
 *   0.3.7: an entry naming a pattern with nothing in it yet plays (silent) instead of stopping the song; what is
 *          recorded there is kept as that number.
 *   0.4.0: three levels: a track's list is now one of 64 chains it is on; the song's rows name chains; PLAY's mode by
 *          the page it is pressed on (SONG: the song from the row picked; CHAIN: the chains; else the phrases).
 *   0.4.1: (4d) staged MUTEs and 8 scenes (PERFORM: ui_layer.c); a step gone to by hand comes in at lists.launch
 *          (LOOP BAR BEAT NOW: seq.c).
 *   0.4.4: (a review) an empty bank written guarded while playing; a staged step read ahead (pattern_prepare); no
 *          stopped path during a count-in; slots and rows inserted or deleted move every track and staged step on
 *          them; saved projects' chains protect their phrases (oc_project.c ocj_refs).
 */

/* the bank of track k holding pool pattern id, NPAT none */
static uint32_t ocl_bank_of(uint32_t k, uint32_t id)
{
    for (uint32_t b = 0; b < NPAT; b++)
        if (bank_pid[k][b] == id + 1u) return b;
    return NPAT;
}

/* step e of chain c: its phrase, OCP_N none */
static uint32_t ocl_chain_pat(uint32_t c, uint32_t e)
{
    uint32_t v = lists.chain[c % OCL_NCHAIN][e % OCL_NENT][0];
    return v && v <= OCP_N ? v - 1u : OCP_N;
}
/* step e of the chain track k is on: its phrase, OCP_N none */
static uint32_t ocl_ent_pat(uint32_t k, uint32_t e) { return ocl_chain_pat(lists.tchain[k % NTRK], e); }

/* chain c's first and last steps naming a phrase (OCL_NENT, OCL_NENT: none) */
static void ocl_chain_span(uint32_t c, uint32_t *first, uint32_t *last)
{
    uint32_t f, l;
    for (f = 0; f < OCL_NENT && ocl_chain_pat(c, f) == OCP_N; f++)
        ;
    if (f == OCL_NENT) { *first = *last = OCL_NENT; return; }
    for (l = OCL_NENT - 1u; ocl_chain_pat(c, l) == OCP_N; l--)
        ;
    *first = f; *last = l;
}

/* the CRC of track k's bank b as the pool would keep it (0: empty) */
static uint32_t ocl_bank_hash(uint32_t k, uint32_t b)
{
    static ocp_gen_t g __attribute__((section(".pool")));
    ocp_gen_bank(&g, k, b);
    return ocp_gen_hash(&g);
}

/* a pool pattern free for a new one: empty in the pool, named by no bank, no chain, no saved project. OCP_N none */
static uint32_t ocl_free_pat(void)
{
    for (uint32_t id = 0; id < OCP_N; id++)
        if (!ocp.len[id] && !ocp.hash[id] && !ocj_named_now(id, 0) && !ocj_refs(id, 4u)) return id;
    return OCP_N;
}

/* new phrase id is track k's bank b: named by the chain step the track is on when that is empty, else the chain's first
 * empty step (none: the pool keeps it, unnamed) */
static void ocl_name(uint32_t k, uint32_t b, uint32_t id)
{
    uint32_t e = lists.cur[k] % OCL_NENT;
    bank_pid[k][b] = (uint8_t)(id + 1u);
    if (b != trk[k].pattern || OCL_ENT(k, e)[0])
        for (e = 0; e < OCL_NENT && OCL_ENT(k, e)[0]; e++)
            ;
    if (e < OCL_NENT) {
        OCL_ENT(k, e)[0] = (uint8_t)(id + 1u);
        OCL_ENT(k, e)[1] = 0;
        if (b == trk[k].pattern) lists.cur[k] = (uint8_t)e;
    }
}

/* track k's bank b kept (stopped): 0 kept (or unchanged, or nothing there); 2 a flash error; 4 PATTERNS FULL */
static int ocl_keep(uint32_t k, uint32_t b)
{
    uint32_t h = ocl_bank_hash(k, b), id = bank_pid[k][b];
    int rc;
    if (id > OCP_N) id = bank_pid[k][b] = 0;
    if (id && !bank_sig[k][b]) bank_sig[k][b] = ocp.hash[id - 1u];   /* (unknown: it came in as the pool has it) */
    if (!id && !h) return 0;                  /* (nothing there: nothing to keep) */
    if (id && h == bank_sig[k][b]) return 0;  /* (unchanged since it came in: another track's keep is not undone) */
    if (!id) {
        uint32_t n = ocl_free_pat();
        if (n == OCP_N) return 4;
        if ((rc = ocp_save(n, k, b)) != 0) return rc == 2 ? 4 : 2;
        ocl_name(k, b, n);
    } else if ((rc = ocp_save(id - 1u, k, b)) != 0) {
        return rc == 2 ? 4 : 2;
    }
    bank_sig[k][b] = h;
    return 0;
}

/* every bank of every track kept (stopped: a SAVE, the autosave): 0, else the first failure's (ocl_keep) */
static int ocl_keep_all(void)
{
    int rc = 0, r;
    for (uint32_t k = 0; k < NTRK; k++)
        for (uint32_t b = 0; b < NPAT; b++)
            if ((r = ocl_keep(k, b)) != 0 && !rc) rc = r;
    return rc;
}

/* the bank of track k to load a pattern into: not the one playing nor the one waiting to, the least recently gone to
 * (one holding nothing first) */
static uint32_t ocl_victim(uint32_t k)
{
    uint32_t best = NPAT, age = 0;
    for (uint32_t b = 0; b < NPAT; b++) {
        uint32_t a;
        if (b == trk[k].pattern || b == trk[k].pattern_next) continue;
        a = !bank_pid[k][b] && !ocl_bank_hash(k, b) ? 512u : (uint8_t)(bank_clock - bank_use[k][b]);
        if (best == NPAT || a > age) { best = b; age = a; }
    }
    return best;
}

/* track k's bank b holds nothing that is not kept (it can be reused while playing) */
static int ocl_bank_clean(uint32_t k, uint32_t b)
{
    uint32_t h = ocl_bank_hash(k, b), id = bank_pid[k][b];
    if (!id || id > OCP_N) return !h;
    return h == (bank_sig[k][b] ? bank_sig[k][b] : ocp.hash[id - 1u]);
}

/* the bank of track k to load into while playing: as ocl_victim, among the clean ones. NPAT none */
static uint32_t ocl_victim_clean(uint32_t k)
{
    uint32_t best = NPAT, age = 0;
    for (uint32_t b = 0; b < NPAT; b++) {
        uint32_t a;
        if (b == trk[k].pattern || b == trk[k].pattern_next || !ocl_bank_clean(k, b)) continue;
        a = !bank_pid[k][b] ? 512u : (uint8_t)(bank_clock - bank_use[k][b]);
        if (best == NPAT || a > age) { best = b; age = a; }
    }
    return best;
}

/* ------------------------------------------------ going to a step --- */
/* What is staged on each track (playing): the step + 1 (0: none), the chain and song row it is of (row OCL_NOROW: the
 * track's own, unchanged), the bank it is in, and the track's pattern_gen when staged (changed: it came in) */
static uint8_t ocl_staged[NTRK], ocl_staged_chain[NTRK], ocl_staged_row[NTRK], ocl_staged_bank[NTRK];
static uint32_t ocl_staged_gen[NTRK];
static uint16_t ocl_staged_epoch[NTRK];                 /* (0.4.6, a review) the project's (pattern_epoch): a load drops it */

/* track k to bank b holding step e of chain c (song row row, OCL_NOROW: unchanged): stopped, at once; playing, staged
 * for its loop end after reps passes of the one playing (is_song: the song's next, which a stop drops) */
static void ocl_place(uint32_t k, uint32_t b, uint32_t c, uint32_t e, uint32_t row, uint32_t reps, int is_song)
{
    bank_use[k][b] = ++bank_clock;
    if (!song.playing) {
        lists.tchain[k] = (uint8_t)c;
        lists.cur[k] = (uint8_t)e;
        if (row != OCL_NOROW) lists.row[k] = (uint8_t)row;
        if (b != trk[k].pattern) pattern_request(&trk[k], b);
        return;
    }
    {
        uint32_t f = motion_guard();
        ocl_staged_gen[k] = trk[k].pattern_gen;
        ocl_staged_epoch[k] = pattern_epoch;
        trk[k].pattern_reps = (uint8_t)((reps > 127u ? 127u : reps) | (is_song ? 0x80u : 0u));
        trk[k].pattern_next = (uint8_t)b;
        motion_unguard(f);
        pattern_prepare(k, b);                 /* (0.4.4) its steps read ahead: the switch at the loop end only swaps */
    }
    ocl_staged[k] = (uint8_t)(e + 1u);
    ocl_staged_chain[k] = (uint8_t)c;
    ocl_staged_row[k] = (uint8_t)row;
    ocl_staged_bank[k] = (uint8_t)b;
}

/* an empty bank of track k (stopped: what it and the one playing had kept first; playing: one free of unkept edits),
 * the track's own LEN DIV SWING GATE (a rest's REPS count its passes); its phrase id + 1 (0: none yet). NPAT: none
 * (*rc: 2 a flash error, 4 no bank to use) */
static uint32_t ocl_empty_bank(uint32_t k, uint32_t pid, int *rc)
{
    uint32_t b;
    *rc = 0;
    if (!song.playing) {
        if ((*rc = ocl_keep(k, trk[k].pattern)) != 0) return NPAT;
        if ((b = ocl_victim(k)) == NPAT) { *rc = 4; return NPAT; }
        if ((*rc = ocl_keep(k, b)) != 0) return NPAT;
    } else if ((b = ocl_victim_clean(k)) == NPAT) {
        *rc = 4;
        return NPAT;
    }
    if (pattern_clear(&trk[k], b)) { *rc = 2; return NPAT; }
    {                                          /* (0.4.4: guarded: the audio's switches share the banks' free list) */
        pattern_t p;
        uint32_t f = motion_guard();
        pattern_read(k, b, &p);
        memcpy(p.timing, &trk[k].p[P_SLEN], sizeof p.timing);
        pattern_write(k, b, &p);
        motion_unguard(f);
    }
    bank_pid[k][b] = (uint8_t)pid;
    bank_sig[k][b] = 0;
    return b;
}

/* track k to step e of chain c (song row row, OCL_NOROW: unchanged): stopped, at once (what the track plays kept
 * first); playing, staged for its loop end after reps passes (is_song: the song's). A step naming no phrase is a rest
 * (an empty pattern: what is recorded there a new phrase named by that step); one naming a phrase with nothing in it yet
 * plays silent too, what is recorded there kept as that number. 0 done; 1 the phrase does not read; 2 a flash error
 * (or a Melodee song running); 4 no bank (PATTERNS FULL; playing: none free of unkept edits); 5 no room for its
 * motion or timed notes */
static int ocl_go_at(uint32_t k, uint32_t c, uint32_t e, uint32_t row, uint32_t reps, int is_song)
{
    uint32_t id, b;
    int rc;
    k %= NTRK; c %= OCL_NCHAIN; e %= OCL_NENT;
    if (chain.running || chain.armed) return 2;
    if (!song.playing && transport_busy()) return 2;   /* (0.4.4: a count-in: no flash writes, no stopped path) */
    id = ocl_chain_pat(c, e);
    if (id == OCP_N || (!ocp.len[id] && !ocp.hash[id] && ocl_bank_of(k, id) == NPAT)) {
        if ((b = ocl_empty_bank(k, id == OCP_N ? 0u : id + 1u, &rc)) == NPAT) return rc;
    } else if ((b = ocl_bank_of(k, id)) == NPAT) {
        if (!song.playing) {
            if ((rc = ocl_keep(k, trk[k].pattern)) != 0) return rc;
            if ((b = ocl_victim(k)) == NPAT) return 4;
            if ((rc = ocl_keep(k, b)) != 0) return rc;
        } else if ((b = ocl_victim_clean(k)) == NPAT) {
            return 4;
        }
        switch (ocp_load(id, k, b, 1)) {
        case 0: break;
        case 2: return 5;
        default: return 1;
        }
        bank_pid[k][b] = (uint8_t)(id + 1u);
        bank_sig[k][b] = ocp.hash[id];
    } else if (!song.playing && (rc = ocl_keep(k, trk[k].pattern)) != 0) {
        return rc;
    }
    ocl_place(k, b, c, e, row, reps, is_song);
    return 0;
}

/* track k on step e of the chain it is on, now (stopped). As ocl_go_at; 1 also: a step naming no phrase */
static int ocl_go(uint32_t k, uint32_t e)
{
    k %= NTRK;
    if (ocl_ent_pat(k, e) == OCP_N) return 1;
    if (transport_busy()) return 2;
    return ocl_go_at(k, lists.tchain[k], e, OCL_NOROW, 0, 0);
}
/* .. a step naming no phrase: a new empty phrase there (stopped: now; playing: at the loop end) */
static int ocl_go_empty(uint32_t k, uint32_t e) { return ocl_go_at(k % NTRK, lists.tchain[k % NTRK], e, OCL_NOROW, 0, 0); }

/* step e of the chain track k is on names pool pattern id (OCP_N: none) with reps passes (1..16) */
static void ocl_set(uint32_t k, uint32_t e, uint32_t id, uint32_t reps)
{
    OCL_ENT(k, e)[0] = (uint8_t)(id < OCP_N ? id + 1u : 0u);
    OCL_ENT(k, e)[1] = (uint8_t)((reps ? reps - 1u : 0u) & 15u);
}

/* track k to chain c: its first step naming a phrase (none: step A, a new phrase there). Stopped: now; playing: at
 * the loop end. As ocl_go_at */
static int ocl_go_chain(uint32_t k, uint32_t c)
{
    uint32_t f, l;
    ocl_chain_span(c % OCL_NCHAIN, &f, &l);
    return ocl_go_at(k, c, f == OCL_NENT ? 0u : f, OCL_NOROW, 0, 0);
}

/* ------------------------------------------------- playing: what comes next --- */
/* the next place of track k as mode plays (*c, *e, *row): 0 one; 1 none (it loops what it plays) */
static int ocl_next(uint32_t k, uint32_t *c, uint32_t *e, uint32_t *row)
{
    uint32_t cur = lists.cur[k] % OCL_NENT, tc = lists.tchain[k] % OCL_NCHAIN, f, l;
    *row = OCL_NOROW;
    if (lists.mode == OCL_CHAIN) {             /* down its chain, after the last named step the first */
        ocl_chain_span(tc, &f, &l);
        if (f == OCL_NENT) return 1;
        *c = tc;
        *e = cur >= f && cur < l ? cur + 1u : f;
        return *e == cur && ocl_ent_pat(k, cur) != OCP_N && bank_pid[k][trk[k].pattern] == OCL_ENT(k, cur)[0];
    }
    if (lists.mode != OCL_SONG) return 1;
    {
        uint32_t r = lists.row[k], cc;
        if (r >= OCL_NROW || !lists.song[r][k]) return 1;   /* (not in the song: silent, or as it was) */
        cc = lists.song[r][k] - 1u;
        ocl_chain_span(cc, &f, &l);
        if (cc == tc && f != OCL_NENT && cur >= f && cur < l) {   /* (on in its row's chain) */
            *c = cc; *e = cur + 1u; *row = r;
            return 0;
        }
        if (cc != tc) {                        /* (its row's chain not begun yet) */
            *c = cc; *e = f == OCL_NENT ? 0u : f; *row = r;
            return 0;
        }
        r++;                                   /* the next row; empty, or the end: the top of the block */
        if (r >= OCL_NROW || !lists.song[r][k])
            for (r = lists.row[k]; r > 0u && lists.song[r - 1u][k]; r--)
                ;
        cc = lists.song[r][k] - 1u;
        ocl_chain_span(cc, &f, &l);
        *c = cc; *e = f == OCL_NENT ? 0u : f; *row = r;
        return r == lists.row[k] && cc == tc && *e == cur && f == l && f != OCL_NENT &&
               bank_pid[k][trk[k].pattern] == OCL_ENT(k, cur)[0];   /* (a block of one row, one step: it loops) */
    }
}

/* playing: step e of the chain track k is on, at the next loop end (the step it plays: what was staged comes in after
 * all not). Stopped: at once. As ocl_go_at */
static int ocl_launch(uint32_t k, uint32_t e)
{
    k %= NTRK;
    e %= OCL_NENT;
    if (!song.playing) return OCL_ENT(k, e)[0] ? ocl_go(k, e) : ocl_go_empty(k, e);
    if (e == lists.cur[k] % OCL_NENT && OCL_ENT(k, e)[0] && bank_pid[k][trk[k].pattern] == OCL_ENT(k, e)[0]) {
        uint32_t f = motion_guard();
        trk[k].pattern_next = 0xff;
        trk[k].pattern_reps = 0;
        motion_unguard(f);
        ocl_staged[k] = 0;
        return 0;
    }
    return ocl_go_at(k, lists.tchain[k], e, OCL_NOROW, 0, 0);
}

/* stopped: every track to song row row (its chain's first step; a track whose cell is empty: silent, out of the song
 * until PLAY again) and the mode the song's. 0 done, else the first failure's (ocl_go_at) */
static int ocl_song_start(uint32_t row)
{
    int rc = 0, r;
    row %= OCL_NROW;
    lists.mode = OCL_SONG;
    for (uint32_t k = 0; k < trk_n(); k++) {
        if (lists.song[row][k]) {
            r = ocl_go_chain(k, lists.song[row][k] - 1u);
            lists.row[k] = r ? OCL_NOROW : (uint8_t)row;   /* (0.4.6: one that did not start: out of the song) */
        } else {
            uint32_t b = ocl_empty_bank(k, 0, &r);   /* (an empty bank: silence) */
            if (b < NPAT) pattern_request(&trk[k], b);
            lists.row[k] = OCL_NOROW;
        }
        if (r && !rc) rc = r;
    }
    return rc;
}

/* each frame (the main loop): a staged step that came in is where the track is; playing in the chain or song mode,
 * each track's next place staged for its REPS; in the phrase mode the song's staging dropped */
static void ocl_poll(void)
{
    for (uint32_t k = 0; k < NTRK; k++) {
        track_t *t = &trk[k];
        uint32_t c, e, row;
        if (ocl_staged[k] && ocl_staged_epoch[k] != pattern_epoch)   /* (0.4.6: staged in a project since let go) */
            ocl_staged[k] = 0;
        if (ocl_staged[k] && t->pattern_next == 0xff) {   /* (came in, or dropped: a stop, a load) */
            if (t->pattern_gen != ocl_staged_gen[k] && t->pattern == ocl_staged_bank[k]) {
                lists.cur[k] = (uint8_t)(ocl_staged[k] - 1u);
                lists.tchain[k] = ocl_staged_chain[k];
                if (ocl_staged_row[k] != OCL_NOROW) lists.row[k] = ocl_staged_row[k];
            }
            ocl_staged[k] = 0;
        }
        if (lists.mode == OCL_PHRASE) {
            if (ocl_staged[k] && (t->pattern_reps & 0x80u)) {
                uint32_t f = motion_guard();
                if (t->pattern_reps & 0x80u) { t->pattern_next = 0xff; t->pattern_reps = 0; }
                motion_unguard(f);
                ocl_staged[k] = 0;
            }
            continue;
        }
        if (!song.playing || chain.running || chain.armed || ocl_staged[k] || t->pattern_next != 0xff ||
            k >= trk_n())
            continue;
        if (ocl_next(k, &c, &e, &row))
            continue;
        (void)ocl_go_at(k, c, e, row, OCL_ENT(k, lists.cur[k])[1] + 1u, 1);
    }
}

/* (the UI's) pool pattern id's LEN, 0 empty; the step staged on track k, OCL_NENT none */
static uint32_t ocl_pat_len(uint32_t id) { return id < OCP_N ? ocp.len[id] : 0u; }
static uint32_t ocl_staged_ent(uint32_t k) { return ocl_staged[k % NTRK] ? ocl_staged[k % NTRK] - 1u : OCL_NENT; }
static uint32_t ocl_staged_chain_of(uint32_t k) { return ocl_staged[k % NTRK] ? ocl_staged_chain[k % NTRK] : OCL_NCHAIN; }
static uint32_t ocl_staged_row_of(uint32_t k)   /* the song row staged on track k, OCL_NOROW none (or its own) */
{
    return ocl_staged[k % NTRK] ? ocl_staged_row[k % NTRK] : OCL_NOROW;
}

/* ---------------------------------------------- a chain's edits (CHAIN) --- */
/* an empty step at e of the chain track k is on, the ones from it a place later (P's goes); the track's place moves
 * with its step */
/* (0.4.4) the slots of chain c from e on moved by d (+1 an insert, -1 a delete): every track on c and every step
 * staged on it with them */
static void ocl_slots_moved(uint32_t c, uint32_t e, int d)
{
    for (uint32_t j = 0; j < NTRK; j++) {
        if (lists.tchain[j] == c) {
            if (d > 0 && lists.cur[j] >= e && lists.cur[j] + 1u < OCL_NENT) lists.cur[j]++;
            else if (d < 0 && lists.cur[j] > e) lists.cur[j]--;
        }
        if (ocl_staged[j] && ocl_staged_chain[j] == c) {
            uint32_t s = ocl_staged[j] - 1u;
            if (d > 0 && s >= e && s + 1u < OCL_NENT) ocl_staged[j]++;
            else if (d < 0 && s > e) ocl_staged[j]--;
        }
    }
}
static void ocl_insert(uint32_t k, uint32_t e)
{
    k %= NTRK; e %= OCL_NENT;
    for (uint32_t i = OCL_NENT - 1u; i > e; i--) {
        OCL_ENT(k, i)[0] = OCL_ENT(k, i - 1u)[0];
        OCL_ENT(k, i)[1] = OCL_ENT(k, i - 1u)[1];
    }
    OCL_ENT(k, e)[0] = OCL_ENT(k, e)[1] = 0;
    ocl_slots_moved(lists.tchain[k] % OCL_NCHAIN, e, 1);
}
/* step e gone, the ones after it a place earlier */
static void ocl_delete(uint32_t k, uint32_t e)
{
    k %= NTRK; e %= OCL_NENT;
    for (uint32_t i = e; i + 1u < OCL_NENT; i++) {
        OCL_ENT(k, i)[0] = OCL_ENT(k, i + 1u)[0];
        OCL_ENT(k, i)[1] = OCL_ENT(k, i + 1u)[1];
    }
    OCL_ENT(k, OCL_NENT - 1u)[0] = OCL_ENT(k, OCL_NENT - 1u)[1] = 0;
    ocl_slots_moved(lists.tchain[k] % OCL_NCHAIN, e, -1);
}
/* step e twice (the copy after it names the same phrase) */
static void ocl_dup(uint32_t k, uint32_t e)
{
    k %= NTRK; e %= OCL_NENT;
    if (e + 1u >= OCL_NENT) return;
    ocl_insert(k, e + 1u);
    OCL_ENT(k, e + 1u)[0] = OCL_ENT(k, e)[0];
    OCL_ENT(k, e + 1u)[1] = OCL_ENT(k, e)[1];
}
/* step e given its own phrase: a copy of the one it names, the first free number (Skiff's COPY; stopped). 0 done; 1 an
 * empty step; 2 a flash error or playing; 4 PATTERNS FULL */
static int ocl_copy(uint32_t k, uint32_t e)
{
    uint32_t id, n, b;
    int rc;
    k %= NTRK; e %= OCL_NENT;
    if ((id = ocl_ent_pat(k, e)) == OCP_N || (!ocp.len[id] && !ocp.hash[id])) return 1;
    if (transport_busy()) return 2;
    if ((b = ocl_bank_of(k, id)) < NPAT && (rc = ocl_keep(k, b)) != 0) return rc;   /* (its edits in the copy too) */
    if ((n = ocl_free_pat()) == OCP_N) return 4;
    if ((rc = ocp_copy(id, n)) != 0) return rc == 2 ? 4 : 2;
    OCL_ENT(k, e)[0] = (uint8_t)(n + 1u);
    if (b < NPAT && lists.cur[k] == e && b == trk[k].pattern) {
        bank_pid[k][b] = (uint8_t)(n + 1u);      /* (the track on it: it plays the copy now) */
        bank_sig[k][b] = ocp.hash[n];
    }
    return 0;
}
/* the chain track k is on emptied (the pool and what the track plays stay) */
static void ocl_clear_chain(uint32_t k)
{
    memset(lists.chain[lists.tchain[k % NTRK] % OCL_NCHAIN], 0, sizeof lists.chain[0]);
    lists.cur[k % NTRK] = 0;
}
static int ocl_chain_any(uint32_t c)           /* chain c names a phrase */
{
    for (uint32_t e = 0; e < OCL_NENT; e++)
        if (lists.chain[c % OCL_NCHAIN][e][0]) return 1;
    return 0;
}
/* a chain free for a new one: naming nothing, in no song cell, no track on it. OCL_NCHAIN none */
static uint32_t ocl_free_chain(void)
{
    for (uint32_t c = 0; c < OCL_NCHAIN; c++) {
        uint32_t used = ocl_chain_any(c);
        for (uint32_t r = 0; r < OCL_NROW && !used; r++)
            for (uint32_t k = 0; k < NTRK; k++) used |= lists.song[r][k] == c + 1u;
        for (uint32_t k = 0; k < NTRK; k++) used |= lists.tchain[k] == c;
        if (!used) return c;
    }
    return OCL_NCHAIN;
}

/* ---------------------------------------------- the song's edits (SONG) --- */
/* song row r's cell of track k a copy of its chain, a new number (as M8's clone). 0 done; 1 an empty cell; 4 no chain
 * free */
static int ocl_chain_clone(uint32_t r, uint32_t k)
{
    uint32_t c, n;
    r %= OCL_NROW; k %= NTRK;
    if (!lists.song[r][k]) return 1;
    c = lists.song[r][k] - 1u;
    if ((n = ocl_free_chain()) == OCL_NCHAIN) return 4;
    memcpy(lists.chain[n], lists.chain[c], sizeof lists.chain[n]);
    lists.song[r][k] = (uint8_t)(n + 1u);
    return 0;
}
/* an empty row at r, the rows from it a place later (the last goes) */
static void ocl_row_insert(uint32_t r)
{
    r %= OCL_NROW;
    for (uint32_t i = OCL_NROW - 1u; i > r; i--) memcpy(lists.song[i], lists.song[i - 1u], NTRK);
    memset(lists.song[r], 0, NTRK);
    for (uint32_t k = 0; k < NTRK; k++) {
        if (lists.row[k] != OCL_NOROW && lists.row[k] >= r && lists.row[k] + 1u < OCL_NROW) lists.row[k]++;
        if (ocl_staged[k] && ocl_staged_row[k] != OCL_NOROW && ocl_staged_row[k] >= r &&
            ocl_staged_row[k] + 1u < OCL_NROW)
            ocl_staged_row[k]++;              /* (0.4.4: a row staged moves with its row) */
    }
}
/* row r gone, the rows after it a place earlier */
static void ocl_row_delete(uint32_t r)
{
    r %= OCL_NROW;
    for (uint32_t i = r; i + 1u < OCL_NROW; i++) memcpy(lists.song[i], lists.song[i + 1u], NTRK);
    memset(lists.song[OCL_NROW - 1u], 0, NTRK);
    for (uint32_t k = 0; k < NTRK; k++) {
        if (lists.row[k] != OCL_NOROW && lists.row[k] > r) lists.row[k]--;
        if (ocl_staged[k] && ocl_staged_row[k] != OCL_NOROW && ocl_staged_row[k] > r) ocl_staged_row[k]--;
    }
}
static int ocl_song_any(void)                  /* a song cell names a chain */
{
    for (uint32_t r = 0; r < OCL_NROW; r++)
        for (uint32_t k = 0; k < NTRK; k++)
            if (lists.song[r][k]) return 1;
    return 0;
}
static void ocl_song_clear(void)
{
    memset(lists.song, 0, sizeof lists.song);
    for (uint32_t k = 0; k < NTRK; k++) lists.row[k] = OCL_NOROW;
}

/* every pool pattern nothing names (no chain step, no bank, no saved project) emptied, a chunk at a time (stopped).
 * How many, or -1 a flash error, -2 playing */
static int ocl_clear_unused(void)
{
    uint8_t want[OCP_N];
    int n = 0;
    if (transport_busy()) return -2;
    if (ocl_keep_all() == 2) return -1;        /* (what the tracks have kept first: it names its patterns) */
    memset(want, 0, sizeof want);
    for (uint32_t id = 0; id < OCP_N; id++)
        if ((ocp.len[id] || ocp.hash[id]) && !ocj_named_now(id, 0) && !ocj_refs(id, 4u)) { want[id] = 255u; n++; }
    if (n && ocj_put_chunks(want, &proj_scratch, proj_wire_u.raw, 0)) return -1;
    return n;
}

/* ------------------------------------------- PERFORM: mutes, scenes (4d) --- */
/* track k's MUTE turned: stopped, at once; playing, staged for its launch point (seq.c; staged already: unstaged) */
static void ocl_mute(uint32_t k)
{
    track_t *t = &trk[k % NTRK];
    if (!song.playing) { t->p[P_MUTE] = (int16_t)!t->p[P_MUTE]; t->mute_arm = 0; return; }
    t->mute_arm = t->mute_arm ? 0u : (uint8_t)(t->p[P_MUTE] ? 1u : 2u);
}
static int ocl_scene_used(uint32_t s) { return (lists.scene_used >> (s % OCL_NSCENE)) & 1u; }
/* scene s: each track in use where it is (its chain and step) and its MUTE (a staged one: as it will be) */
static void ocl_scene_store(uint32_t s)
{
    s %= OCL_NSCENE;
    for (uint32_t k = 0; k < NTRK; k++) {
        uint32_t e = ocl_staged_ent(k) < OCL_NENT ? ocl_staged_ent(k) : lists.cur[k] % OCL_NENT;
        uint32_t c = ocl_staged_ent(k) < OCL_NENT ? ocl_staged_chain[k] : lists.tchain[k];
        uint32_t m = trk[k].mute_arm ? trk[k].mute_arm - 1u : trk[k].p[P_MUTE] != 0;
        lists.scene[s][k][0] = (uint8_t)(k < trk_n() ? c % OCL_NCHAIN + 1u : 0u);
        lists.scene[s][k][1] = (uint8_t)(e | (m ? 0x80u : 0u));
    }
    lists.scene_used |= (uint8_t)(1u << s);
}
/* scene s recalled: each track it holds to its chain and step (stopped: at once; playing: staged for the launch point,
 * so that with BAR they all land together) and its MUTE likewise. The song playing: the chains play on from there.
 * 0 done; 1 an empty scene; else the first failure's (ocl_go_at) */
static int ocl_scene_recall(uint32_t s)
{
    int rc = 0, r;
    s %= OCL_NSCENE;
    if (!ocl_scene_used(s)) return 1;
    if (lists.mode == OCL_SONG) lists.mode = OCL_CHAIN;
    for (uint32_t k = 0; k < trk_n(); k++) {
        uint32_t c = lists.scene[s][k][0], e = lists.scene[s][k][1] & (OCL_NENT - 1u), m = lists.scene[s][k][1] >> 7;
        if (!c) continue;
        c--;
        if (c != lists.tchain[k] || e != lists.cur[k] % OCL_NENT || ocl_staged_ent(k) < OCL_NENT ||
            bank_pid[k][trk[k].pattern] != lists.chain[c][e][0]) {
            if (!song.playing && c == lists.tchain[k] && e == lists.cur[k] % OCL_NENT)
                r = 0;                               /* (stopped, where it is: as it is) */
            else if (c == lists.tchain[k])
                r = ocl_launch(k, e);                /* (its own chain: the step it plays unstages) */
            else
                r = ocl_go_at(k, c, e, OCL_NOROW, 0, 0);
            if (r && !rc) rc = r;
        }
        if ((uint32_t)(trk[k].p[P_MUTE] != 0) != m || trk[k].mute_arm) {
            if (!song.playing) { trk[k].p[P_MUTE] = (int16_t)m; trk[k].mute_arm = 0; }
            else trk[k].mute_arm = (uint8_t)(m + 1u);
        }
    }
    return rc;
}

/* ------------------------------------------- PHRASES: the pool (0.4.4) --- */
/* pool phrase id named by this project: a chain's slot, or a bank of a track (it is in one) */
static int ocl_pat_named(uint32_t id)
{
    for (uint32_t c = 0; c < OCL_NCHAIN; c++)
        for (uint32_t e = 0; e < OCL_NENT; e++)
            if (lists.chain[c][e][0] == id + 1u) return 1;
    for (uint32_t k = 0; k < NTRK; k++)
        for (uint32_t b = 0; b < NPAT; b++)
            if (bank_pid[k][b] == id + 1u) return 1;
    return 0;
}
static uint32_t ocl_pat_slots(uint32_t id)      /* this project's chain slots naming it */
{
    uint32_t n = 0;
    for (uint32_t c = 0; c < OCL_NCHAIN; c++)
        for (uint32_t e = 0; e < OCL_NENT; e++) n += lists.chain[c][e][0] == id + 1u;
    return n;
}
static int ocl_pat_used(uint32_t id) { return id < OCP_N && (ocp.len[id] || ocp.hash[id]); }
/* phrase id copied to the first free number (stopped; what a bank holds of it kept first): the new number, or OCP_N
 * + 1 an empty phrase, OCP_N + 2 PHRASES FULL, OCP_N + 3 a flash error or playing */
static uint32_t ocl_pool_copy(uint32_t id)
{
    uint32_t n;
    int rc;
    if (!ocl_pat_used(id)) return OCP_N + 1u;
    if (transport_busy()) return OCP_N + 3u;
    if (ocl_keep_all() == 2) return OCP_N + 3u;
    if ((n = ocl_free_pat()) == OCP_N) return OCP_N + 2u;
    if ((rc = ocp_copy(id, n)) != 0) return rc == 2 ? OCP_N + 2u : OCP_N + 3u;
    return n;
}
/* phrase id emptied (stopped): 0 done; 1 nothing in it; 2 named here or by a saved project; 3 a flash error or
 * playing */
static int ocl_pool_clear(uint32_t id)
{
    uint8_t want[OCP_N];
    if (!ocl_pat_used(id)) return 1;
    if (transport_busy()) return 3;
    if (ocl_pat_named(id) || ocj_refs(id, 4u)) return 2;
    memset(want, 0, sizeof want);
    want[id] = 255u;
    return ocj_put_chunks(want, &proj_scratch, proj_wire_u.raw, 0) ? 3 : 0;
}
/* the selected track's chain slot e names phrase id and the track goes there (stopped: at once; playing: at the
 * launch point). As ocl_launch */
static int ocl_pool_put(uint32_t k, uint32_t e, uint32_t id)
{
    uint8_t was;
    int rc;
    k %= NTRK; e %= OCL_NENT;
    if (chain.running || chain.armed || (!song.playing && transport_busy())) return 2;   /* (0.4.6: the slot as it was) */
    was = OCL_ENT(k, e)[0];
    OCL_ENT(k, e)[0] = (uint8_t)(id + 1u);
    if ((rc = ocl_launch(k, e)) != 0) OCL_ENT(k, e)[0] = was;
    return rc;
}
