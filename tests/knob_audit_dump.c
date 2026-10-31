/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* The preset dump for tests/knob_audit.py (docs/orangecat/UI_DESIGN.md, "The four knobs per engine").
 * Prints, as JSON, every engine named on the command line: its name, its eight EDIT controls (label, min, max,
 * default), today's HOME knobs and every preset's stored values (e[8], env[4], fenv, mono), read through the
 * firmware's own engine table (tests/hostsim.c includes engines.c), so the audit sees exactly what a preset load sets.
 *   cc -O2 -w -Ibuild/gen -Ifirmware/src -Itests -o knob_audit_dump tests/knob_audit_dump.c -lm
 *   ./knob_audit_dump 8 14 9 > presets.json          (MACRO, SHAPES, DRUM-X)
 *   the same built with -DMELODEE_LEGACY_EXTRAS=1:  ./knob_audit_dump 7 > wheel.json   (WHEEL is built in only there)
 * (-O2: at -O0 the host compiler emits the HAL's unused device inlines and fails.)
 * Revisions:
 *   (Orange Cat 0.15 design, branch ui-design) first version. */
#include <stdio.h>
#include <stdint.h>
#define main hostsim_main
#include "hostsim.c"
#undef main
int main(int argc, char **argv)
{
    int first = 1;
    printf("{");
    for (int a = 1; a < argc; a++) {
        int e = atoi(argv[a]);
        const engine_t *en = e >= 0 && e < (int)NENGINES ? ENGINES[e] : 0;
        if (!en || !en->npresets)
            continue;
        printf("%s\"%d\":{\"name\":\"%s\",\"edit\":[", first ? "" : ",", e, en->name);
        first = 0;
        for (int k = 0; k < 8; k++)
            printf("%s[\"%s\",%d,%d,%d]", k ? "," : "", en->edit[k].label, en->edit[k].min, en->edit[k].max,
                   en->edit[k].def);
        printf("],\"knob\":[%d,%d,%d,%d],\"presets\":[", en->knob[0], en->knob[1], en->knob[2], en->knob[3]);
        for (int i = 0; i < en->npresets; i++) {
            const preset_t *p = eng_preset(en, (uint32_t)i);
            printf("%s{\"name\":\"%s\",\"e\":[%d,%d,%d,%d,%d,%d,%d,%d],\"env\":[%d,%d,%d,%d],\"fenv\":%d,\"mono\":%d}",
                   i ? "," : "", p->name, p->e[0], p->e[1], p->e[2], p->e[3], p->e[4], p->e[5], p->e[6], p->e[7],
                   p->env[0], p->env[1], p->env[2], p->env[3], p->fenv, p->mono);
        }
        printf("]}");
    }
    printf("}\n");
    return 0;
}
