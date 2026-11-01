/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat, Release 3 (branch r3-format), Part 7: SCENES VERSION 2 (docs/orangecat/PROJECT_FORMAT_R3.md section 6; UI_BUILD.md "Part 7"). Behind
 * OC_R3_SCENES (needs OC_R3_FORMAT; off in the build set). Included by project.c before oc_project.c: the music's scene set, and the SCNS section's
 * bytes (encode, decode, the conversion of version 1). The recall and store (oc_scenes2_run.c) come after the lists.
 *
 * SCNS version 2 (a record section of the OCJ4 record; the section is a length-prefixed blob, so a later version may append and an older firmware carries it):
 *   version u8 (2), count u8 (16), used u16 (a bit a scene), nsnd u8 (the sound table's entries, at most 16), jfxn u8 (PROJ_JFX_N: a later firmware's
 *   effect block is another size, which this one reads as foreign),
 *   nsnd x sound entry: len u16 (of what follows), engine u8, preset u8, np u16, np x value s16 (little endian), native length u8, the native bytes
 *   count x scene entry: name u8, mask u8, boundary u8, 3 reserved (zero; read and carried as zero), then the parts the mask names, in this order:
 *     tempo (mask 1)           bpm u16, swing u16
 *     sounds (mask 2)          NTRK x (index into the sound table + 1; 0 none)
 *     mixer (mask 4)           NTRK x (level, pan, mute): level and pan are the value + 64 clamped to a byte, mute 0 or 1
 *     effects (mask 8)         PROJ_JFX_N global effect values (+ 64), then NTRK x 3 sends (the reverb, delay, chorus send, + 64)
 *     chains and mutes (16, 32) NTRK x (the chain + 1, the step | the mute << 7): as today's scene; both bits name this one part, and which of the
 *                              two a recall obeys is the bit
 *   An entry for a scene whose used bit is clear is the 6 byte header with mask 0. Anything after the last entry is carried (a later version's).
 * Hostile bytes: every length is checked against the section before it is used; a count other than 16, a sound table past 16 or an index past the table,
 * a mask with bit 6 or 7, an entry that runs past the end: the section is "foreign" (kept byte for byte, plays no scenes), as Part 1's rule says.
 * Version 1 (today's: count 8, the scenes as the lists hold them, used u16) converts on load: mask 48 (chains and mutes), boundary 0 (the project's launch),
 * used widened; the next save writes version 2. The first 8 scenes' chain and mute cells are mirrored into lists.scene (the ARRANGE command writes the lists
 * whole, the PERFORM page and the old paths read them), by scn2_sync_from_lists and scn2_sync_to_lists.
 * Revisions:
 *   Part 7 (November 1, 2026): first version. */
#if OC_R3_SCENES
#define SCN2_MAXSND 16u
#define SCN2_MAXNP 160u
enum { SCM_TEMPO = 1, SCM_SOUND = 2, SCM_MIXER = 4, SCM_FX = 8, SCM_CHAIN = 16, SCM_MUTE = 32, SCM_ALL = 63 };
typedef struct {
    uint8_t name, mask, boundary;
    int16_t bpm, swing;
    uint8_t snd[NTRK];                         /* the sound table's index + 1 */
    uint8_t lvl[NTRK], pan[NTRK], mute[NTRK];
    uint8_t fx[PROJ_JFX_N], send[NTRK][3];
    uint8_t chain[NTRK], stepmute[NTRK];       /* the chain + 1 (0: left as it is); the step | the mute << 7 */
} scn2_t;
typedef struct { uint8_t engine, preset, used; uint16_t np; int16_t p[SCN2_MAXNP]; } scn2_snd_t;
typedef struct { uint16_t used; uint8_t nsnd; scn2_t s[SCN2_N]; scn2_snd_t snd[SCN2_MAXSND]; } scn2_set_t;
static scn2_set_t scn2 __attribute__((section(".pool")));          /* the music's scenes */
static scn2_set_t scn2_in __attribute__((section(".pool")));       /* a record's, while it is read */
#define SCN2_MAX_BYTES (4u + 2u + SCN2_MAXSND * (2u + 5u + 2u * SCN2_MAXNP + 1u) + SCN2_N * (6u + 4u + NTRK + NTRK * 3u + PROJ_JFX_N + NTRK * 3u + NTRK * 2u))
static uint8_t scn2_buf[SCN2_MAX_BYTES] __attribute__((section(".pool")));
static uint32_t scn2_buf_len;

static void scn2_store(uint32_t s, uint32_t mask);                                  /* (oc_scenes2_run.c, after the lists) */
static int scn2_recall(uint32_t s, uint32_t *missing, uint32_t *skipped);
static void scn2_clear(scn2_set_t *z) { memset(z, 0, sizeof *z); }
static uint32_t scn2_part_len(uint32_t mask)
{
    return (mask & SCM_TEMPO ? 4u : 0u) + (mask & SCM_SOUND ? NTRK : 0u) + (mask & SCM_MIXER ? NTRK * 3u : 0u) +
           (mask & SCM_FX ? PROJ_JFX_N + NTRK * 3u : 0u) + (mask & (SCM_CHAIN | SCM_MUTE) ? NTRK * 2u : 0u);
}
static void scn2_p16(uint8_t *b, uint32_t v) { b[0] = (uint8_t)v; b[1] = (uint8_t)(v >> 8); }
static uint32_t scn2_g16(const uint8_t *b) { return (uint32_t)b[0] | (uint32_t)b[1] << 8; }
/* the encoded set into scn2_buf; scn2_buf_len */
static void scn2_encode(const scn2_set_t *z)
{
    uint8_t *b = scn2_buf;
    uint32_t i, k, at = 0;
    b[at++] = 2; b[at++] = SCN2_N; scn2_p16(b + at, z->used); at += 2; b[at++] = z->nsnd; b[at++] = PROJ_JFX_N;
    for (i = 0; i < z->nsnd; i++) {
        const scn2_snd_t *d = &z->snd[i];
        scn2_p16(b + at, 5u + 2u * d->np); at += 2;
        b[at++] = d->engine; b[at++] = d->preset; scn2_p16(b + at, d->np); at += 2;
        for (k = 0; k < d->np; k++) { scn2_p16(b + at, (uint16_t)d->p[k]); at += 2; }
        b[at++] = 0;                                   /* (the native patch: Part 7 stores none) */
    }
    for (i = 0; i < SCN2_N; i++) {
        const scn2_t *e = &z->s[i];
        uint32_t mask = (z->used >> i) & 1u ? e->mask : 0u;
        b[at++] = e->name; b[at++] = (uint8_t)mask; b[at++] = e->boundary; b[at++] = 0; b[at++] = 0; b[at++] = 0;
        if (mask & SCM_TEMPO) { scn2_p16(b + at, (uint16_t)e->bpm); scn2_p16(b + at + 2, (uint16_t)e->swing); at += 4; }
        if (mask & SCM_SOUND) { memcpy(b + at, e->snd, NTRK); at += NTRK; }
        if (mask & SCM_MIXER) for (k = 0; k < NTRK; k++) { b[at++] = e->lvl[k]; b[at++] = e->pan[k]; b[at++] = e->mute[k]; }
        if (mask & SCM_FX) { memcpy(b + at, e->fx, PROJ_JFX_N); at += PROJ_JFX_N; memcpy(b + at, e->send, NTRK * 3u); at += NTRK * 3u; }
        if (mask & (SCM_CHAIN | SCM_MUTE)) for (k = 0; k < NTRK; k++) { b[at++] = e->chain[k]; b[at++] = e->stepmute[k]; }
    }
    scn2_buf_len = at;
}
/* a section's bytes (version 2) into z. 0 read; -1 foreign (the whole section is kept as it is). *used_len: how many bytes were understood (the rest is a later version's) */
static int scn2_decode(const uint8_t *b, uint32_t n, scn2_set_t *z, uint32_t *used_len)
{
    uint32_t i, k, at = 6, nsnd;
    scn2_clear(z);
    if (n < 6u || b[0] != 2u || b[1] != SCN2_N || b[5] != PROJ_JFX_N) return -1;
    z->used = (uint16_t)scn2_g16(b + 2); nsnd = b[4];
    if (nsnd > SCN2_MAXSND) return -1;
    z->nsnd = (uint8_t)nsnd;
    for (i = 0; i < nsnd; i++) {
        uint32_t len, np;
        scn2_snd_t *d = &z->snd[i];
        if (at + 2u > n) return -1;
        len = scn2_g16(b + at); at += 2;
        if (len < 5u || len > n - at) return -1;
        d->engine = b[at]; d->preset = b[at + 1]; np = scn2_g16(b + at + 2);
        if (np > SCN2_MAXNP || 5u + 2u * np > len) return -1;
        for (k = 0; k < np; k++) d->p[k] = (int16_t)scn2_g16(b + at + 4u + 2u * k);
        d->np = (uint16_t)np; d->used = 1;
        at += len;                                      /* (a native patch, or a later version's tail of the entry, is skipped) */
    }
    for (i = 0; i < SCN2_N; i++) {
        scn2_t *e = &z->s[i];
        uint32_t mask, pl;
        if (at + 6u > n) return -1;
        e->name = b[at]; mask = b[at + 1]; e->boundary = b[at + 2];
        if (mask > SCM_ALL || e->boundary > 4u) return -1;
        e->mask = (uint8_t)mask; at += 6;
        pl = scn2_part_len(mask);
        if (pl > n - at) return -1;
        if (mask & SCM_TEMPO) { e->bpm = (int16_t)scn2_g16(b + at); e->swing = (int16_t)scn2_g16(b + at + 2); at += 4; }
        if (mask & SCM_SOUND) {
            memcpy(e->snd, b + at, NTRK); at += NTRK;
            for (k = 0; k < NTRK; k++) if (e->snd[k] > nsnd) return -1;
        }
        if (mask & SCM_MIXER) for (k = 0; k < NTRK; k++) { e->lvl[k] = b[at]; e->pan[k] = b[at + 1]; e->mute[k] = b[at + 2] & 1u; if (b[at + 2] > 1u) return -1; at += 3; }
        if (mask & SCM_FX) { memcpy(e->fx, b + at, PROJ_JFX_N); at += PROJ_JFX_N; memcpy(e->send, b + at, NTRK * 3u); at += NTRK * 3u; }
        if (mask & (SCM_CHAIN | SCM_MUTE)) for (k = 0; k < NTRK; k++) { e->chain[k] = b[at]; e->stepmute[k] = b[at + 1]; at += 2; if (b[at - 2] > OCL_NCHAIN) return -1; }
    }
    if (used_len) *used_len = at;
    return 0;
}
/* version 1's scenes (today's lists) as version 2: chains and mutes, the project's boundary */
static void scn2_from_v1(scn2_set_t *z, const uint8_t scene[OCL_NSCENE][NTRK][2], uint32_t used)
{
    scn2_clear(z);
    for (uint32_t s = 0; s < OCL_NSCENE; s++) {
        scn2_t *e = &z->s[s];
        if (!((used >> s) & 1u)) continue;
        z->used |= (uint16_t)(1u << s);
        e->mask = SCM_CHAIN | SCM_MUTE; e->boundary = 0;
        for (uint32_t k = 0; k < NTRK; k++) { e->chain[k] = scene[s][k][0]; e->stepmute[k] = scene[s][k][1]; }
    }
}
/* the first 8 scenes' chain and mute cells to and from the lists (the ARRANGE command and the old paths read and write lists.scene) */
static void scn2_sync_to_lists(const scn2_set_t *z, ocl_lists_t *l)
{
    memset(l->scene, 0, sizeof l->scene); l->scene_used = 0;
    for (uint32_t s = 0; s < OCL_NSCENE; s++) {
        if (!((z->used >> s) & 1u)) continue;
        l->scene_used |= (uint8_t)(1u << s);
        if (z->s[s].mask & (SCM_CHAIN | SCM_MUTE))
            for (uint32_t k = 0; k < NTRK; k++) { l->scene[s][k][0] = z->s[s].chain[k]; l->scene[s][k][1] = z->s[s].stepmute[k]; }
    }
}
static void scn2_sync_from_lists(scn2_set_t *z, const ocl_lists_t *l)
{
    for (uint32_t s = 0; s < OCL_NSCENE; s++) {
        scn2_t *e = &z->s[s];
        if (!((l->scene_used >> s) & 1u)) { z->used &= (uint16_t)~(1u << s); memset(e, 0, sizeof *e); continue; }
        if (!((z->used >> s) & 1u)) { z->used |= (uint16_t)(1u << s); e->mask = SCM_CHAIN | SCM_MUTE; }
        if (!(e->mask & (SCM_CHAIN | SCM_MUTE))) e->mask |= SCM_CHAIN | SCM_MUTE;
        for (uint32_t k = 0; k < NTRK; k++) { e->chain[k] = l->scene[s][k][0]; e->stepmute[k] = l->scene[s][k][1]; }
    }
}

/* R3-AUDIO-INTERRUPT (not merged; docs/orangecat/R3_AUDIO_INTERRUPT.md): the scene fade (xf_len, the per-track arm equal to launch_here, the hold until the swap and
 * the commit with a preset load's side effects, note-on dropping, the cancel) and the slotless staged phrase with a per-track boundary. Session A reviews and times
 * them. Until they are merged scn2_recall applies a scene's sound as a preset change is today, and a phrase key launches on demand. */
#endif
