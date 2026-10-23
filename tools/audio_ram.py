#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""Place the enabled audio call graph in SRAM after LLVM has finished inlining.

The resident profile places the complete dry audio graph in executable POOL SRAM.
Existing flash-safe and low-SRAM DSP sections keep their placement. Crossing
XIP/SRAM uses a volatile function pointer: pi32v2 direct branches cannot span
that distance. Calling conventions and function signatures stay unchanged.

(Orange Cat 0.5.0) A function placed in a section named .text.xip* stays in flash
(XIP) even when the audio graph reaches it, and so does everything only it calls:
the MACRO engine's models (eng_macro.c) are too big for the SRAM left. The audio
interrupt never runs while the flash is being written (fm1_flash.h: interrupts off),
so code in flash is safe there; it only runs slower when it is not in the cache.

(Orange Cat 0.6.1) memcpy and memset live in SRAM (the audio graph reaches them). A
struct copy or a zeroing in a flash function is an llvm.memcpy / llvm.memset intrinsic
here, which the backend lowers to a direct call of memcpy / memset after this pass:
from far enough into the flash image that call is out of branch range ("relocation
truncated to fit: R_PI32V2_LONG_JUMP_23M2 against symbol memcpy"; 0.6.1's first build,
from seq_tool_ui). So in flash functions those intrinsics become calls through the same
volatile pointers (the byte value of a memset widened to the int memset takes).
"""
import json
import re
import sys
from pathlib import Path

FUNCTION = re.compile(r'^define ([^\n]+?)@([\w.]+)\(([^\n]*)\)([^\n]*)\{\n(.*?)^}', re.M | re.S)
REFERENCE = re.compile(r'@([\w.]+)')
CALL = re.compile(r'\bcall\b[^\n@]*@([\w.]+)\(')
# (0.6.1) LLVM 4's typed pointers (i8*, the alignment an argument) and later LLVMs' ptr; the arguments are split by
# hand (a constant GEP holds commas)
INTRINSIC = re.compile(r'\bcall void @llvm\.(memcpy|memset)\.p0(?:i8)?(?:\.p0(?:i8)?)?\.i32\(')


def intrinsic_args(line, m):
    """the intrinsic call at m: its arguments (split at the top level) and where its ')' ends"""
    args, depth, begin = [], 0, m.end()
    for pos in range(m.end(), len(line)):
        c = line[pos]
        if c in '([{<':
            depth += 1
        elif c in ')]}>':
            if not depth:
                args.append(line[begin:pos].strip())
                return args, pos + 1
            depth -= 1
        elif c == ',' and not depth:
            args.append(line[begin:pos].strip())
            begin = pos + 1
    return None, None


TBAA = re.compile(r', !tbaa(?:\.struct)? ![\w.]+')
ROOTS = {'fm1_alnk0_irq', 'audio_block', 'mix_block', 'ua_audio', 'click_render',
         'p5_worker_kernel', 'fm6_worker_kernel', 'audio_batch_worker', 'audio_batch_parallel', 'p5_batch_kernel', 'fm6_batch_kernel', 'engine_batch_kernel', 'fm1_timer5_irq'}
CALLBACKS = {'note_on', 'render', 'amp', 'block', 'done', 'alloc', 'legato',
             'mono_key', 'post', 'units', 'cap', 'keys', 'desc'}
COLD_CALLBACKS = {'preset_make'}


def engine_roots(src):
    # Indirect engine calls are absent from LLVM's direct call graph. Check
    # the interface too, so a new callback cannot silently stay in flash.
    core = (src / 'core.h').read_text()
    engine = re.search(r'typedef struct\s*\{[^}]*\}\s*engine_t;', core, re.S)
    if not engine:
        raise ValueError('Engine callback interface is missing')
    fields = set(re.findall(r'\(\*(\w+)\)', engine[0]))
    unknown = fields - CALLBACKS - COLD_CALLBACKS
    if unknown:
        raise ValueError(f'Unclassified engine callbacks: {sorted(unknown)}')
    roots = set(ROOTS)
    for file in src.glob('eng_*.c'):
        for field, name in re.findall(r'\.(\w+)\s*=\s*(\w+)\s*[,}]', file.read_text()):
            if field in CALLBACKS:
                roots.add(name)
    return roots


def function_type(match):
    # Optimized definitions have scalar/pointer arguments; reject a new ABI
    # shape rather than silently constructing the wrong indirect call type.
    ret = match[1].strip().split()[-1]
    args = []
    depth = 0
    args_source = []
    begin = 0
    for pos, char in enumerate(match[3] + ','):
        depth += (char == '(') - (char == ')')
        if char == ',' and depth == 0:
            args_source.append(match[3][begin:pos].strip())
            begin = pos + 1
    for arg in filter(None, args_source):
        token = re.match(r'(?:void|ptr|i\d+|float|double|%[\w.]+)\**|\.\.\.', arg)
        if not token:
            raise ValueError(f'Unsupported audio argument type for {match[2]}: {arg}')
        end = token.end()
        rest = arg[end:].lstrip()
        if rest.startswith('('):
            start = len(arg) - len(rest)
            depth = 0
            for pos in range(start, len(arg)):
                depth += (arg[pos] == '(') - (arg[pos] == ')')
                if not depth:
                    end = pos + 1
                    while end < len(arg) and arg[end] == '*':
                        end += 1
                    break
        typ = arg[:end]
        args.append(typ)
    return f'{ret} ({", ".join(args)})'


def xip(section):
    # (Orange Cat 0.5.0) audio code kept in flash on purpose (see the header)
    return section.startswith('.text.xip')


def ram(section):
    # executable SRAM: what the pass places, and the sections that are already there
    return section in ('.audio_text', '.dsp_text', '.ram_text')


def relocate(ir, roots):
    functions = {m[2]: m for m in FUNCTION.finditer(ir)}
    sections = {name: (re.search(r'\bsection "([^"]+)"', m[4]) or [None, ''])[1]
                for name, m in functions.items()}
    selected = set()
    pending = list(roots)
    while pending:
        name = pending.pop()
        # usb_poll is deliberately excluded: TIMER5 calls it only outside
        # the audio interrupt. Nested endpoint/UART/input work remains resident.
        if name == 'usb_poll' or name not in functions or name in selected or xip(sections[name]):
            continue
        selected.add(name)
        pending.extend(REFERENCE.findall(functions[name][5]))
    if 'fm1_alnk0_irq' not in selected:
        raise ValueError('Audio ISR is missing from the optimized module')

    for name in selected:
        if not sections[name]:
            sections[name] = '.audio_text'
        elif sections[name] not in ('.audio_text', '.dsp_text', '.ram_text'):
            raise ValueError(f'Unexpected audio section for {name}: {sections[name]}')
    far = {}

    def rewrite(match):
        name = match[2]
        header = match[0].split('\n', 1)[0]
        if name in selected and not re.search(r'\bsection "', match[4]):
            header = header[:-1].rstrip() + ' section ".audio_text" {'
        body = []
        for line in match[5].splitlines():
            if not ram(sections[name]):             # (0.6.1) a flash function's memcpy / memset: through a pointer
                m = INTRINSIC.search(line)
                args, end = intrinsic_args(line, m) if m else (None, None)
                if args and m[1] in selected and len(args) in (4, 5):
                    typ = function_type(functions[m[1]])
                    far[m[1]] = typ
                    local = f'%audio_far_{len(body)}'
                    body.append(f'  {local} = load volatile {typ}*, {typ}** @__audio_far_{m[1]}, align 4')
                    if m[1] == 'memset':            # (memset's byte as the int it takes)
                        value = args[1].split(None, 1)
                        body.append(f'  {local}_c = zext i8 {value[1]} to i32')
                        args[1] = f'i32 {local}_c'
                    pre = line[:m.start()]                 # (its result named: an unnamed one would take a number)
                    line = TBAA.sub('', pre[:len(pre) - len(pre.lstrip())] + f'{local}_r = ' + pre.lstrip() +
                                    f'call {typ.split()[0]} {local}({args[0]}, {args[1]}, {args[2]})' + line[end:])
                    body.append(line)
                    continue
            call = CALL.search(line)
            if call and call[1] in functions and sections[name] != sections[call[1]]:
                # SRAM sections can call each other directly; all fit well
                # inside branch range. Only a flash/RAM crossing needs this.
                if ram(sections[name]) != ram(sections[call[1]]):
                    target = call[1]
                    typ = function_type(functions[target])
                    far[target] = typ
                    local = f'%audio_far_{len(body)}'
                    body.append(f'  {local} = load volatile {typ}*, {typ}** @__audio_far_{target}, align 4')
                    line = line[:call.start(1)-1] + local + line[call.end(1):]
            body.append(line)
        return header + '\n' + '\n'.join(body) + '\n}'

    result = FUNCTION.sub(rewrite, ir)
    globals_ir = '\n'.join(f'@__audio_far_{name} = internal global {typ}* @{name}, align 4'
                           for name, typ in sorted(far.items()))
    result += '\n' + globals_ir + '\n'
    return result, sorted(selected), sorted(far)


def main(source, destination):
    src = Path(__file__).resolve().parents[1] / 'firmware/src'
    roots = engine_roots(src)
    result, selected, far = relocate(Path(source).read_text(), roots)
    Path(destination).write_text(result)
    Path(destination).with_suffix('.json').write_text(json.dumps({'resident': selected, 'far_calls': far}, indent=2))
    print(f'audio RAM: {len(selected)} reachable functions; {len(far)} flash/RAM call boundaries')


if __name__ == '__main__':
    main(*sys.argv[1:3])
