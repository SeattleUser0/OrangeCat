// SPDX-License-Identifier: MIT
// Orange Cat 0.5.1: Braids' own MacroOscillator (Mutable Instruments' C++, MIT), for tests/braids_parity.
#include <cstdlib>
#include <new>
#include "braids/macro_oscillator.h"
#include "stmlib/utils/random.h"

extern "C" void *ref_new(void) {
  void *mem = calloc(1, sizeof(braids::MacroOscillator));     // (zeroed, as a static object would be on the module)
  braids::MacroOscillator *m = new (mem) braids::MacroOscillator;
  m->Init();
  return m;
}
extern "C" void ref_set(void *o, int shape, int pitch, int timbre, int color) {
  braids::MacroOscillator *m = (braids::MacroOscillator *)o;
  m->set_shape(static_cast<braids::MacroOscillatorShape>(shape));
  m->set_pitch(pitch);
  m->set_parameters(timbre, color);
}
extern "C" void ref_strike(void *o) { ((braids::MacroOscillator *)o)->Strike(); }
extern "C" void ref_render(void *o, const unsigned char *sync, short *out, unsigned n) {
  ((braids::MacroOscillator *)o)->Render(sync, out, n);
}
extern "C" void ref_seed(unsigned s) { stmlib::Random::Seed(s); }
