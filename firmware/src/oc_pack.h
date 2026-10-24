/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat 0.8.0: the asset pack, the compile-time side (the run-time side is oc_pack.c).
 *
 * What it is
 *   The lookup tables of SHAPES (Braids, braids/pb_tables.h) and MACRO (Plaits, plaits/pl_tables.h): about 39.7 KB of
 *   constants, the largest block of the image. They live in flash outside the image: tools/gen_pack.py writes them as
 *   build/orangecat-<version>.pak (a 512-byte header, then the tables) and the firmware reads them IN PLACE through the
 *   plaintext XIP window (fl_plain_window_init) at 0x93000 + their offset, where the pack is installed (the 10 sectors of
 *   storage.c's PACK area). Reading is what it was: a load through the XIP cache.
 *
 * Two builds of the same source
 *   OC_PACK_XIP 0 (the host tests, and a build without the pack): the tables are ordinary static const arrays, as they
 *   were. OC_PACK_XIP 1 (the device): each table's name is a macro for a pointer into the pack, made by gen_pack.py in
 *   build/gen/oc_pack_tables.h; the array that the header still defines beside it is unused and the compiler drops it.
 *   Tables are declared with OC_TBL(type, name)[n] = {...}, which is that switch. Every use of a table is an index, a
 *   pointer or a sizeof of nothing (the only ones are PB_x_SIZE / PL_x_SIZE, constants), so both builds read alike.
 *
 * When the pack is missing
 *   oc_pack.c checks it at boot (header, table hash, CRC-32). Not there, or not this firmware's: oc_pack_good stays 0,
 *   and SHAPES and MACRO render nothing (OC_PACK_READY, tested at the top of their note-on and render) and the screen
 *   says PACK MISSING; every setting, preset and project stays as it is. The device boots and everything else plays.
 *
 * In a loadable module (0.10.0, step 1: OC_MODULE defined, the sources in tools/module)
 *   There is no oc_pack_good of the module's own (it would be a byte of .data, and a module has none): OC_PACK_READY reads the
 *   firmware's flag through the services table (OC_FW(), which the module's source defines before any engine code runs).
 *
 * Revisions:
 * (Orange Cat 0.10.0) fw_api_t at a fixed address, no RAM in the module (0.10.0)
 *   0.8.0: first version.
 *   0.10.0: OC_MODULE: the ready flag is read through fw_api_t.
 */
#pragma once
#include <stdint.h>

#ifndef OC_PACK_XIP
#define OC_PACK_XIP 0
#endif
#ifndef OC_PACK_GATE                   /* the engines ask whether the pack is there: always with the device's tables */
#define OC_PACK_GATE OC_PACK_XIP
#endif

#define OC_PACK_MAGIC 0x4B50434Fu      /* "OCPK" */
#define OC_PACK_VERSION 1u
#define OC_PACK_HDR 512u               /* the header: magic, version, flags, table hash, length, payload CRC-32, entries, header CRC-32 */
#define OC_PACK_LO 0x93000u            /* where it is installed: storage.c's PACK area (ST_PACK_LO) ... */
#define OC_PACK_AREA (10u * 0x1000u)   /* ... and its 10 sectors (ST_PACK_SECTORS) */

#if OC_PACK_XIP
#include "oc_pack_tables.h"            /* build/gen: OC_PACK_HASH, OC_PACK_LEN, one macro per table */
#define OC_TBL(type, name) static const type __attribute__((unused)) OC_UNUSED_##name
#else
#define OC_TBL(type, name) static const type name
#ifndef OC_PACK_HASH
#define OC_PACK_HASH 0x0C0FFEE1u       /* (the host tests' stand-in: a pack for the in-memory tables is a made one) */
#endif
#ifndef OC_PACK_LEN
#define OC_PACK_LEN 4096u
#endif
#endif

#ifdef OC_MODULE
#define OC_PACK_READY() (*OC_FW()->pack_ready)         /* (a module is built for the device: the gate is on) */
#else
/* 1: the pack was checked and is there. Without the gate there is nothing to wait for. */
static uint8_t oc_pack_good = OC_PACK_GATE ? 0u : 1u;
#define OC_PACK_READY() (!OC_PACK_GATE || oc_pack_good)
#endif
