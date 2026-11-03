/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 * PLAITS (the model picker's module): the host build's one unit includes every model unit here (tests/module_host_plaits_tu.c), after the
 * glue's weak declarations. A unit left out with -DPL_HOST_NO_<key> stands for a model the pick left out: its descriptor reads as null. */
#pragma once
#ifndef PL_HOST_NO_va
#include "mdl_va.c"
#endif
#ifndef PL_HOST_NO_wavshp
#include "mdl_wavshp.c"
#endif
#ifndef PL_HOST_NO_fm
#include "mdl_fm.c"
#endif
#ifndef PL_HOST_NO_grain
#include "mdl_grain.c"
#endif
#ifndef PL_HOST_NO_addtv
#include "mdl_addtv.c"
#endif
#ifndef PL_HOST_NO_chord
#include "mdl_chord.c"
#endif
#ifndef PL_HOST_NO_swarm
#include "mdl_swarm.c"
#endif
#ifndef PL_HOST_NO_noise
#include "mdl_noise.c"
#endif
#ifndef PL_HOST_NO_partcl
#include "mdl_partcl.c"
#endif
#ifndef PL_HOST_NO_string
#include "mdl_string.c"
#endif
#ifndef PL_HOST_NO_modal
#include "mdl_modal.c"
#endif
#ifndef PL_HOST_NO_kick
#include "mdl_kick.c"
#endif
#ifndef PL_HOST_NO_snare
#include "mdl_snare.c"
#endif
#ifndef PL_HOST_NO_hihat
#include "mdl_hihat.c"
#endif
#ifndef PL_HOST_NO_harp
#include "mdl_harp.c"
#endif
#ifndef PL_HOST_NO_vavcf
#include "mdl_vavcf.c"
#endif
#ifndef PL_HOST_NO_pd
#include "mdl_pd.c"
#endif
#ifndef PL_HOST_NO_sixop
#include "mdl_sixop.c"
#endif
#ifndef PL_HOST_NO_terran
#include "mdl_terran.c"
#endif
#ifndef PL_HOST_NO_strmch
#include "mdl_strmch.c"
#endif
#ifndef PL_HOST_NO_chip
#include "mdl_chip.c"
#endif
#ifndef PL_HOST_NO_wavtbl
#include "mdl_wavtbl.c"
#endif
#ifndef PL_HOST_NO_speech
#include "mdl_speech.c"
#endif
#ifndef PL_HOST_NO_syx0
#include "tbl_syx0.c"
#endif
#ifndef PL_HOST_NO_syx1
#include "tbl_syx1.c"
#endif
#ifndef PL_HOST_NO_syx2
#include "tbl_syx2.c"
#endif
#ifndef PL_HOST_NO_wav0
#include "tbl_wav0.c"
#endif
#ifndef PL_HOST_NO_wav1
#include "tbl_wav1.c"
#endif
#ifndef PL_HOST_NO_wav2
#include "tbl_wav2.c"
#endif
