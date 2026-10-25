#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""(Orange Cat 0.13.7, branch linux-hw) SysEx between a pipe and the FM-1's MIDI port, for tools/hw/oc_install.mjs (node has no MIDI).

stdin: one SysEx frame a line, hex (F0 .. F7), sent as is; stdout: each SysEx frame the unit sends, one a line, hex. A line READY first.
The port is the first whose name matches the FM-1 (as tools/hw/fm1hw.py finds it).
"""
import re
import sys
import threading

import mido

PORT_RE = re.compile(r"orange cat|melodee|felucca|fm-1", re.I)


def main():
    names = [n for n in mido.get_input_names() if PORT_RE.search(n)]
    if not names:
        print("ERROR no FM-1 MIDI port", flush=True)
        return 3
    out = mido.open_output(names[0])

    def rx(msg):
        if msg.type == "sysex":
            sys.stdout.write(bytes([0xF0, *msg.data, 0xF7]).hex() + "\n")
            sys.stdout.flush()

    inp = mido.open_input(names[0], callback=rx)
    print("READY " + names[0], flush=True)
    for line in sys.stdin:
        b = bytes.fromhex(line.strip())
        if len(b) >= 2 and b[0] == 0xF0 and b[-1] == 0xF7:
            out.send(mido.Message("sysex", data=b[1:-1]))
    inp.close()
    out.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
