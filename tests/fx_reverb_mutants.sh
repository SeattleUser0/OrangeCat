# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
# Release 3 step 8a: the mutations of tests/fx_reverb_module.sh --mutate (sourced by it): each changes ONE thing in the MODULE's copy of the kernel text (rev_kern_mut.h; the resident modes keep
# rev_kern.h) and the test must fail. One line a knob of each mode that the kernel reads (SIZE, DAMP / FOLW, AIR / PITCH, MOD, RATE, WIDE), the state and the services, and the descriptor.
# Not here, because they are not the kernel's: FILT (the send's tone) and SPRING's PRE in milliseconds (the pre-delay) are the host's, before the kernel; SHIMMER's SIZE and DAMP are the
# resident ROOM network's, which the module calls through the context.
# SPRING
mutant 's/int32_t g = 19661 + (int32_t)s \* 85 /int32_t g = 19661 + (int32_t)s * 86 /' "SPRING SIZE: the loop's gain slope 85 -> 86"
mutant 's/(s \* 1323u) >> 7/(s * 1324u) >> 7/' "SPRING SIZE: the loop's length"
mutant 's/int32_t kl = 26000 - clamp(kc->g\[G_RDAMP\], 0, 127) \* 160;/int32_t kl = 26000 - clamp(kc->g[G_RDAMP], 0, 127) * 161;/' "SPRING DAMP: the loop's low-pass 160 -> 161 a step"
mutant 's/(3 + rm \/ 2)/(3 + rm \/ 3)/' "SPRING MOD: the wobble's depth"
mutant 's/kc->lfo_inc\[rm ? kc->g\[G_RRATE\] & 127 : 24\]/kc->lfo_inc[rm ? kc->g[G_RRATE] \& 63 : 24]/' "SPRING RATE: the wobble's rate read through 6 bits (the host's LFO table)"
mutant 's/kc->lfo_inc\[rm ? kc->g\[G_RRATE\] & 127 : 24\]/kc->lfo_inc[rm ? kc->g[G_RRATE] \& 127 : 25]/' "SPRING: the wobble's rate with MOD 0 (24 -> 25)"
mutant 's/& SP_MASK\]) \* 3 \* wd) >>/\& SP_MASK]) * 2 * wd) >>/' "SPRING WIDE: the side's level"
mutant 's/if (wd) revk_spring_side(kc, n, wd);/if (wd > 1) revk_spring_side(kc, n, wd);/' "SPRING WIDE 1: the side dropped"
mutant 's/#define SP_A 2867 /#define SP_A 2866 /' "SPRING: the allpass chain's coefficient"
mutant 's/kc->st->sp_he = o & 63;/kc->st->sp_he = 0;/' "SPRING: the low cut's remainder not kept in the state"
mutant 's/kc->sr\[i\] -= sd;\n    }\n}//; /revk_spring_side/,/^}/ s/kc->sr\[i\] -= sd;/kc->sr[i] += sd;/' "SPRING WIDE: the right side's sign"
# SHIMMER
mutant 's/amt = clamp(kc->g\[G_RMOD\], 0, 127) \* 190;/amt = clamp(kc->g[G_RMOD], 0, 127) * 191;/' "SHIMMER MOD: the shimmer's amount"
mutant 's/st = clamp(12 + air, -12, 24)/st = clamp(12 + air, -12, 23)/' "SHIMMER AIR: the grains' top speed (AIR +12 and above)"
mutant 's/tk = air < 0 ? 32767 + air \* 1150 : 9000;/tk = air < 0 ? 32767 + air * 1150 : 9001;/' "SHIMMER AIR up: the shelf's corner"
mutant 's/tk = air < 0 ? 32767 + air \* 1150 : 9000;/tk = air < 0 ? 32767 + air * 1151 : 9000;/' "SHIMMER AIR down: the low-pass"
mutant 's/in\[i\] = rev_in\[i\] + mulq15(kc->fb\[i\], amt);/in[i] = rev_in[i] - mulq15(kc->fb[i], amt);/' "SHIMMER: the feedback block's sign (the host's block, through the context)"
mutant 's/    kc->room(in, t, n);/    kc->room(rev_in, t, n);/' "SHIMMER: the resident ROOM network fed the send without the feedback"
mutant 's/x += ((x0 + (((x1 - x0) \* f) >> 8)) \* w) >> 9;/x += ((x0 + (((x1 - x0) * f) >> 8)) * w) >> 10;/' "SHIMMER: the grains' level"
mutant 's/return g\[G_RTYPE\] == 2 || g\[G_RTYPE\] == 4 ? clamp(g\[G_RPRE\], -24, 24) : 0;/return g[G_RTYPE] == 2 || g[G_RTYPE] == 4 ? clamp(g[G_RPRE], -24, 23) : 0;/' "AIR's range (24 -> 23 semitones)"
# RESO
mutant 's/26000 + sz \* 52 :/26000 + sz * 53 :/' "RESO SIZE: the strings' sustain"
mutant 's/clamp(31200 + (sz - 100) \* 58, 0, 32740)/clamp(31200 + (sz - 100) * 58, 0, 32739)/' "RESO SIZE at the top: the feedback's ceiling"
mutant 's/(uint32_t)clamp(g\[G_RPRE\], 0, 14) : 0u;/(uint32_t)clamp(g[G_RPRE], 0, 13) : 0u;/' "RESO PITCH: its range (14 -> 13 steps)"
mutant 's/return g\[G_RDAMP\] >= 1 && g\[G_RDAMP\]/return g[G_RDAMP] >= 2 \&\& g[G_RDAMP]/' "RESO FOLW: T1 not followed"
mutant 's/uint32_t lo = kc->reso_lo;/uint32_t lo = 0xFFu;/' "RESO FOLW: the followed track's note (the host's input) not read"
mutant 's/mask = kc->reso_mask;/mask = 0xFFFu;/' "RESO: the scale (the host's input) not read"
mutant 's/if (kc->g\[G_RMOD\] > 64) {/if (kc->g[G_RMOD] > 65) {/' "RESO MOD: the open voicing's threshold"
mutant 's/(int32_t)kc->st->rs_per\[k\]) \/ 6);/(int32_t)kc->st->rs_per[k]) \/ 7);/' "RESO: the strings' glide"
mutant 's/int32_t in = mulq15(rev_in\[i\], 3600), a = 0;/int32_t in = mulq15(rev_in[i], 3601), a = 0;/' "RESO: the input's level"
# the lines' knee (the part of the text the resident ROOM shares; the module's copy is what SPRING's line write calls)
mutant 's/(softclip(clamp(a - 24576, 0, 16384) \* 4) >> 2);/(softclip(clamp(a - 24576, 0, 16384) * 4) >> 3);/' "the lines' soft knee (SPRING's loop above -2.5 dBFS)"
# the descriptor
mutant 's/#define REVK_LINE_SHIMMER 1u/#define REVK_LINE_SHIMMER 0u/' "a capability word: SHIMMER declares no line block"
mutant 's/#define REVK_FLAGS 8u /#define REVK_FLAGS 9u /' "a capability word: the bank asks for a seed"
mutant 's/#define REVK_VERSION 0x00010000u/#define REVK_VERSION 0x00010001u/; s/_Static_assert(OC_MODULE_VERSION == REVK_VERSION, "one version");//' "the descriptor's version word"
