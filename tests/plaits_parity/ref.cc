// SPDX-License-Identifier: MIT
// Orange Cat 0.5.0: the original Plaits engines (Mutable Instruments' C++, MIT), for tests/plaits_parity.
#include "models.h"
#include "plaits/dsp/engine/virtual_analog_engine.h"
#include "plaits/dsp/engine/waveshaping_engine.h"
#include "plaits/dsp/engine/fm_engine.h"
#include "plaits/dsp/engine/grain_engine.h"
#include "plaits/dsp/engine/additive_engine.h"
#include "plaits/dsp/engine/wavetable_engine.h"
#include "plaits/dsp/engine/chord_engine.h"
#include "plaits/dsp/engine/swarm_engine.h"
#include "plaits/dsp/engine/noise_engine.h"
#include "plaits/dsp/engine/particle_engine.h"
#include "plaits/dsp/engine/string_engine.h"
#include "plaits/dsp/engine/modal_engine.h"
#include "plaits/dsp/engine/bass_drum_engine.h"
#include "plaits/dsp/engine/snare_drum_engine.h"
#include "plaits/dsp/engine/hi_hat_engine.h"
#include "stmlib/utils/random.h"
#include <cstring>

struct RefBox {
  plaits::Engine *e;
  char buffer[65536];
  stmlib::BufferAllocator alloc;
};

extern "C" void *ref_new(int model) {
  RefBox *b = new RefBox;
  memset(b->buffer, 0, sizeof b->buffer);
  b->alloc.Init(b->buffer, sizeof b->buffer);
  switch (model) {
    case M_VA: b->e = new plaits::VirtualAnalogEngine; break;
    case M_WAVESHAPING: b->e = new plaits::WaveshapingEngine; break;
    case M_FM: b->e = new plaits::FMEngine; break;
    case M_GRAIN: b->e = new plaits::GrainEngine; break;
    case M_ADDITIVE: b->e = new plaits::AdditiveEngine; break;
    case M_WAVETABLE: b->e = new plaits::WavetableEngine; break;
    case M_CHORD: b->e = new plaits::ChordEngine; break;
    case M_SWARM: b->e = new plaits::SwarmEngine; break;
    case M_NOISE: b->e = new plaits::NoiseEngine; break;
    case M_PARTICLE: b->e = new plaits::ParticleEngine; break;
    case M_STRING: b->e = new plaits::StringEngine; break;
    case M_MODAL: b->e = new plaits::ModalEngine; break;
    case M_BASS_DRUM: b->e = new plaits::BassDrumEngine; break;
    case M_SNARE: b->e = new plaits::SnareDrumEngine; break;
    case M_HI_HAT: b->e = new plaits::HiHatEngine; break;
    default: delete b; return 0;
  }
  b->e->Init(&b->alloc);
  b->e->LoadUserData(NULL);
  b->e->Reset();
  return b;
}

extern "C" void ref_render(void *box, const parity_params_t *p, float *out, float *aux, size_t size, int *enveloped) {
  RefBox *b = (RefBox *)box;
  plaits::EngineParameters q;
  q.trigger = p->trigger;
  q.note = p->note;
  q.timbre = p->timbre;
  q.morph = p->morph;
  q.harmonics = p->harmonics;
  q.accent = p->accent;
  bool env = *enveloped != 0;
  b->e->Render(q, out, aux, size, &env);
  *enveloped = env;
}

extern "C" void ref_seed(uint32_t s) { stmlib::Random::Seed(s); }
