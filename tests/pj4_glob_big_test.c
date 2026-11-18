/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat Release 3 (Round 28, Session A, for Session C): a reproducible failing case for BUILD.md 7.41 "Item 2", the conversion's GLOB limit.
 * NOT in tests/run_tests.sh on purpose: it FAILS today for N >= 500 (the suite stays green until the fix). Build and run (from the worktree root, as pj4_sec_test's job):
 *   HC_DIR=$PWD nice -n 19 ~/fm1-hw/r22/hc.sh pj4_glob_big_test pj4_glob_big_test -DOC_PJ4=1 && ~/fm1-hw/r22/bin/pj4_glob_big_test
 * (equivalently: cc -O1 -g -ffp-contract=off -fsanitize=address,undefined -fno-sanitize=alignment -fno-sanitize-recover=all -w <the build set -D flags>
 *  -Ibuild/gen -Ifirmware/src -DOC_PJ4=1 -o pj4_glob_big_test tests/pj4_glob_big_test.c -lm)
 * (build/gen must exist: ./build.sh, or tools/build.py's generate(), with the venv's python for the fonts.)
 * For N = 400, 500, 1000 dense recorded notes (varied times, lengths, notes, velocities; cells flagged SF_RECORDED as the fixture keeps them), save a project in the
 * OCJ3 store, convert it with the hook (pj4_boot), then print GLOB's size (read with a record-sized buffer), whether the conversion accepted it, whether the old record
 * is erased, and whether pj4p_read (PJ4_SECMAX = 4,096 B scratch) gives the project back. Two forms of the record:
 *   0. as the OCJ3 saver writes it: ocj_write step 3 EMPTIES the part's recording area (the notes live in the pool's phrases), so GLOB stays 355 B: passes.
 *   1. with the notes in the project part's recording area (PROJ_REC_OFF, 8,192 B; written by the firmware's proj_record_pack, the hash made again): the record a
 *      part that carries its timed notes gives the conversion. GLOB grows past 4,096 B at about 450 notes: the conversion accepts it, erases the old record (step 5),
 *      and pj4p_read refuses GLOB: the project is unreadable. FAILS for N >= 500.
 */
#define OC_PROJECT_NO_MAIN
#include "oc_project_test.c"

static pj4_ws_t WS;
static uint8_t cvwork[PJ4_WORK];
static uint8_t frz[PJ4_NFRZ * PJ4_FRZ_BYTES];
static uint64_t names[4];
static uint8_t status[4];
static uint32_t over[4];
static pj4p_ctx_t cx = {frz};
static pj4_cvhook_t hk = {pj4p_split, &cx};
static void mk(uint32_t slot, pj4_head_t *h) { h->seed = 0x1000u + slot * 7919u; h->uid = 0xA0u + slot; }

static uint32_t rs = 12345;
static uint32_t rn(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }

static void dense(uint32_t nn)
{
    reset();
    fill_all(7, 1);
    recording_reset();
    for (uint32_t n = 0; n < nn; n++) {
        uint32_t k = n % NTRK, b = (n / NTRK) % NPAT, st = (n / (NTRK * NPAT)) % NSTEP;
        recorded_note_t *r = &recording[n];
        pattern_t p;
        r->on = (uint16_t)(st * 120u + rn() % 97u); r->duration = (uint16_t)(30u + rn() % 900u);
        r->note = (uint8_t)(24u + rn() % 80u); r->vel = (uint8_t)(1u + rn() % 126u);
        r->owner = (uint8_t)(k * NPAT + b); r->step = (uint8_t)st; r->pitch = (int8_t)(rn() % 7u) - 3; r->length = (uint8_t)(rn() % 2u);
        pattern_read(k, b, &p);
        { step_t *s = &p.step[st]; memset(s, 0, sizeof *s); s->time = ST_NOTE; s->n = 1; s->note[0] = r->note; s->vel = r->vel; s->flags = SF_RECORDED; }
        pattern_write(k, b, &p);
    }
    recording_reindex();
}

/* the record in slot 0 with the notes put into its project part's recording area (PROJ_REC_OFF, the directory PROJ_REC_DIR) by the firmware's own proj_record_pack, the part's
 * hash made again: the record the conversion would meet if a project part carried its timed notes (the OCJ3 saver, ocj_write step 3, empties that area: its notes are in the
 * pool's phrases). 0 done */
static int notes_into_part(void)
{
    static uint8_t rec[PJ4_RECMAX], pack[PROJ_STORE_SIZE];
    static project_t q;
    ocj_head_t h;
    ocj_src_t z;
    uint32_t hsz = (uint32_t)sizeof h, at = 0, outn = 0, total = (uint32_t)sizeof ocj_l + PROJ_STORE_SIZE, sum;
    int rl = ocs_read(OCS_PROJECT, 0, rec, sizeof rec);
    if (rl < (int)hsz) return -1;
    memcpy(&h, rec, hsz);
    while (at < (uint32_t)rl - hsz) {                                  /* the lists then the part, run-length coded as one (as pj4p_split) */
        const uint8_t *zz = rec + hsz;
        uint32_t v = zz[at++], n = 1;
        if (!v) n = zz[at++];
        for (; n; n--, outn++) { if (outn < sizeof ocj_l) ((uint8_t *)&ocj_l)[outn] = (uint8_t)v; else pack[outn - sizeof ocj_l] = (uint8_t)v; }
    }
    if (outn != total) return -1;
    memset(&q, 0, sizeof q);
    memcpy(q.recording, recording, sizeof q.recording);
    if (!proj_record_pack(pack, &q)) return -1;
    sum = proj_hash(pack, PROJ_STORE_SIZE - 4u); memcpy(pack + PROJ_STORE_SIZE - 4u, &sum, 4);
    h.zlen = ocj_rle_len(pack);
    h.len = hsz + h.zlen;
    ocj_h = h;
    z.src.get = ocj_get; z.in = pack; z.pos = 0; z.hpos = 0; z.pending = -1;
    return ocs_write_src(OCS_PROJECT, 0, &z.src, h.len) ? -1 : 0;
}

/* part: 0 the record as the OCJ3 saver writes it; 1 with the notes in its project part. 0 the project reads back (or the conversion refused and kept it) */
static int one(uint32_t nn, int part)
{
    static uint8_t orig[PJ4_RECMAX], big[PJ4_RECMAX], pack[PROJ_STORE_SIZE], tmp[PJ4_SECMAX];
    static ocj_head_t h;
    static ocl_lists_t l;
    int olen, glob, rd, old_gone, accepted, sv;
    dense(nn);
    sv = project_save_as(0, "DENSE");
    if (part && notes_into_part()) { printf("  N=%4u: could not write the record with the notes in its part\n", nn); return 1; }
    olen = ocs_read(OCS_PROJECT, 0, orig, sizeof orig);
    for (uint32_t s = 0; s < 4u; s++) names[s] = (uint64_t)ocj.names[s][0] | (uint64_t)ocj.names[s][1] << 32;
    memset(frz, 0, sizeof frz);
    pj4_boot(&WS, names, cvwork, mk, status, over, &hk);
    accepted = status[0] == 1;
    old_gone = !ocs_exists(OCS_PROJECT, 0);
    glob = pj4_sec_read(0, PJ4_T_GLOB, big, sizeof big);
    rd = pj4p_read(0, &h, &l, pack, tmp);
    printf("  N=%4u notes, %s: saved %d; old record %d B; conversion %s (status %u); old record %s; GLOB %d B (limit %u); pj4p_read %s\n", nn,
           part ? "notes in the project part" : "OCJ3 saver (notes in the pool)", sv, olen, accepted ? "ACCEPTED" : "refused", status[0],
           old_gone ? "ERASED" : "kept", glob, PJ4_SECMAX, rd ? "REFUSED at GLOB: project unreadable" : "ok");
    return !(sv == 0 && olen > 0 && (accepted ? rd == 0 : !old_gone));
}

int main(void)
{
    int bad = 0;
    static const uint32_t ns[] = {400u, 500u, 1000u};
    for (int part = 0; part < 2; part++)
        for (uint32_t i = 0; i < 3u; i++) {
            char what[160];
            int b = one(ns[i], part);
            snprintf(what, sizeof what, "%u dense recorded notes, %s: converted and the project reads back (or refused, the old record kept)", ns[i],
                     part ? "in the project part" : "as the OCJ3 saver writes them");
            bad |= check(what, !b);
        }
    printf(bad ? "pj4 GLOB limit: FAILED\n" : "pj4 GLOB limit: all passed\n");
    return bad ? 1 : 0;
}
