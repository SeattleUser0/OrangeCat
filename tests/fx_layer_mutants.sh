# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
# Release 3 step 8b: the mutations of tests/fx_layer_module.sh --mutate (sourced by it): each changes ONE thing in the MODULE's copy of the kernel text (perf_kern_mut.h; the resident effects
# keep perf_kern.h) or in the module's own descriptor (tools/module/fxlayer_mod.c) and the test must fail. A line for every knob of each card the kernel reads, each effect's own arithmetic and
# state, what comes through the context (the tempo, SCRATCH's count, the head of the block, the fade's direction, the semitone service), the new text (start, run), and the descriptor.
# Not here, because they are not the kernel's: which effect plays and when (the keys, the press order, the 1/16: the host's), REPEAT, CRUSH, THROW, the mutes, the 32 KB's taking and losing.
# The third word of a line is the scenario the test runs first (the mutated effect's own), so a mutant is caught in seconds.
# On record (FX_MODULES_PLAN.md 17.7): the first run of this check reported five mutants caught that were not (the test's own fault, fixed: the verdict line). Of those, two were equivalent
# and are replaced here ("the reversed slice's speed" as -65,535: the same -16,384 after the shift as -65,536; "a slice read before it is recorded" at half the span: no different on a grid
# of whole 1/16s), one needed a stereo scenario (STEREO, with six mutants of the channels), two needed SCRATCH's. A sixth, "the mix at a full share" (w == 32767), was equivalent too (the
# share moves in steps of 256) and is "the mix's weight in a fade" now. One mutant is caught by run E alone (the two copies of the kernel side by side, every knob value), not by any render:
# "SHUFFLE AMOUNT: on the beats" (a threshold one step off in a hundred, compared with a hash once a beat: no render of sane length is sure to hear it).
# GLITCH
mutant 's/\* kc->gp\[1\], -24, 24));/* kc->gp[1], -24, 23));/' "GLITCH PITCH: the repeats' top interval (24 -> 23 semitones)" GLITCH
mutant 's/? ks->u.g.k : 24u) \* kc->gp\[1\], -24, 24));/? ks->u.g.k : 24u) * kc->gp[2], -24, 24));/' "GLITCH PITCH read from SIZE's byte" GLITCH
mutant '/pfk_glt_step/,/^}/ s/(ks->gs \& 3u ? d : d \* d \/ 100u)/(ks->gs \& 3u ? d * d \/ 100u : d)/' "GLITCH DENS: squared off the beats instead of on them" GLITCH
mutant 's/z = (uint32_t)clamp(kc->gp\[2\], 0, 3), j/z = (uint32_t)clamp(kc->gp[2], 0, 2), j/' "GLITCH SIZE: TINY read as SHORT" GLITCH
mutant '/pfk_glt_step/,/^}/ s/((uint32_t)(kc->gp\[3\] - 1) << 5)/((uint32_t)(kc->gp[3] - 1) << 4)/' "GLITCH VARI: the variation's place in the hash" GLITCH
mutant 's/ks->u.g.rate = -65536;/ks->u.g.rate = -65532;/' "GLITCH: the reversed slice's speed" GLITCH
mutant 's/s = g0 - 2u \* L;/s = g0 - L;/' "GLITCH: the octave-up slice's start" GLITCH
mutant 's/ks->u.g.rate = 32768;/ks->u.g.rate = 32767;/' "GLITCH: the octave-down slice's speed" GLITCH
mutant 's/% (ks->u.g.sp - L + 1u);/% (ks->u.g.sp - L + 2u);/' "GLITCH SKIP: where it jumps" GLITCH
mutant 's/g = ks->u.g.k \& 1u ? 0u : g;/g = ks->u.g.k \& 1u ? g : 0u;/' "GLITCH GATE: the open half" GLITCH
mutant 's/int32_t b = 1 << (ks->u.g.sh - 1u);/int32_t b = 1 << (ks->u.g.sh - 2u);/' "GLITCH CRUSH: the rounding" GLITCH
mutant 's/ks->u.g.h = (uint8_t)(1u + (h >> 14) % 6u);/ks->u.g.h = (uint8_t)(1u + (h >> 14) % 5u);/' "GLITCH CRUSH: the hold" GLITCH
mutant 's/g = e << (15 - PFK_RAMP_LOG2);/g = e << (14 - PFK_RAMP_LOG2);/' "GLITCH: a slice's window" GLITCH
mutant 's/if ((s16 + sp > PB_MAX - 512u || /if ((s16 + sp > PB_MAX - 5120u || /' "GLITCH: too slow to read a slice back (the tempo: 100 BPM read as too slow)" TEMPO
mutant '/static int pfk_glt(/,/^}/ s/if (++ks->gph >= ks->P) {/if (++ks->gph > ks->P) {/' "GLITCH: the 1/16 a sample long" GLITCH
mutant 's/        ks->u.g.k++;/        ks->u.g.k += 2u;/' "GLITCH: the segment count kept in the state" GLITCH
mutant 's/h \*= 0x9E3779B1u;/h *= 0x9E3779B3u;/' "the slices' hash" GLITCH
mutant 's/int32_t fr = (int32_t)((s >> 2) \& 0x7FFFu);/int32_t fr = (int32_t)((s >> 2) \& 0x3FFFu);/' "the ring's interpolation" GLITCH
mutant 's/pb_put(pb, f, (st->hl + l) >> 2, (st->hr + r) >> 2);/pb_put(pb, f, (st->hl + l) >> 2, (st->hr + l) >> 2);/' "the recorder: the right channel" STEREO
# TAPE STOP
mutant '/pfk_tape_start/,/^}/ s/clamp(kc->gp\[1\], 0, 4)/clamp(kc->gp[1], 0, 3)/' "TAPE TIME: 1/4 read as 1/8" TAPE
mutant 's/ks->u.t.c = c \* 327;/ks->u.t.c = c * 328;/' "TAPE CURVE: its scale" TAPE
mutant 's/m = TP_LAG \* 600u \/ (uint32_t)(300 - c);/m = TP_LAG * 601u \/ (uint32_t)(300 - c);/' "TAPE: the longest fall (a slow tempo)" TEMPO
mutant 's/ks->u.t.p = (PB_MAX - 4u) << 16;/ks->u.t.p = (PB_MAX - 6u) << 16;/' "TAPE: where it starts reading" TAPE
mutant 's/g = v < 8192u ? v << 2 : 32768u;/g = v < 8192u ? v << 1 : 32768u;/' "TAPE: the fade over its last quarter" TAPE
mutant 's/ks->u.t.q = ks->u.t.p1 = ks->u.t.p;/ks->u.t.q = ks->u.t.p;/' "TAPE stopped: the platter's place not set" TAPE
mutant 's/clamp((int32_t)(kc->scr - ks->u.t.sc), -64, 64)/clamp((int32_t)(kc->scr - ks->u.t.sc), -64, 63)/' "SCRATCH: a fast turn's limit" SCRATCH
mutant 's/clamp((int32_t)(kc->scr - ks->u.t.sc), -64, 64)/clamp((int32_t)(kc->scr - ks->u.t.sc), -63, 64)/' "SCRATCH: a fast turn's limit, backwards" SCRATCH
mutant 's/(int32_t)(b < 512u ? b : 512u)/(int32_t)(b < 512u ? b : 513u)/' "SCRATCH: a detent's longest step (a slow tempo)" TAPE
mutant 's/-(int32_t)(PB_MAX - 1024u), -4);/-(int32_t)(PB_MAX - 1024u), -5);/' "SCRATCH: the hand's nearest place" TAPE
mutant 's/uint32_t b = kc->beat \/ 48u;/uint32_t b = kc->beat \/ 47u;/' "SCRATCH: a detent is 1\/48 beat (the tempo through the context)" TAPE
mutant 's/if (kc->head \&\& ks->state) pfk_scratch(kc);/if (kc->head) pfk_scratch(kc);/' "SCRATCH taken while the tape still falls" SCRATCH
mutant 's/if (kc->head \&\& ks->state) pfk_scratch(kc);/if (!kc->head \&\& ks->state) pfk_scratch(kc);/' "SCRATCH taken in the wrong run of the block (the head flag)" TAPE
mutant 's/: 0) - ks->u.t.gn) >> 5;/: 0) - ks->u.t.gn) >> 4;/' "TAPE stopped: the level following the hand" TAPE
mutant '/pfk_tape_start/,/^}/ s/ks->u.t.sc = kc->scr;/ks->u.t.sc = 0;/' "TAPE: the detents before the press not forgotten" TAPE
# FREEZE
mutant 's/return 100u + 2u \* (uint32_t)clamp(kc->gp\[0\], 0, 100);/return 100u + 2u * (uint32_t)clamp(kc->gp[0], 0, 99);/' "FREEZE DENS: its top" FREEZE
mutant 's/static const int8_t FZ_JIT\[4\] = {12, -12, 7, -5};/static const int8_t FZ_JIT[4] = {12, -12, 7, -4};/' "FREEZE SPRAY: a grain moved a fourth" FREEZE
mutant 's/sp = (uint32_t)clamp(kc->gp\[2\], 0, 100), len/sp = (uint32_t)clamp(kc->gp[2], 0, 99), len/' "FREEZE SPRAY: its top" FREEZE
mutant 's/len = kc->beat >> (5u - (uint32_t)clamp(kc->gp\[3\], 0, 4)),/len = kc->beat >> (5u - (uint32_t)clamp(kc->gp[3], 0, 3)),/' "FREEZE SIZE: 1\/8 read as 1\/16" FREEZE
mutant 's/    int32_t n = kc->gp\[1\], rate;/    int32_t n = kc->gp[1] + 1, rate;/' "FREEZE PITCH: a semitone off" FREEZE
mutant 's/rate = (int32_t)kc->semi(n);/rate = (int32_t)kc->semi(n) + 1;/' "FREEZE: the grain's speed (the host's semitone service)" FREEZE
mutant 's/ks->u.f.rnd = 0x2545F491u;/ks->u.f.rnd = 0x2545F493u;/' "FREEZE: the generator's start" FREEZE
mutant 's/    ks->u.f.sd = 1;/    ks->u.f.sd = -1;/' "FREEZE: the scan's first direction" FREEZE
mutant 's/w = (int32_t)((v->x \* (65536u - v->x)) >> 15);/w = (int32_t)((v->x * (65536u - v->x)) >> 14);/' "FREEZE: a grain's window" FREEZE
mutant 's/j = ks->u.f.iv \/ 8u + 1u;/j = ks->u.f.iv \/ 4u + 1u;/' "FREEZE: the scan's speed" FREEZE
mutant 's/if (2u \* ks->wr < ks->u.f.c)/if (2u * ks->wr < ks->u.f.c + 2u)/' "FREEZE: frozen a frame late" FREEZE
mutant 's/ks->yl = (pl + ks->u.f.nl) >> 1;/ks->yl = pl;/' "FREEZE: the left sample between two grain evaluations" FREEZE
mutant 's/: PB_MAX - 1024u;/: PB_MAX - 1025u;/' "FREEZE: the longest eighth it keeps (a slow tempo, the tempo through the context)" TEMPO
# SHUFFLE
mutant 's/!(gs \& 3u) ? A \* A \/ 100u : A;/!(gs \& 3u) ? A * A \/ 101u : A;/' "SHUFFLE AMOUNT: on the beats" SHUFFLE
mutant 's/ch = 4u >> clamp(kc->gp\[1\], 0, 2), s16/ch = 4u >> clamp(kc->gp[1], 0, 1), s16/' "SHUFFLE SIZE: 1\/16 read as 1\/8" SHUFFLE
mutant 's/clamp(kc->gp\[2\], 0, 100) ? 1u + (h >> 13) % 3u : SK_NORM;/clamp(kc->gp[2], 0, 100) ? 1u + (h >> 13) % 2u : SK_NORM;/' "SHUFFLE SPEED: no chunk played twice" SHUFFLE
mutant '/pfk_shuf_step/,/^}/ s/((uint32_t)(kc->gp\[3\] - 1) << 5)/((uint32_t)(kc->gp[3] - 1) << 4)/' "SHUFFLE VARI: the variation's place in the hash" SHUFFLE
mutant 's/k == SK_HALF ? 16384u/k == SK_HALF ? 16385u/' "SHUFFLE: the half speed" SHUFFLE
mutant 's/AD_M\[8\] = {16, 17, 19, 21, 23, 25, 27, 29};/AD_M[8] = {16, 17, 19, 21, 23, 25, 27, 30};/' "SHUFFLE: an ADPCM step" SHUFFLE
mutant 's/if (c \& 2u) d += s >> 1;/if (c \& 2u) d += s >> 2;/' "SHUFFLE: the decoder's half step" SHUFFLE
mutant 's/(int32_t)(q->fr >> 1)) >> 15);/(int32_t)(q->fr >> 1)) >> 14);/' "SHUFFLE: the reader's interpolation" SHUFFLE
mutant 's/    ks->u.s.xn = PFK_RAMP;/    ks->u.s.xn = PFK_RAMP \/ 2;/' "SHUFFLE: the cross-fade between chunks" SHUFFLE
mutant 's/t\[1\] = (int16_t)ks->u.s.ep;/t[1] = 0;/' "SHUFFLE: the coder's value at a 1\/16 not noted (the table in the 32 KB's last frames)" SHUFFLE
mutant 's/ks->u.s.mid = ch \* ks->P \/ 2u;/ks->u.s.mid = ch * ks->P \/ 3u;/' "SHUFFLE: where a chunk played twice starts again" SHUFFLE
mutant 's/    ks->u.s.r\[0\].live = 1; /    ks->u.s.r[0].live = 0; /' "SHUFFLE start: not live before the first chunk moves" SHUFFLE
mutant 's/ad_put(kc, clamp((ks->hl + l + r) >> 3, -32768, 32767));/ad_put(kc, clamp((ks->hl + l + r) >> 2, -32768, 32767));/' "SHUFFLE: the recorded level" SHUFFLE
# the calls (new text) and the mix
mutant 's/ks->w += clamp(tw - ks->w, -PFK_SLOPE, PFK_SLOPE); \\/ks->w += clamp(tw - ks->w, -PFK_SLOPE, PFK_SLOPE + 1); \\/' "run: the fade in's slope" GLITCH
mutant 's/        tw = on \&\& kc->go ? 32768 : 0; \\/        tw = on ? 32768 : 0; \\/' "run: the effect does not fade out when let go (the context's go)" GLITCH
mutant 's/            bl\[i\] = pf_mix(l, yl, ks->w); \\/            bl[i] = pf_mix(l, yr, ks->w); \\/' "run: the left sample mixed from the right" GLITCH
mutant 's/return w == 32768 ? y : x + mulq16(y - x, (uint32_t)w << 1);/return w == 32768 ? y : x + mulq16(y - x, (uint32_t)w);/' "the mix's weight in a fade" GLITCH
mutant 's/PFK_FN static void pfk_glt_start(const fx3_ctx_t \*kc) { pfk_glt_step(kc); }/PFK_FN static void pfk_glt_start(const fx3_ctx_t *kc) { (void)kc; }/' "GLITCH start: no first step" GLITCH
# the two channels (STEREO: the other scenarios are all but mono at the layer)
mutant 's/\*r = 2 \* (p\[1\] + (((q\[1\] - p\[1\]) \* fr) >> 15));/*r = 2 * (p[1] + (((q[0] - p[1]) * fr) >> 15));/' "the ring's right channel interpolated towards the left" STEREO
mutant 's/ar += (gr \* w) >> 15;/ar += (gl * w) >> 15;/' "FREEZE: the grains' right channel" STEREO
mutant 's/ks->yr = mulq16(r, g << 1);/ks->yr = mulq16(l, g << 1);/' "GLITCH GATE: the right channel" STEREO
mutant 's/ks->u.g.cr = ((r + b) >> ks->u.g.sh) << ks->u.g.sh;/ks->u.g.cr = ((l + b) >> ks->u.g.sh) << ks->u.g.sh;/' "GLITCH CRUSH: the right channel" STEREO
mutant 's/            br\[i\] = pf_mix(r, yr, ks->w); \\/            br[i] = pf_mix(l, yr, ks->w); \\/' "run: the right sample's live share taken from the left" STEREO
mutant 's/        ks->yr = r;/        ks->yr = l;/' "SHUFFLE live: the right channel" STEREO
# the descriptor
mutant 's/#define PFK_WORK 8u /#define PFK_WORK 7u /' "the caps blocks declare 7 working blocks (the host lends the 32 KB: 8)"
mutant 's/{"VARI", F_INT, 1, 16, 1, 0, 0}};/{"VARI", F_INT, 1, 15, 1, 0, 0}};/' "the pages: VARI's range"
mutant 's/{"AMOUNT", F_INT, 0, 100, 60, 0, "%"}/{"AMOUNT", F_INT, 0, 100, 61, 0, "%"}/' "the pages: AMOUNT's default"
mutant 's/"SHORT", "TINY"}/"SHORT", "TINI"}/' "the pages: a name of GLITCH's SIZE"
mutant 's/FXL_PID_TAPE\[3\] = {{FW_PID_RELATIVE + 1u, FW_PID_NONE}/FXL_PID_TAPE[3] = {{FW_PID_VOLATILE + 1u, FW_PID_NONE}/' "the pages: SCRATCH a volatile id (a value) instead of a relative one"
mutant 's/{"CURVE", F_INT, -100, 100, 0, 0, 0}/{"CURVE", F_INT, -100, 100, 0, 0, "%"}/' "the pages: a unit on CURVE"
mutant 's/{ .effect_id = 1u + GX_FRZ,/{ .effect_id = 1u + GX_SHUF,/' "the bank: FREEZE's entry with SHUFFLE's effect id"
mutant 's/\.cost = 0, \.ram_extra_bytes = 0, \.gov_us = 0, \.abi = FX3_ABI_ID }/.cost = 0, .ram_extra_bytes = 0, .gov_us = 0, .abi = FX2_ABI_ID }/' "the caps blocks carry the send host's abi"
mutant 's/FXL_CALLS_SHUF = { PFK_CALLS_SHUF }/FXL_CALLS_SHUF = { PFK_CALLS_FRZ }/' "SHUFFLE's call table points at FREEZE's functions"
mutant 's/FXL_ROW_TAPE = {"TAPE STOP", FAM_FX, SC_FX, GR_NONE, {0, 1, 2, 0xFF}}/FXL_ROW_TAPE = {"TAPE STOP", FAM_FX, SC_FX, GR_NONE, {0, 1, 2, 2}}/' "the pages: a knob in TAPE STOP's empty place"
