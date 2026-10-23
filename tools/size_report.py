#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Orange Cat: what the firmware image is made of, by engine and area.
"""Size report of a built firmware ELF, grouped by engine and area.

  tools/size_report.py OBJDUMP ELF [OUT.txt]

OBJDUMP is the JieLi toolchain's objdump ($JIELI_TOOLCHAIN/common/bin/objdump). The app is one translation unit
(melodee.c includes the rest), so symbols carry no file name: each symbol is put in a group by its name (the
GROUPS patterns below, first match wins). Flash is the .text output section (code and constants run in place from
flash) plus the flash copies of .data, .ram_text, .dsp_text and .dsp_tables; RAM is .data, .bss and .pool.
The groups are estimates: shared helpers an engine uses are counted where their names put them.

Used for Orange Cat build doc phase 3 (strip Melodee's engines): the cost of each engine before it is removed.

Revisions:
  0.1.3 (Orange Cat 0.5.1): groups for the MACRO (Plaits port) and SHAPES (Braids port) engines.
  0.1.2: SRAM-resident code (.audio_text and the rest copied at boot) counted in flash and RAM; 909 (d9_) and
         drum sample kit (dsk) names in the DRUM group; all RAM sections listed.
  0.1.1: bytes per section and the largest ungrouped symbols in the report (to refine GROUPS); every sized
         symbol in a RAM section counts as RAM.
  0.1.0: first version.
"""
import re
import subprocess
import sys
from collections import defaultdict

GROUPS = [   # (group, name pattern), first match wins; case-insensitive
    ("MACRO engine (Plaits port)", r"^pl_|^pla_|^plb_|^plc_|^pld_|^mc_|ENG_MACRO|^MC_|N_MC_"),          # (Orange Cat 0.5.0)
    ("SHAPES engine (Braids port)", r"^pb_|^pba_|^pbb_|^pbc_|^pbd_|^pbm_|^sh_render|^sh_note_on|^sh_units|^sh_part"
     r"|ENG_SHAPES|^SH_|N_SH_"),                                                                        # (Orange Cat 0.5.1)
    ("PROPHET engine", r"prophet|^p5_|_p5_|^P5_|p5edit|prophet_"),
    ("CZ-1 engine", r"^cz_|_cz_|^CZ_|cz1|^upd933|ENG_CZ|eng_cz"),
    ("SID engine", r"^sid_|_sid_|^SID_|resid|ENG_SID|eng_sid|drum_sid"),
    ("FM6 engine", r"fm6|^dx7|dexed|msfa|^FM6|ENG_FM6|fm4_"),
    ("DRUM engine (808, 909)", r"drum|^x0x|^d909|^d808|909|808|ENG_DRUM|eng_drum|kit|^d9_|^d8_|^dsk|^DSK"),
    ("Other engines (PHASE, VOICE, TRIO, WHEEL, PHYS, NOISE, SLICE)",
     r"^phase_|eng_phase|formant|klatt|^trio|eng_trio|wheel|^phys|eng_phys|^noise_|eng_noise|^slc_|slice"),
    ("Effects", r"^fx_|_fx_|fx\b|reverb|delay|chorus|phaser|^tape|echo|^rev_|^dly_|insert"),
    ("Sequencer and recording", r"^seq_|seq\b|^rec_|record|^note_|pattern|^pat_|song|chain|motion|swing|arp"),
    ("Voices, mixing and audio core", r"voice|^mix_|^audio|^dsp_|^eng_|engine|resource|batch|worker|core1|cpu1"),
    ("Storage and projects", r"^st_|storage|^proj|project|^fl_|flash|upreset|preset|bank|backup|persist|settings|favor"),
    ("USB, MIDI and editor link", r"^usb|^ua_|^midi|^ed_|editor|sysex|^cdc|^con_|console|ota|uart"),
    ("Screen, fonts and UI", r"^ui_|^cv_|^gfx|^draw|^lcd|^menu|^AF_|^AP_|font|icon|keycap|palette|^scr_|glyph|page|^panel|^T_|theme"),
]


def main(objdump, elf, out=None):
    syms = subprocess.run([objdump, "-t", elf], capture_output=True, text=True, check=True).stdout
    # objdump -t line: ADDR FLAGS(7 chars) SECTION SIZE NAME; split from the right so odd flag columns still parse
    line = re.compile(r"^([0-9a-f]{8}) (.{7}) (\S+)\s+([0-9a-f]{8})\s+(\S+)\s*$")
    # (0.1.2) code and tables copied from flash to SRAM at boot (.audio_text, .dsp_text, .dsp_tables, .ram_text,
    # .data) count in both columns: they take flash for the copy and SRAM while running
    loaded = {".data", ".ram_text", ".dsp_text", ".dsp_tables", ".audio_text"}
    flash = {".text"} | loaded
    ram = {".bss", ".pool", ".ramx", ".sbss", ".sdata", ".noinit", ".cpu1_stacks", ".audio_scratch"} | loaded
    by = defaultdict(lambda: [0, 0])                    # group -> [flash bytes, RAM bytes]
    big = []
    sections = defaultdict(int)                         # (0.1.1) bytes per output section, all sized symbols
    pats = [(g, re.compile(p, re.I)) for g, p in GROUPS]
    for ln in syms.splitlines():
        m = line.match(ln)
        if not m:
            continue
        _, flags, sec, size, name = m.groups()
        size = int(size, 16)
        sections[sec] += size
        if not size or flags[5] in "df":                # (0.1.1) skip debug and file symbols only
            continue
        g = next((g for g, p in pats if p.search(name)), "Other")
        if sec in flash:
            by[g][0] += size
        if sec in ram:
            by[g][1] += size
        big.append((size, sec, name, g))
    rows = sorted(by.items(), key=lambda kv: -kv[1][0])
    text = ["Orange Cat size report (estimate, by symbol name)", "",
            f"{'group':<62} {'flash B':>9} {'RAM B':>9}"]
    for g, (f, r) in rows:
        text.append(f"{g:<62} {f:>9} {r:>9}")
    text.append(f"{'total':<62} {sum(v[0] for v in by.values()):>9} {sum(v[1] for v in by.values()):>9}")
    text += ["", "bytes per section (all sized symbols):"]
    for sec, n in sorted(sections.items(), key=lambda kv: -kv[1]):
        text.append(f"  {sec:<14} {n:>9}")
    text += ["", "largest 30 symbols not in a group (Other):"]
    for size, sec, name, g in [b for b in sorted(big, reverse=True) if b[3] == "Other"][:30]:
        text.append(f"  {size:>8} {sec:<11} {name[:60]}")
    text += ["", "largest 40 symbols:"]
    for size, sec, name, g in sorted(big, reverse=True)[:40]:
        text.append(f"  {size:>8} {sec:<11} {name[:48]:<48} {g}")
    report = "\n".join(text) + "\n"
    if out:
        open(out, "w").write(report)
    sys.stdout.write(report)


if __name__ == "__main__":
    if len(sys.argv) not in (3, 4):
        sys.exit(__doc__)
    main(*sys.argv[1:])
