/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* (Round 25) What a pool phrase save writes always loads, and an older build's event does not make a phrase or a project unloadable (BUILD.md 7.33, "Left open by the
 * reviews"). One rule for the savers and the loaders (recording.c recording_load_rule, oc_pool.c ocp_note_rule):
 *   S  the save: a bank holding an event the loader refuses (an older build's step byte 0x7F; a control note with a pitch) is saved WITHOUT it, and the phrase loads.
 *      On 712b4222 the save wrote both events and the load refused the whole phrase.
 *   L  the pool loader: a phrase image an older build wrote, one event with step byte 0x7F, loads with that event dropped and every other kept; its cell (flagged, the
 *      note in it) is kept as a manual step by the load's reconcile (OC_ONE_AUTH). On 712b4222 the phrase was refused.
 *   C  corrupt data is still refused: 0x7F with a pitch out of range, a duration 0, a velocity 0, a control note in a version 2 phrase.
 *   P  the project loaders: a FUN wire image (proj_import, proj_unpack) and a project_t (proj_ok, the raw path) holding an 0x7F event load, the event dropped; one
 *      with 0x7F and a pitch out of range is refused. On 712b4222 both images were refused. */
#define OC_POOL_NO_MAIN
#include "oc_pool_test.c"

static int vbad;
static int vc(const char *what, int ok) { printf("pool valid: %-118s %s\n", what, ok ? "ok" : "FAIL"); vbad += !ok; return !ok; }

static uint32_t add_ev(uint32_t owner, uint32_t step, uint32_t note, uint32_t on)
{
    uint32_t j = 0;
    while (j < RECORD_MAX && recording[j].vel) j++;
    recorded_note_t *r = &recording[j];
    memset(r, 0, sizeof *r);
    r->on = (uint16_t)on; r->duration = 9000; r->note = (uint8_t)note; r->vel = 90; r->owner = (uint8_t)owner; r->step = (uint8_t)step;
    return j;
}
static uint32_t count_of(uint32_t owner, uint32_t *bad)
{
    uint32_t n = 0;
    *bad = 0;
    for (uint32_t i = 0; i < RECORD_MAX; i++)
        if (recording[i].vel && (recording[i].owner & 31u) == owner) { n++; *bad += !recording_valid(&recording[i]); }
    return n;
}
/* bank 2 of track 0: LEN 48 (cut from 64), five ordinary events on steps 1 3 5 20 30 and one on step 15 (note 61), their cells flagged */
static void setup(void)
{
    fresh();
    fill(0, 2, 48, 0, 2, 0, 5);
    add_ev(2, 1, 60, 100); add_ev(2, 3, 62, 200); add_ev(2, 5, 64, 300); add_ev(2, 20, 65, 400); add_ev(2, 30, 67, 500);
    add_ev(2, 15, 61, 1000);
    recording_reindex();
    fixture_cells_for_events();
    {   pattern_t p;                                    /* the cell of step 15 as rec_note left it: a NOTE cell with the note, flagged */
        pattern_read(0, 2, &p);
        p.step[15].time = ST_NOTE; p.step[15].n = 1; p.step[15].note[0] = 61; p.step[15].vel = 90; p.step[15].flags |= SF_RECORDED;
        pattern_write(0, 2, &p);
    }
}
/* pattern j of a chunk record: where its notes start, how many */
static uint32_t notes_off(const uint8_t *rec, uint32_t j, uint32_t *nn)
{
    uint32_t off = 8, i, steps = 0;
    for (i = 0; i < j; i++) off += (uint32_t)(rec[2 * i] | rec[2 * i + 1] << 8);
    for (i = 0; i < 8u; i++) steps += (uint32_t)__builtin_popcount(rec[off + 12 + i]);
    off += OCP_HEAD + steps * OCP_STEP;
    off += 1u + rec[off] * 4u;
    *nn = (uint32_t)(rec[off] | rec[off + 1] << 8);
    return off + 2u;
}

int main(void)
{
    static uint8_t rec[OCS_PARTS_MAX * OCS_PAYLOAD_MAX], img[OCS_PARTS_MAX * OCS_PAYLOAD_MAX];
    static project_t q, q2;
    static project_store_t st;
    uint32_t i, bad, n0, n1, nn, at;
    int n;

    /* S: the save */
    setup();
    n0 = count_of(2, &bad);
    {
        uint32_t a = add_ev(2, 0x7F, 63, 40000);        /* an older build's key on step 64 past the cut: step byte 0x7F */
        uint32_t b = add_ev(2, 7, 125, 0);              /* a control note with a pitch (the Round 23 drum-track editor) */
        recording[b].length = RN_CTL; recording[b].pitch = 3;
        vc("S0 the fixture: both added events are ones recording_valid refuses", !recording_valid(&recording[a]) && !recording_valid(&recording[b]));
        vc("S1 the bank holding them saves to the pool", ocp_save(9, 0, 2) == 0);
        memset(&recording[a], 0, sizeof recording[a]); memset(&recording[b], 0, sizeof recording[b]);   /* (out of the sequencer's RAM before anything indexes it) */
        recording_reindex();
    }
    n = ocs_read(OCS_POOL, 2, rec, sizeof rec);
    notes_off(rec, 1, &nn);
    vc("S2 the phrase it wrote holds only the events the loader keeps (6 of 8)", n > 0 && nn == n0);
    vc("S3 ... and it is version 2 (the left-out control note does not make it version 3)", n > 0 && rec[8 + (rec[0] | rec[1] << 8)] == OCP_VER);
    vc("S4 the phrase loads into another bank", ocp_load(9, 1, 3, 1) == 0);
    n1 = count_of(1u * NPAT + 3u, &bad);
    vc("S5 ... with its six events, every one valid", n1 == n0 && !bad);

    /* L: a phrase image an older build wrote */
    setup();
    vc("L0 the bank saves (all six events valid)", ocp_save(13, 0, 2) == 0);
    n = ocs_read(OCS_POOL, 3, rec, sizeof rec);
    at = notes_off(rec, 1, &nn);
    for (i = 0; i < nn && !(rec[at + i * OCP_NOTE + offsetof(recorded_note_t, step)] == 15u && rec[at + i * OCP_NOTE + offsetof(recorded_note_t, note)] == 61u); i++)
        ;
    vc("L1 the image has the event of step 15", n > 0 && i < nn);
    at += i * OCP_NOTE;
    memcpy(img, rec, (size_t)n);
    img[at + offsetof(recorded_note_t, step)] = 0x7Fu;  /* what an older build wrote for that key: step byte 0x7F */
    vc("L2 the image as an older build wrote it is stored", ocs_write(OCS_POOL, 3, img, (uint32_t)n) == 0);
    vc("L3 the older build's phrase loads", ocp_load(13, 1, 3, 1) == 0);
    {
        uint32_t has7f = 0;
        n1 = count_of(1u * NPAT + 3u, &bad);
        for (i = 0; i < RECORD_MAX; i++) has7f |= recording[i].vel && (recording[i].owner & 31u) == 1u * NPAT + 3u && recording[i].step == 0x7Fu;
        vc("L4 ... the 0x7F event dropped, the five others loaded", n1 == 5u && !bad && !has7f);
    }
#if OC_ONE_AUTH
    {   pattern_t p;
        pattern_read(1, 3, &p);
        vc("L5 ... its cell (step 15) kept as the manual step the step row had: note 61, no longer flagged",
           p.step[15].time == ST_NOTE && p.step[15].n == 1 && p.step[15].note[0] == 61 && !(p.step[15].flags & SF_RECORDED));
    }
#endif

    /* C: corrupt data is still refused */
    {
        static const struct { const char *what; uint32_t field, value; } bad_ev[] = {
            { "C1 a 0x7F event with a pitch out of range is refused", offsetof(recorded_note_t, pitch), 30u },
            { "C2 a 0x7F event with duration 0 is refused", offsetof(recorded_note_t, duration), 0u },
            { "C3 a 0x7F event with velocity 0 is refused", offsetof(recorded_note_t, vel), 0u },
        };
        for (uint32_t c = 0; c < sizeof bad_ev / sizeof bad_ev[0]; c++) {
            memcpy(img, rec, (size_t)n);
            img[at + offsetof(recorded_note_t, step)] = 0x7Fu;
            img[at + bad_ev[c].field] = (uint8_t)bad_ev[c].value;
            if (bad_ev[c].field == offsetof(recorded_note_t, duration)) img[at + bad_ev[c].field + 1] = 0;
            vc(bad_ev[c].what, ocs_write(OCS_POOL, 3, img, (uint32_t)n) == 0 && ocp_load(13, 1, 0, 1) != 0);
        }
        memcpy(img, rec, (size_t)n);
        img[at + offsetof(recorded_note_t, step)] = 0x7Eu;  /* (62, rounded: step 63, valid) */
        img[at + offsetof(recorded_note_t, pitch)] = 30u;
        vc("C4 an ordinary event with a pitch out of range is refused", ocs_write(OCS_POOL, 3, img, (uint32_t)n) == 0 && ocp_load(13, 1, 0, 1) != 0);
        memcpy(img, rec, (size_t)n);
        img[at + offsetof(recorded_note_t, step)] = 0x7Fu;
        img[at + offsetof(recorded_note_t, length)] = RN_CTL; img[at + offsetof(recorded_note_t, note)] = 125u; img[at + offsetof(recorded_note_t, pitch)] = 0;
        vc("C5 a 0x7F control note in a version 2 phrase is refused", ocs_write(OCS_POOL, 3, img, (uint32_t)n) == 0 && ocp_load(13, 1, 0, 1) != 0);
    }

    /* P: the project loaders */
    setup();
    project_capture(&q);
    {
        uint32_t idx = RECORD_MAX, pos;
        for (i = 0, at = 0; i < RECORD_MAX; i++) if (q.recording[i].vel && (q.recording[i].owner & 31u) < 2u) at++;
        for (i = 0; i < RECORD_MAX; i++)
            if (q.recording[i].vel && (q.recording[i].owner & 31u) == 2u) { if (q.recording[i].step == 15u && q.recording[i].note == 61u) { idx = i; break; } at++; }
        vc("P0 the captured project holds the event of step 15", idx < RECORD_MAX);
        vc("P1 it packs", proj_pack(&st, &q));
        uint8_t *v = st.raw + PROJ_REC_OFF + at * 8u;
        pos = 46; vc("P2 the wire image has the event where the test looks", wire_bits_get(v, &pos, 8) == 15u);
        pos = 46; wire_bits_put(v, &pos, 0x7Fu, 8);      /* the image an older build wrote */
        {   uint32_t sum = proj_hash(st.raw, PROJ_STORE_SIZE - 4u); memcpy(st.raw + PROJ_STORE_SIZE - 4u, &sum, 4); }
        vc("P3 a project image (FUN wire) with an older build's 0x7F event loads", proj_import(&q2, &st, sizeof st) == 1);
        {   uint32_t k = 0, has7f = 0;
            for (i = 0; i < RECORD_MAX; i++) if (q2.recording[i].vel && (q2.recording[i].owner & 31u) == 2u) { k++; has7f |= q2.recording[i].step == 0x7Fu; }
            vc("P4 ... that event dropped, the five others kept, the project as checked (proj_ok)", k == 5u && !has7f && proj_ok(&q2));
        }
        {   /* the same with a pitch out of range: refused (the pitch code is 6 bits at bit 57) */
            project_store_t st2 = st;
            uint32_t p2 = 57; wire_bits_put(st2.raw + PROJ_REC_OFF + at * 8u, &p2, 62u, 6);   /* (pitch +38) */
            uint32_t sum = proj_hash(st2.raw, PROJ_STORE_SIZE - 4u); memcpy(st2.raw + PROJ_STORE_SIZE - 4u, &sum, 4);
            vc("P5 a project image with a 0x7F event and a pitch out of range is refused", proj_import(&q2, &st2, sizeof st2) == 0);
        }
        q2 = q;
        q2.recording[idx].step = 0x7Fu; q2.sum = proj_sum(&q2);
        vc("P6 a project_t with an older build's 0x7F event checks (proj_ok)", proj_ok(&q2));
        {   static project_t q3;
            uint32_t k = 0, has7f = 0;
            int ok = proj_import(&q3, &q2, sizeof q2) == 1;
            for (i = 0; ok && i < RECORD_MAX; i++) if (q3.recording[i].vel && (q3.recording[i].owner & 31u) == 2u) { k++; has7f |= q3.recording[i].step == 0x7Fu; }
            vc("P7 ... and imports with that event dropped, its sum right", ok && k == 5u && !has7f && q3.sum == proj_sum(&q3));
        }
        q2.recording[idx].pitch = 30; q2.sum = proj_sum(&q2);
        vc("P8 a project_t with a 0x7F event and a pitch out of range is refused", !proj_ok(&q2));
    }
    printf("pool valid: %s\n", vbad ? "FAILED" : "all checks passed");
    return vbad != 0;
}
