/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
#ifndef MELODEE_STEP_SNAPSHOT_H
#define MELODEE_STEP_SNAPSHOT_H
/* Compact steps for history and the inactive-pattern pool. Preserve every field, including unused note
 * bytes and n on rests/ties. Three bounded control fields share one byte;
 * all other bytes remain exact.
 * (Orange Cat 0.2, build doc phase 4a) Skiff's step fields in two bytes more, Melodee's nine left exactly as they
 * were: x = the flags above SF_RECORDED (RATCH, FLAM: bits 3..5 down to 0..2) | the trig condition << 3 (0..31);
 * nudge as it is. Eleven bytes; exact for every flag a step may hold (SF_ALL), every condition below 32 and every
 * nudge byte */
typedef struct {
    uint8_t note[4], meta, vel, hit, acc, probability, x;
    int8_t nudge;
} step_snapshot_t;
_Static_assert(sizeof(step_snapshot_t) == 11u, "compact history step");
_Static_assert(ST_REST < 4u && (SF_ACCENT | SF_SLIDE | SF_RECORDED) < 8u,
               "history control fields fit one byte");
_Static_assert((SF_ALL >> 3) < 8u && TC_COUNT <= 32, "Skiff's flags and the conditions fit one byte");
static void step_snapshot_put(step_snapshot_t *out, const step_t *in)
{
    for (uint32_t i = 0; i < 4u; i++) out->note[i] = in->note[i];
    out->meta = (uint8_t)(in->n | (in->time << 3) | (in->flags << 5));
    out->vel = in->vel; out->hit = in->hit; out->acc = in->acc; out->probability = in->probability;
    out->x = (uint8_t)(((in->flags >> 3) & 7u) | (in->cond << 3));
    out->nudge = in->nudge;
}
static step_t step_snapshot_get(const step_snapshot_t *in)
{
    step_t out;
    for (uint32_t i = 0; i < 4u; i++) out.note[i] = in->note[i];
    out.n = in->meta & 7u; out.time = (in->meta >> 3) & 3u;
    out.flags = (uint8_t)((in->meta >> 5) | (in->x & 7u) << 3);
    out.vel = in->vel; out.hit = in->hit; out.acc = in->acc; out.probability = in->probability;
    out.cond = in->x >> 3; out.nudge = in->nudge;
    return out;
}
static void step_snapshot_save(step_snapshot_t *out, const step_t *in)
{
    for (uint32_t i = 0; i < NSTEP; i++) step_snapshot_put(&out[i], &in[i]);
}
static void step_snapshot_load(step_t *out, const step_snapshot_t *in)
{
    for (uint32_t i = 0; i < NSTEP; i++) out[i] = step_snapshot_get(&in[i]);
}
static int step_snapshot_equal(const step_snapshot_t *a, const step_t *b)
{
    for (uint32_t i = 0; i < NSTEP; i++) {
        step_t s = step_snapshot_get(&a[i]);
        if (memcmp(&s, &b[i], sizeof s)) return 0;
    }
    return 1;
}
#endif
