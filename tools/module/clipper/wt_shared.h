/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 *
 * Orange Cat, the engine candidates gate (docs/orangecat/ENGINE_CANDIDATES.md, "Clipper"; docs/orangecat/ENGINE_LOADING_PLAN.md, "Shared backends"): one
 * wavetable format, one frame cache and one frame read for the Clipper engines that play single-cycle waves (VECTOR today; WSEQ and WAVE on it only by
 * re-band-limiting their waves into this format: the plan's answer says what that costs).
 *
 * The format (a "bank"): frames of one single cycle, each in three band-limited copies, 512 points to the 96th harmonic, 128 to the 24th, 32 to the
 * 6th (WTS_PTS, 672 int16, 1,344 B), the copies one after the other (WTS_OFF): Clipper's VECTOR format (its vector_tables.h, tools/gen_vector.py).
 * A voice reads the copy whose top harmonic stays under ~20 kHz at its increment (wts_level: VECTOR's thresholds).
 *
 * The cache: per sample a voice reads a few frames (VECTOR: its four corners) at scattered points, and a bank read from the module's flash per sample is
 * what the cache rule forbids (docs/orangecat/ADDING_AN_ENGINE.md, section 4); a whole bank as a hot set is 16 x 1,344 = 21,504 B of arena. So each part
 * keeps the frames it plays in its own arena block (wts_cache_t: up to WTS_SLOTS frames, 1,344 B each), copied from the bank when the part's choice
 * changes (wts_frame: a slot holds frame idx of bank; a new idx copies 1,344 B, once). The engine reads the slot's RAM.
 *
 * The read: wts_lerp(frame, ph, sh, mask), the linear interpolation between a copy's points (VECTOR's, as its loop writes it), and the level's index
 * shift and mask (wts_shift, wts_mask) for a level. An engine weights and sums frames itself (VECTOR: four, bilinear; a crossfade: two).
 */
#ifndef WT_SHARED_H
#define WT_SHARED_H 1
#define WTS_LEVELS 3u
#define WTS_PTS (512u + 128u + 32u)
#define WTS_SLOTS 4u
static const uint16_t WTS_OFF[WTS_LEVELS] = {0, 512, 640};
static inline uint32_t wts_level(uint32_t inc) { return inc < 20270000u ? 0u : inc < 81100000u ? 1u : 2u; }   /* the copy for an increment */
static inline uint32_t wts_shift(uint32_t lv) { return 23u + 2u * lv; }                                       /* index = ph >> shift */
static inline uint32_t wts_mask(uint32_t lv) { return (512u >> (2u * lv)) - 1u; }
typedef struct {
    int16_t f[WTS_SLOTS][WTS_PTS];                /* the frames the part plays (RAM) */
    uint8_t idx[WTS_SLOTS];                       /* frame number + 1 each slot holds (0: none) */
} wts_cache_t;
/* slot s of cache c holding frame idx of bank (nf frames): copied on a change */
static inline const int16_t *wts_frame(wts_cache_t *c, uint32_t s, const int16_t (*bank)[WTS_PTS], uint32_t nf, uint32_t idx)
{
    idx = idx < nf ? idx : nf - 1u;
    if (c->idx[s] != idx + 1u) {
        memcpy(c->f[s], bank[idx], sizeof c->f[s]);
        c->idx[s] = (uint8_t)(idx + 1u);
    }
    return c->f[s];
}
/* a copy's value at phase ph: points k and k + 1 (k = ph >> sh), linearly, Q15 between */
static inline int32_t wts_lerp(const int16_t *w, uint32_t ph, uint32_t sh, uint32_t mask)
{
    uint32_t k = ph >> sh, k1 = (k + 1u) & mask;
    int32_t fr = (int32_t)((ph >> (sh - 15u)) & 0x7FFFu);
    return w[k] + (((w[k1] - w[k]) * fr) >> 15);
}
#endif
