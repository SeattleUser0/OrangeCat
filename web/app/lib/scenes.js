// SPDX-License-Identifier: GPL-3.0-only
// The engines' pictures: the scene bytes of firmware/src/oc_scenes.h (the pack's trailer), by engine name. Not a file of its own any more: they
// come out of lib/fw_tables.js (tests/web_tables.c dumps SCENES[]), so they are the firmware's as built. SHAPES' name is its module's (the
// Nightly builds the engine without it, so the table cannot name it).
import { FW } from "./fw_tables.js";
const NAME = { 8: "MACRO", 9: "DRUM-X", 14: "SHAPES" };
export const SCENES = Object.fromEntries(FW.scenes.map((s) => [NAME[s.engine] ?? "#" + s.engine, s.bytes]));
