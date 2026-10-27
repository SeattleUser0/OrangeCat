#!/usr/bin/env python3
# The placement A/B (measurement branch only): MACRO's voice kernel, its model dispatch and the VA and MODAL renders (VA BASS, MALLET)
# in .dsp_text, the native kernels' section (FM6's, the Prophet's, the 808's); every other model's render marked .text.xip_macro so
# tools/audio_ram.py keeps it (and what only it calls) in flash. Run from the checkout's root, once (measure/batch-s1-ram: applied).
import re, sys
RAM = '__attribute__((section(".dsp_text")))'
XIP = '__attribute__((section(".text.xip_macro")))'
def mark(path, names, attr):
    s = open(path).read()
    for n in names:
        pat = re.compile(r'^static (inline )?((?:__attribute__\(\(\w+\)\) )?void) (' + n + r')\(', re.M)
        m = pat.search(s)
        assert m, (path, n)
        assert not m.group(1), (path, n, "inline")
        s = s[:m.start()] + 'static ' + attr + ' ' + m.group(2) + ' ' + n + '(' + s[m.end():]
    open(path, 'w').write(s)
P = 'firmware/src/plaits/'
mark(P + 'pl_va.h', ['pl_vsaw_render', 'pl_vshape_render_t', 'pl_va_render'], RAM)
mark(P + 'pl_modal.h', ['pl_modal_svf_process', 'pl_modal_voice_render', 'pl_modal_render'], RAM)
for f, n in [('pl_waveshaping.h', 'pl_ws_render'), ('pl_fm.h', 'pl_fm_render'), ('pl_grain.h', 'pl_grain_render'), ('pl_additive.h', 'pl_add_render'),
             ('pl_chord.h', 'pl_chord_render'), ('pl_swarm.h', 'pl_swarm_render'), ('pl_noise.h', 'pl_noise_render'),
             ('pl_particle.h', 'pl_particle_render'), ('pl_string.h', 'pl_string_render'), ('pl_string.h', 'pl_harp_render'),
             ('pl_bassdrum.h', 'pl_bd_render'), ('pl_snare.h', 'pl_sd_render'), ('pl_hihat.h', 'pl_hh_render')]:
    mark(P + f, [n], XIP)
p = 'firmware/src/eng_macro.c'
s = open(p).read()
for a in ('static MC_XIP void mc_model_render(', 'static MC_XIP void mc_voice('):
    assert s.count(a) == 1, a
    s = s.replace(a, a.replace('MC_XIP', '__attribute__((section(".dsp_text"), noinline))'))
open(p, 'w').write(s)
print("MACRO's kernel, the VA and MODAL renders in .dsp_text; the other models' renders in .text.xip_macro")
