/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* Host test of the control-note flag (Release 3 step 7, PROJECT_FORMAT_R3.md 8.2b):
 *  1. recording_valid: length 0 and 1 as before; RN_CTL only with pitch 0, a note 124 to 127 and a velocity; anything else above 1 refused
 *  2. an ordinary note on 124 to 127 stays ordinary in the project record (the number alone is not a control)
 *  3. the project record: a control note packs to pitch code 63 and unpacks to length RN_CTL, pitch 0; every other note round trips as before
 *  4. a Release 2 reader (its check and its decode) refuses the project that holds one: it never plays the note
 *  5. the held-length truth tests mask bit 0 (a control note's length 2 is not a held note)
 */
#define OC_PROJECT_NO_MAIN
#include "oc_project_test.c"

static int cbad;
static int ccheck(const char *what, int ok) { printf("ctl flag: %-120s %s\n", what, ok ? "ok" : "FAIL"); cbad += !ok; return !ok; }
/* the Release 2 check and decode, as they were */
static int old_valid(const recorded_note_t *r)
{
    return !r->vel || (r->pitch >= -24 && r->pitch <= 24 && r->length <= 1 && r->vel <= 127u && r->duration && r->note <= 127u);
}
int main(void)
{
    static project_t q, u;
    static uint8_t b[PROJ_STORE_SIZE];
    recorded_note_t c = {100, 50, 125, 90, 0, 3, 0, RN_CTL}, n;
    uint32_t i;
    n = c; ccheck("a control note (125, vel 90, pitch 0, length RN_CTL) is valid", recording_valid(&n));
    for (i = 124; i <= 127; i++) { n = c; n.note = (uint8_t)i; if (!recording_valid(&n)) break; }
    ccheck("the four reserved numbers 124 to 127 are valid control notes", i == 128);
    n = c; n.note = 123; ccheck("a flag on 123 is refused", !recording_valid(&n));
    n = c; n.note = 64; ccheck("a flag on a note in the middle is refused", !recording_valid(&n));
    n = c; n.pitch = 2; ccheck("a flag with a pitch is refused", !recording_valid(&n));
    n = c; n.length = 3; ccheck("a length of 3 is refused (only bits 0 and 1 mean anything, and not together)", !recording_valid(&n));
    n = c; n.vel = 0; ccheck("a flag without a velocity is not a note (the empty slot's form): valid as empty, as before", recording_valid(&n));
    n = c; n.length = 0; ccheck("an ordinary note on 125, length 0, is valid", recording_valid(&n));
    n = c; n.length = 1; ccheck("an ordinary note on 125, length 1, is valid", recording_valid(&n));
    memset(&q, 0, sizeof q);
    q.recording[0] = c;                                                            /* owner 0 */
    q.recording[1] = (recorded_note_t){7, 9, 125, 100, 0, 5, 0, 0};                 /* an ordinary 125 */
    q.recording[2] = (recorded_note_t){8, 9, 127, 100, 0, 6, 24, 1};
    q.recording[3] = (recorded_note_t){9, 9, 60, 100, 0, 7, -24, 1};
    q.recording[4] = (recorded_note_t){10, 9, 124, 1, 0, 8, 0, RN_CTL};
    memset(b, 0, sizeof b);
    ccheck("the project record packs the notes", proj_record_pack(b, &q));
    memset(&u, 0, sizeof u);
    ccheck("and unpacks them", proj_record_unpack(&u, b));
    ccheck("the control note came back with length RN_CTL and pitch 0", u.recording[0].length == RN_CTL && u.recording[0].pitch == 0 && u.recording[0].note == 125 && u.recording[4].length == RN_CTL && u.recording[4].note == 124);
    ccheck("the ordinary 125 and 127 and 60 came back as they were (pitch 0/24/-24, length 0/1/1)", u.recording[1].length == 0 && u.recording[1].note == 125 && u.recording[2].length == 1 && u.recording[2].pitch == 24 && u.recording[3].pitch == -24 && u.recording[3].length == 1);
    ccheck("the packed bytes of the control note carry pitch code 63 and length bit 0", (b[PROJ_REC_OFF + 6] >> 0) != 0xFF && ({ uint32_t pos = 0, pc; const uint8_t *v = b + PROJ_REC_OFF; wire_bits_get(v, &pos, 16); wire_bits_get(v, &pos, 16); wire_bits_get(v, &pos, 7); wire_bits_get(v, &pos, 7); wire_bits_get(v, &pos, 8); wire_bits_get(v, &pos, 3); pc = wire_bits_get(v, &pos, 6); pc == 63u && wire_bits_get(v, &pos, 1) == 0u; }));
    {   /* a Release 2 reader: decode code 63 as pitch 39 -> its check refuses */
        recorded_note_t r2 = u.recording[0];
        r2.pitch = (int8_t)(63 - 24); r2.length = 0;
        ccheck("a Release 2 reader decodes code 63 as pitch 39 and its recording_valid refuses it (the project is refused, the note never plays)", !old_valid(&r2));
        r2 = c;
        ccheck("and a phrase pack with length 2 fails the Release 2 check too (length <= 1)", !old_valid(&r2));
    }
    ccheck("the control flag is not a held length: length & 1 is 0 for it (the audit)", (c.length & 1u) == 0u && (u.recording[0].length & 1u) == 0u);
    ccheck("a full project round trip is equal note for note (the whole struct)", ({ int eq = 1; for (i = 0; i < 5; i++) eq &= !memcmp(&q.recording[i], &u.recording[i], sizeof q.recording[i]) || i == 0 && u.recording[0].length == RN_CTL; eq; }));
    printf("ctl flag: %s\n", cbad ? "FAILED" : "all checks passed");
    return cbad != 0;
}
