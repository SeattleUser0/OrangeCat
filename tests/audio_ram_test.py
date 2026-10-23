#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""Verify closure, ABI and safe branches across XIP/SRAM for the IR pass."""
import sys
import unittest
import tempfile
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from audio_ram import relocate, FUNCTION, function_type, engine_roots

IR = '''define void @fm1_alnk0_irq() {
  call fastcc void @render(i32 32)
  store void (i8*)* @worker, void (i8*)** @callback
  ret void
}
define internal fastcc void @render(i32 %n) {
  call void @hot(i32 %n)
  ret void
}
define void @worker(i8* %context) {
  ret void
}
define void @hot(i32 %n) section ".dsp_text" {
  ret void
}
define void @boot() {
  call fastcc void @render(i32 32)
  ret void
}
define void @accept(void (i8*, i32)* %job, i8* %context) {
  ret void
}
@callback = global void (i8*)* null
'''

class AudioRamTest(unittest.TestCase):
    def test_closure_and_crossing(self):
        output, selected, far = relocate(IR, {'fm1_alnk0_irq'})
        self.assertEqual(set(selected), {'fm1_alnk0_irq','render','hot','worker'})
        self.assertEqual(far,['render'])
        self.assertIn('call fastcc void %audio_far_', output)
        self.assertIn('call void @hot(i32 %n)',output)
        self.assertIn('call fastcc void @render(i32 32)',output)
        self.assertIn('@__audio_far_render = internal global void (i32)* @render',output)
        functions={m[2]:m for m in FUNCTION.finditer(output)}
        self.assertIn('section ".audio_text"',functions['worker'][4])
        self.assertIn('section ".dsp_text"',functions['hot'][4])
        self.assertNotIn('section',functions['boot'][4])

    def test_xip_section_stays_in_flash(self):
        # (Orange Cat 0.5.0) .text.xip*: reached from the ISR, kept in flash with what only it calls
        ir = IR + '''define void @macro_render(i32 %n) section ".text.xip_macro" {
  call void @model(i32 %n)
  call fastcc void @render(i32 %n)
  ret void
}
define void @model(i32 %n) {
  ret void
}
define void @isr2() {
  call void @macro_render(i32 1)
  ret void
}
'''
        output, selected, far = relocate(ir, {'fm1_alnk0_irq', 'isr2', 'macro_render'})
        self.assertNotIn('macro_render', selected)
        self.assertNotIn('model', selected)
        self.assertIn('macro_render', far)                      # RAM -> flash: through a pointer
        self.assertIn('render', far)                            # flash -> RAM: through a pointer
        functions = {m[2]: m for m in FUNCTION.finditer(output)}
        self.assertIn('section ".text.xip_macro"', functions['macro_render'][4])
        self.assertNotIn('section', functions['model'][4])
        self.assertIn('call void @model(i32 %n)', output)       # flash -> flash: direct

    def test_callback_abi(self):
        fn=next(m for m in FUNCTION.finditer(IR) if m[2]=='accept')
        self.assertEqual(function_type(fn),'void (void (i8*, i32)*, i8*)')

    def test_flash_memcpy_memset_own_copies(self):
        # (Orange Cat 0.6.1) memcpy / memset in SRAM: flash functions call flash's own copies, directly (their struct
        # copies and zeroings: LLVM 4's intrinsics, a constant GEP among the arguments; their written calls); a
        # direct call of the SRAM ones is out of branch range from far into the flash
        ir = IR + '''define i8* @memcpy(i8* returned %d, i8* nocapture readonly %s, i32 %n) #1 {
  ret i8* %d
}
define i8* @memset(i8* returned %d, i32 %c, i32 %n) local_unnamed_addr #1 {
  ret i8* %d
}
define void @isr3(i8* %p, i32 %n) {
  %1 = call i8* @memcpy(i8* %p, i8* %p, i32 %n)
  %2 = call i8* @memset(i8* %p, i32 0, i32 %n)
  call void @llvm.memcpy.p0i8.p0i8.i32(i8* %p, i8* %p, i32 16, i32 4, i1 false)
  ret void
}
define void @ui_copy(i8* %a, i8* %b) {
  call void @llvm.memcpy.p0i8.p0i8.i32(i8* %a, i8* getelementptr inbounds (%struct.s, %struct.s* @gs, i32 0, i32 1), i32 80, i32 4, i1 false), !tbaa.struct !3
  tail call void @llvm.memset.p0i8.i32(i8* %b, i8 0, i32 64, i32 4, i1 false)
  %3 = load i8, i8* %a
  call void @llvm.memcpy.p0i8.p0i8.i32(i8* %a, i8* %b, i32 4, i32 4, i1 false)
  %4 = call i8* @memcpy(i8* %a, i8* %b, i32 8)
  ret void
}
'''
        output, selected, far = relocate(ir, {'fm1_alnk0_irq', 'isr3'})
        self.assertIn('memcpy', selected)
        self.assertIn('memset', selected)
        self.assertNotIn('memcpy', far)
        self.assertNotIn('memset', far)
        functions = {m[2]: m for m in FUNCTION.finditer(output)}
        ui = functions['ui_copy'][5]
        self.assertNotIn('@llvm.mem', ui)
        self.assertNotIn('@memcpy(', ui)
        self.assertIn('= call i8* @__flash_memcpy(i8* %a, i8* getelementptr inbounds (%struct.s, %struct.s* @gs, i32 0, '
                      'i32 1), i32 80)', ui)
        self.assertNotIn('!tbaa', ui)
        self.assertIn('zext i8 0 to i32', ui)
        self.assertIn('= tail call i8* @__flash_memset(i8* %b, i32 %flash_memset_1_c, i32 64)', ui)
        self.assertIn('%3 = load i8, i8* %a', ui)          # (no unnamed value added: the numbering holds)
        self.assertIn('%4 = call i8* @__flash_memcpy(i8* %a, i8* %b, i32 8)', ui)
        self.assertIn('call void @llvm.memcpy.p0i8.p0i8.i32(i8* %p, i8* %p, i32 16, i32 4, i1 false)',
                      functions['isr3'][5])                 # (SRAM code keeps memcpy in SRAM: near)
        self.assertIn('call i8* @memcpy(i8* %p', functions['isr3'][5])
        self.assertIn('section ".audio_text"', functions['memcpy'][4])
        self.assertIn('define internal i8* @__flash_memcpy(i8* returned %d, i8* nocapture readonly %s, i32 %n) #1 noinline {',
                      output)
        self.assertNotIn('section', functions['__flash_memset'][4])
        self.assertIn('noinline', functions['__flash_memset'][4])

    def test_missing_isr_rejected(self):
        with self.assertRaises(ValueError):relocate('', {'fm1_alnk0_irq'})

    def test_indirect_engine_callbacks(self):
        src=Path(__file__).resolve().parents[1]/'firmware/src'
        roots=engine_roots(src)
        self.assertIn('fm6_key',roots)
        self.assertIn('drum_keys',roots)
        self.assertNotIn('p5_preset_make',roots)

    def test_new_callback_requires_classification(self):
        with tempfile.TemporaryDirectory() as directory:
            src=Path(directory)
            (src/'core.h').write_text('typedef struct { void (*new_hook)(void); } engine_t;')
            with self.assertRaisesRegex(ValueError,'Unclassified engine callbacks'):
                engine_roots(src)

if __name__=='__main__':unittest.main()
