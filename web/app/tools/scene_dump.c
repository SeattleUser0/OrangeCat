/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat web app: the engine pictures' scene bytes (Session C's tests/ui_scenes.h, branch ui-design) as JSON, for
 * web/app/lib/scenes.js. The scenes are data (UI_DESIGN.md, "The scene format"); this prints the bytes the C arrays hold, so the
 * page draws the unit's own pictures and not copies of them.
 *   cc -I DIR_WITH_ui_scenes.h -o scene_dump web/app/tools/scene_dump.c && ./scene_dump > scenes.json
 *   node web/app/tools/gen_scenes.mjs scenes.json COMMIT          (writes web/app/lib/scenes.js)
 * The interpreter in ui_scene.h draws on gfx.c's canvas; it is not used here, so its canvas is stubbed.
 * Revisions:
 *   0.14.2: first version (Session D). */
#include <stdint.h>
#include <stdio.h>
static uint16_t cv_px[1];
static uint32_t cv_w, cv_h;
#define CV_OY 0
static uint16_t swap16(uint16_t v) { return v; }
static uint16_t mix565(uint16_t a, uint16_t b, uint32_t t) { (void)b; (void)t; return a; }
static void cv_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c) { (void)x; (void)y; (void)w; (void)h; (void)c; }
static void cv_frame(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c) { (void)x; (void)y; (void)w; (void)h; (void)c; }
static void cv_disc(int32_t x, int32_t y, int32_t d, uint16_t c, int ring) { (void)x; (void)y; (void)d; (void)c; (void)ring; }
#include "ui_scenes.h"

static void dump(const char *name, const uint8_t *s, size_t n, int last)
{
    printf("  \"%s\": [", name);
    for (size_t i = 0; i < n; i++) printf("%s%u", i ? "," : "", s[i]);
    printf("]%s\n", last ? "" : ",");
}
int main(void)
{
    printf("{\n");
    dump("MACRO", SCENE_MACRO, sizeof SCENE_MACRO, 0);
    dump("SHAPES", SCENE_SHAPES, sizeof SCENE_SHAPES, 0);
    dump("DRUM-X", SCENE_DRUMX, sizeof SCENE_DRUMX, 0);
    dump("WHEEL", SCENE_WHEEL, sizeof SCENE_WHEEL, 1);
    printf("}\n");
    (void)scene_draw; (void)cv_px; (void)cv_w; (void)cv_h;
    return 0;
}
