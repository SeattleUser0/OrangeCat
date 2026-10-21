/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat: each track's list of pool patterns (build doc 4c, Skiff's lists; Skiff 1.3.3 bank.c is the model).
 * Included by project.c, after oc_project.c.
 *
 * A track's list has 16 entries, A..P: each names a pool pattern (01..64) or none, and its REPS. The track is on one
 * entry and plays that entry's pattern. The pool (oc_pool.c) is what is kept; the track's 8 Melodee banks are a cache
 * of it (pattern.c bank_pid: the pattern each bank holds). Going to an entry plays its pattern from the bank that has
 * it, else loads it into the least recently used bank of the track first.
 *
 * Keeping (Skiff's keep): a bank whose pattern changed since it came in (bank_sig) is written back to the pool
 * pattern it is; steps on no pattern yet become a new pattern, the first free number, named by the entry the track is
 * on (or the list's first empty entry). Kept while stopped only (a flash write mutes the audio): going to an entry, a
 * SAVE, the autosave. Two tracks on the same pattern each play their own copy; the one kept last is the pattern.
 *
 * Revisions:
 *   0.3.3: first version (lists, the cache, keep, going to an entry while stopped).
 *   0.3.4: playing: the song (SONG on: each track down its list, REPS passes an entry) and launches (ocl_stage).
 *   0.3.5: rests (an empty entry between the list's first and last patterns: silence, Skiff 1.2.1); entry edits.
 */

/* the bank of track k holding pool pattern id, NPAT none */
static uint32_t ocl_bank_of(uint32_t k, uint32_t id)
{
    for (uint32_t b = 0; b < NPAT; b++)
        if (bank_pid[k][b] == id + 1u) return b;
    return NPAT;
}

/* entry e of track k: its pattern, OCP_N none */
static uint32_t ocl_ent_pat(uint32_t k, uint32_t e)
{
    uint32_t v = lists.ent[k % NTRK][e % OCL_NENT][0];
    return v && v <= OCP_N ? v - 1u : OCP_N;
}

/* the CRC of track k's bank b as the pool would keep it (0: empty) */
static uint32_t ocl_bank_hash(uint32_t k, uint32_t b)
{
    static ocp_gen_t g __attribute__((section(".pool")));
    ocp_gen_bank(&g, k, b);
    return ocp_gen_hash(&g);
}

/* a pool pattern free for a new one: empty in the pool, named by no bank, no list, no saved project. OCP_N none */
static uint32_t ocl_free_pat(void)
{
    for (uint32_t id = 0; id < OCP_N; id++)
        if (!ocp.len[id] && !ocp.hash[id] && !ocj_named_now(id, 0) && !ocj_refs(id, 4u)) return id;
    return OCP_N;
}

/* new pattern id is track k's bank b: named by the entry the track is on when that is empty, else the list's first
 * empty entry (none: the pool keeps it, unnamed) */
static void ocl_name(uint32_t k, uint32_t b, uint32_t id)
{
    uint32_t e = lists.cur[k] % OCL_NENT;
    bank_pid[k][b] = (uint8_t)(id + 1u);
    if (b != trk[k].pattern || lists.ent[k][e][0])
        for (e = 0; e < OCL_NENT && lists.ent[k][e][0]; e++)
            ;
    if (e < OCL_NENT) {
        lists.ent[k][e][0] = (uint8_t)(id + 1u);
        lists.ent[k][e][1] = 0;
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

/* pool pattern id into a bank of track k, ready to play (stopped: what that bank had is kept first). The bank, or NPAT
 * (and *rc: 1 the pattern does not read, 2 a flash error, 4 PATTERNS FULL, 5 no room for its motion or timed notes) */
static uint32_t ocl_fetch(uint32_t k, uint32_t id, int *rc)
{
    uint32_t b = ocl_bank_of(k, id);
    *rc = 0;
    if (b < NPAT) return b;
    if ((b = ocl_victim(k)) == NPAT) { *rc = 4; return NPAT; }
    if ((*rc = ocl_keep(k, b)) != 0) return NPAT;
    switch (ocp_load(id, k, b, 1)) {
    case 0: break;
    case 2: *rc = 5; return NPAT;
    default: *rc = 1; return NPAT;
    }
    bank_pid[k][b] = (uint8_t)(id + 1u);
    bank_sig[k][b] = ocp.hash[id];
    return b;
}

/* track k on its entry e now (stopped): what the track plays kept, then the entry's pattern in. 0 done; 1 an empty
 * entry (or one that does not read); 2 a flash error; 4 PATTERNS FULL; 5 no room for its motion or timed notes */
static int ocl_go(uint32_t k, uint32_t e)
{
    uint32_t id, b;
    int rc;
    k %= NTRK;
    e %= OCL_NENT;
    if ((id = ocl_ent_pat(k, e)) == OCP_N || (!ocp.len[id] && !ocp.hash[id])) return 1;
    if (transport_busy()) return 2;
    if ((rc = ocl_keep(k, trk[k].pattern)) != 0) return rc;
    if ((b = ocl_fetch(k, id, &rc)) == NPAT) return rc;
    lists.cur[k] = (uint8_t)e;
    bank_use[k][b] = ++bank_clock;
    if (b != trk[k].pattern) pattern_request(&trk[k], b);
    return 0;
}

/* entry e of track k names pool pattern id (OCP_N: none) with reps passes (1..16) */
static void ocl_set(uint32_t k, uint32_t e, uint32_t id, uint32_t reps)
{
    k %= NTRK;
    e %= OCL_NENT;
    lists.ent[k][e][0] = (uint8_t)(id < OCP_N ? id + 1u : 0u);
    lists.ent[k][e][1] = (uint8_t)((reps ? reps - 1u : 0u) & 15u);
}

/* ------------------------------------------------- playing: the song --- */
/* (Orange Cat 0.3, 4c step 2) Playing, a track's next entry is staged: its pattern loaded into a bank (flash reads only;
 * a bank holding unkept edits is never reused while playing), then Melodee's own queued switch (pattern_next) swaps it
 * in at the track's loop end once the entry playing has made its REPS passes (seq.c, pattern_reps). With SONG on each
 * track stages its next entry by itself; a launch (ocl_launch) stages any entry for the next loop end. */
static uint8_t ocl_staged[NTRK];               /* the entry + 1 staged on each track (0: none) */
static uint8_t ocl_staged_bank[NTRK];
static uint32_t ocl_staged_gen[NTRK];          /* the track's pattern_gen when staged: changed, it came in */
static int ocl_stage_empty(uint32_t k, uint32_t e, uint32_t reps, int is_song);

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

/* the song's next entry of track k: after the one it is on, the next; an empty entry between the list's first and last
 * patterns is a rest (Skiff 1.2.1: silence for its REPS passes); after the last pattern, the first. OCL_NENT: none
 * (no entry names one) */
static uint32_t ocl_song_next(uint32_t k)
{
    uint32_t cur = lists.cur[k] % OCL_NENT, first, last;
    for (first = 0; first < OCL_NENT && ocl_ent_pat(k, first) == OCP_N; first++)
        ;
    if (first == OCL_NENT) return OCL_NENT;
    for (last = OCL_NENT - 1u; ocl_ent_pat(k, last) == OCP_N; last--)
        ;
    return cur >= first && cur < last ? cur + 1u : first;
}

/* playing: entry e of track k staged to come in at its loop end, once the entry playing has made reps passes (song:
 * the song's, which a stop drops). 0 staged; 1 an empty entry (or a song running); 4 no bank free of unkept edits;
 * 5 no room for its motion or timed notes */
static int ocl_stage(uint32_t k, uint32_t e, uint32_t reps, int song)
{
    uint32_t id = ocl_ent_pat(k, e), b, f;
    k %= NTRK;
    if (id == OCP_N || (!ocp.len[id] && !ocp.hash[id]) || chain.running || chain.armed) return 1;
    if ((b = ocl_bank_of(k, id)) == NPAT) {
        if ((b = ocl_victim_clean(k)) == NPAT) return 4;
        switch (ocp_load(id, k, b, 1)) {
        case 0: break;
        case 2: return 5;
        default: return 1;
        }
        bank_pid[k][b] = (uint8_t)(id + 1u);
        bank_sig[k][b] = ocp.hash[id];
    }
    bank_use[k][b] = ++bank_clock;
    f = motion_guard();
    ocl_staged_gen[k] = trk[k].pattern_gen;
    trk[k].pattern_reps = (uint8_t)((reps > 127u ? 127u : reps) | (song ? 0x80u : 0u));
    trk[k].pattern_next = (uint8_t)b;
    motion_unguard(f);
    ocl_staged[k] = (uint8_t)(e % OCL_NENT + 1u);
    ocl_staged_bank[k] = (uint8_t)b;
    return 0;
}

/* entry e of track k: stopped, at once (ocl_go); playing, at its next loop end. As ocl_go / ocl_stage */
static int ocl_launch(uint32_t k, uint32_t e)
{
    k %= NTRK;
    if (!song.playing) return ocl_go(k, e);
    if (e % OCL_NENT == lists.cur[k] % OCL_NENT && bank_pid[k][trk[k].pattern] == lists.ent[k][e % OCL_NENT][0]) {
        uint32_t f = motion_guard();               /* (the entry it plays: what was staged comes in after all not) */
        trk[k].pattern_next = 0xff;
        trk[k].pattern_reps = 0;
        motion_unguard(f);
        ocl_staged[k] = 0;
        return 0;
    }
    return ocl_stage(k, e, 0, 0);
}

/* each frame (the main loop): a staged entry that came in is the one the track is on; with SONG on and playing each
 * track's next entry staged for its REPS; SONG turned off: the song's staging dropped */
static void ocl_poll(void)
{
    for (uint32_t k = 0; k < NTRK; k++) {
        track_t *t = &trk[k];
        if (ocl_staged[k] && t->pattern_next == 0xff) {   /* (came in, or dropped: a stop, a load) */
            if (t->pattern_gen != ocl_staged_gen[k] && t->pattern == ocl_staged_bank[k])
                lists.cur[k] = (uint8_t)(ocl_staged[k] - 1u);
            ocl_staged[k] = 0;
        }
        if (!lists.song) {
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
        {
            uint32_t cur = lists.cur[k] % OCL_NENT, e = ocl_song_next(k);
            if (e == OCL_NENT || (e == cur && ocl_ent_pat(k, cur) != OCP_N && bank_pid[k][t->pattern] == lists.ent[k][cur][0]))
                continue;                     /* (no entry; or a list of one, playing it: it loops) */
            if (ocl_ent_pat(k, e) == OCP_N)    /* (a rest) */
                (void)ocl_stage_empty(k, e, lists.ent[k][cur][1] + 1u, 1);
            else
                (void)ocl_stage(k, e, lists.ent[k][cur][1] + 1u, 1);
        }
    }
}

/* (the UI's) pool pattern id's LEN, 0 empty; the entry staged on track k, OCL_NENT none */
static uint32_t ocl_pat_len(uint32_t id) { return id < OCP_N ? ocp.len[id] : 0u; }
static uint32_t ocl_staged_ent(uint32_t k) { return ocl_staged[k % NTRK] ? ocl_staged[k % NTRK] - 1u : OCL_NENT; }

/* ---------------------------------------------- the list's edits (SONG) --- */
/* an empty entry at e of track k, the ones from it a place later (P's goes); the track's place moves with its entry */
static void ocl_insert(uint32_t k, uint32_t e)
{
    k %= NTRK; e %= OCL_NENT;
    for (uint32_t i = OCL_NENT - 1u; i > e; i--) {
        lists.ent[k][i][0] = lists.ent[k][i - 1u][0];
        lists.ent[k][i][1] = lists.ent[k][i - 1u][1];
    }
    lists.ent[k][e][0] = lists.ent[k][e][1] = 0;
    if (lists.cur[k] >= e && lists.cur[k] + 1u < OCL_NENT) lists.cur[k]++;
}
/* entry e of track k gone, the ones after it a place earlier */
static void ocl_delete(uint32_t k, uint32_t e)
{
    k %= NTRK; e %= OCL_NENT;
    for (uint32_t i = e; i + 1u < OCL_NENT; i++) {
        lists.ent[k][i][0] = lists.ent[k][i + 1u][0];
        lists.ent[k][i][1] = lists.ent[k][i + 1u][1];
    }
    lists.ent[k][OCL_NENT - 1u][0] = lists.ent[k][OCL_NENT - 1u][1] = 0;
    if (lists.cur[k] > e) lists.cur[k]--;
}
/* entry e of track k twice (the copy after it names the same pattern) */
static void ocl_dup(uint32_t k, uint32_t e)
{
    k %= NTRK; e %= OCL_NENT;
    if (e + 1u >= OCL_NENT) return;
    ocl_insert(k, e + 1u);
    lists.ent[k][e + 1u][0] = lists.ent[k][e][0];
    lists.ent[k][e + 1u][1] = lists.ent[k][e][1];
}
/* entry e of track k given its own pattern: a copy of the one it names, the first free number (Skiff's COPY; stopped).
 * 0 done; 1 an empty entry; 2 a flash error or playing; 4 PATTERNS FULL */
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
    lists.ent[k][e][0] = (uint8_t)(n + 1u);
    if (b < NPAT && lists.cur[k] == e && b == trk[k].pattern) {
        bank_pid[k][b] = (uint8_t)(n + 1u);      /* (the track on it: it plays the copy now) */
        bank_sig[k][b] = ocp.hash[n];
    }
    return 0;
}
/* every list empty (the pool and what the tracks play stay) */
static void ocl_clear(void)
{
    memset(lists.ent, 0, sizeof lists.ent);
    memset(lists.cur, 0, sizeof lists.cur);
}
static int ocl_any(void)                        /* an entry names a pattern */
{
    for (uint32_t k = 0; k < NTRK; k++)
        for (uint32_t e = 0; e < OCL_NENT; e++)
            if (lists.ent[k][e][0]) return 1;
    return 0;
}

/* entry e of track k (empty: naming no pattern) to a new empty pattern: the track on it (stopped: at once; playing: at
 * its loop end, after reps passes of the one playing; song: the song's, a rest), its steps a new pattern named by the
 * entry when they are kept. 0 done; 2 a flash error; 4 no bank to use (playing: none free of unkept edits) */
static int ocl_stage_empty(uint32_t k, uint32_t e, uint32_t reps, int is_song)
{
    uint32_t b;
    int rc;
    k %= NTRK; e %= OCL_NENT;
    if (chain.running || chain.armed) return 2;
    if (!song.playing) {
        if ((rc = ocl_keep(k, trk[k].pattern)) != 0) return rc;
        if ((b = ocl_victim(k)) == NPAT) return 4;
        if ((rc = ocl_keep(k, b)) != 0) return rc;
    } else if ((b = ocl_victim_clean(k)) == NPAT) {
        return 4;
    }
    if (pattern_clear(&trk[k], b)) return 2;
    {   /* (the track's own LEN DIV SWING GATE: a rest's REPS count its passes) */
        pattern_t p;
        pattern_read(k, b, &p);
        memcpy(p.timing, &trk[k].p[P_SLEN], sizeof p.timing);
        pattern_write(k, b, &p);
    }
    bank_pid[k][b] = 0;
    bank_sig[k][b] = 0;
    bank_use[k][b] = ++bank_clock;
    if (!song.playing) {
        lists.cur[k] = (uint8_t)e;
        pattern_request(&trk[k], b);
        return 0;
    }
    {
        uint32_t f = motion_guard();
        ocl_staged_gen[k] = trk[k].pattern_gen;
        trk[k].pattern_reps = (uint8_t)((reps > 127u ? 127u : reps) | (is_song ? 0x80u : 0u));
        trk[k].pattern_next = (uint8_t)b;
        motion_unguard(f);
    }
    ocl_staged[k] = (uint8_t)(e + 1u);
    ocl_staged_bank[k] = (uint8_t)b;
    return 0;
}
static int ocl_go_empty(uint32_t k, uint32_t e) { return ocl_stage_empty(k, e, 0, 0); }

/* every pool pattern nothing names (no list entry, no bank, no saved project) emptied, a chunk at a time (stopped).
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
