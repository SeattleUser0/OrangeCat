/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat 0.10.0 (module PoC step 5): the module loader. A loadable engine (the sources in tools/module, fw_api.h) is found in the
 * MODULE area of the flash map at boot, checked, and registered in the engine table (engines.c eng_register).
 *
 * Where a module is
 *   The module area is 12 sectors, 0xC8000..0xD3FFF. SHAPES' module is 6 sectors, so it has two places it can be linked for, "starts" 0 and 6
 *   (sector index in the area; tools/build.py --module makes one image for each and the .ocm carries both). The installer writes a
 *   replacement BESIDE the old module when another start is free (0.10.1; before that the code said in place while this comment said beside):
 *   the new module is written there, its header page last, read back from the flash and checked as the loader checks it, and only then is
 *   the old module taken out of the table and its start erased. A power cut at any point before the new header page is programmed leaves
 *   the old module; after it, the new one (or, in the instant between the commit and the first erase of the old start, both: the lower
 *   start is used, and both are good). Only when no other start is free is a replacement written IN PLACE: the old module is erased first
 *   and a power cut leaves none (the web page and the card browser say so before they start). The loader looks at both starts.
 *   Flash offset 0xC8000 reads at XIP 0x020C4000 (FM1_XIP: the loader header takes the first 0x4000): that is where start 0 runs.
 *
 * The checks (in this order; the first that fails names the state of the start)
 *   MD_EMPTY    the first word is not FW_MOD_MAGIC (erased, or never written): nothing there, no message
 *   MD_TORN     the header's CRC-32 (hcrc) is wrong: an install cut off, or flash damage; the installer writes the header page LAST, so a
 *               torn install normally leaves no magic at all, and this is the rarer case
 *   MD_REFUSED  fw_module_check said no (another interface identifier, another table address, a table the firmware does not have): the
 *               module was built for another firmware. `why` has fw_module_check's code
 *   MD_BAD_HDR  a field is out of range: link_addr is not where this start runs, length is not a multiple of 4 or does not fit the start,
 *               engine_ptr is outside the code, the engine number is not one a module may supply, state_size is 0 or too big, or the
 *               asset pack the tables come from is not this build's (pack_hash)
 *   MD_BAD_CRC  the CRC-32 of the code (bytes 0x100..length) differs from the header's: a damaged or partly written module
 *   MD_BAD_TAIL (0.10.1) the CRC is right but a sector of the module ends in 80 bytes the update loader would take for one of its update
 *               records (below): an OTA update would erase that sector and with it the module. `why` is the sector, counted from 1. Not
 *               registered. tools/build.py --module moves the code on until no sector does, so this is a module built by something else,
 *               or damage that happened to land there
 *   MD_CRASHED  the last boot died in its first 30 s with this module in use (below): skipped until a power cycle or until it is
 *               installed or removed again
 *   MD_NOSLOT   the engine number is a built-in one and OC_MODULE_WINS is 0 (eng_register: EM_SHADOWED), or two valid modules claim the same number
 *   MD_OK       registered
 * The update loader's record rule (0.10.1)
 *   loader/ldr_core.c erases any data sector between 0x93000 and 0xFC000 whose last 256 bytes, read as 80 bytes, look like one of its update
 *   records: bytes 6..7 are 0x5441 and the first word is not 0 and is the CRC-16 of bytes 2..79. The pack has had the check since 0.8.0
 *   (pk_tail); a module has it now, in three places that apply one rule (oc_pack.c's pk_record_like): tools/build.py --module (moves the
 *   code on 4 bytes at a time until no sector tail of the image matches; no package otherwise), oc_mod_end (reads the tails of the code it
 *   was given, from the flash, before it programs the header: a module that fails is refused, rc 8, MI_TAIL, and never registered) and
 *   oc_mod_check (every sector the module covers, MD_BAD_TAIL). The sectors a module covers are those that start inside its length; the
 *   bytes after its end are erased flash.
 *
 * Two valid modules for one engine number: (0.11.1) the one with the higher header version is registered; on equal versions the one at the higher
 * start (a replacement is written to the free start). The other is erased at that boot (not while lg_incomplete, and an erase error leaves it for the
 * next boot), and until then it is MD_NOSLOT. (The installer never leaves two; a crash between "write the new" and "erase the old" does.)
 * Equal versions are the weak case: a replacement of the SAME version written to start 0 over an old module at start 1, cut before the old erase,
 * leaves the old one in charge at the next boot.
 *
 * The crash mark
 *   A module that crashes the firmware must not keep it from starting. main.c's boot guard (bootguard.failed) counts a boot that died in its first
 *   30 s. The mark is a second word in .noinit: `inuse`, a bit per start, set (oc_mod_note_use, called from the main loop in those 30 s) when a
 *   track plays the engine a registered module supplies. At the next boot: bootguard.failed != 0 and the bit is set -> the module is marked
 *   banned (MD_CRASHED) and not registered; the boot goes on without it. Tracks that used it fall to the engine table's answer for the number
 *   (the built-in SHAPES when it is built in, else the retired slot, as for any engine that is not there). `banned` stays until power is lost (the noinit
 *   RAM is gone) or the start is installed or removed (oc_mod_unban). Neither the bits nor the bans are written to flash.
 *   Beyond the first 30 s (0.10.1): the boot guard does not see a death later than that, so a module that crashes reliably after 30 s would
 *   reboot the unit for ever. oc_modguard also keeps `run` (1 from the first boot until an orderly reset: reset_intended, i.e. UBOOT, an update; a
 *   power loss clears the RAM) and, per start, `unclean`. The reset reason does not tell an orderly reset from a crash on this chip (a fault ends in
 *   fm1_reboot, which is also what a soft reset is), so the firmware's own mark is used: at a boot with `run` still 1 the last session died (the
 *   watchdog, a CPU exception, a hang); each start whose module's engine a track held in it (`inuse`, now set all session, not only in the first 30 s)
 *   has `unclean` + 1, any other start (or a clean end) is back to 0. Three in a row bans the module as above and the screen says
 *   MODULE BANNED SHAPES once (oc_mod_ban_notice). The 30 second rule bans at the first death, as before.
 *
 * The PC guard (hal/fm1_guard.h)
 *   Instruction fetch is allowed in [0x02000120, _etext] and the SRAM code window. A module runs from the module area, beyond _etext. When one is
 *   registered the loader sets fm1_guard_code_end to the last byte of the highest module, before main.c enables the guard; with none the
 *   window is what it was. The window then also holds the flash between (the pack, the library); the hardware has one such window (two PC limit
 *   pairs exist, LIMIT1 is the SRAM code window), so that is the cost of a module: a wild jump into the pack or the library is not caught while one is
 *   registered. (0.10.1) oc_mod_boot rewrites LIMIT0_H at its end (fm1_guard_pc_window), so an install or remove in a running session moves the window at
 *   once; 0.10.0 left the old one until the next reset. (0.11.1) ONE helper (oc_mod_window) writes it from what is registered, on every path that
 *   registers or unregisters a module: oc_mod_boot, oc_mod_park, and the erase-error path of an in-place begin.
 *
 * Host tests: tests/oc_module_test.c (a simulated NOR: every refusal, the crash mark, a good module, two modules).
 *
 * Revisions:
 * (Orange Cat 0.13.7) a module's hot set (engine_t hot) is checked in flash (oc_mod_hot_check: inside its code or the pack's tables, MD_BAD_HDR else) and handed to
 *   eng_register_hot; a set that does not fit the arena is NO ROOM (MD_NOSLOT, why 4)
 * (Orange Cat 0.13.5) the geometry table can be made writable by a host test (OC_MOD_GEO_QUAL); nothing else changed.
 * (Orange Cat 0.13.4) oc_mod_image_check / oc_mod_hdr_matches / oc_mod_pages_check_rd: the checks of an image take a reader, so the card checks the file it is about to write
 * with the code oc_mod_end and the boot check use; oc_mod_tails is oc_pack.c's oc_rule_tails over the flash.
 * (Orange Cat 0.13.4, Hardening 1) oc_mod_end runs the page descriptor check before the commit (it was only in the boot check after it).
 * (Orange Cat 0.10.1) module sector tails against the update loader's record rule
 * (Orange Cat 0.11.0) ED_ENGINES (125) and the Engines table in the web page; module header gains version; lint fix for the host window constant
 * (Orange Cat 0.11.1) every path that takes a module out of the table writes the PC window
 * (Orange Cat 0.12.0) a module's pages as data (fw_pages.h), the Prophet's number is a module slot
 * (Orange Cat 0.12.1) the Prophet as a module at 8 sectors, three module starts, a module of another engine in the way is refused and named
 * (Orange Cat 0.12.2) fuzzers for the library group, the pack and the module loader; the loser of two modules no longer erases the winner's sectors; module link overflow...
 * (Orange Cat 0.14.0) WHEEL (engine 7) as a module: 4 sectors (2,920 B built), starts 0 and 1 (sectors 0 and 6): beside SHAPES at 0, beside the Prophet at 4
 *   0.10.0: first version.
 *   0.10.1: the crash count beyond the first 30 s (oc_mod_power_on, oc_mod_ban_notice); the update loader's record rule on the sector tails (MD_BAD_TAIL, MI_TAIL, oc_mod_tails). A replacement is written beside the old
 *           module when another start is free (oc_mod_plan, oc_mod_inplace, mw.inplace); the header comment now says what the code does.
 *   0.11.1: two valid modules of one engine at boot (a cut between the commit of a replacement and the erase of the old one): the higher header version
 *           wins, on equal versions the higher start; the loser is erased at that boot (oc_mod_boot). Before it the lower start won and the next install went in place.
 *           oc_mod_window, the one helper for the PC guard's window (an in-place begin that failed to erase, and an install abandoned without its end, left it wide).
 *           oc_mod_note_engine: the project load marks the module's use as it gives a track the engine (a death before the main loop's first pass is attributed).
 *           oc_mod_reload: every oc_mod_boot from a running session (end of an install, remove) first takes the engine out of the table and parks its voices
 *           (oc_mod_park_voices); the table's two stores are one critical section there (oc_mod_reg, oc_mod_unreg).
 *   0.12.1: three starts (sector 0, 6 and 4; oc_mod_geo): SHAPES takes 6 sectors (starts 0, 1), the Prophet 8 (sectors 0 and 4). A candidate whose span shares a sector with a
 *           usable module of ANOTHER engine is blocked, never erased (oc_mod_plan, oc_mod_blocked_by; MI_NOFREE); the same engine's module is replaced beside or in place as before.
 */
#ifndef FM1_GUARD_CODE_END_DEFINED                             /* hal/fm1_guard.h declares it; a host build has no guard header */
static uint32_t fm1_guard_code_end;
#include "../hal/fm1_guard_pc.h"
#define OC_HOST_ETEXT 34078720u                                 /* a host build's _etext (any value above the image: the lint keeps hex addresses in hal/) */
static uint32_t host_pc_hi, host_pc_sets;                       /* what the device's LIMIT0_H would hold, and how often it was written */
static void fm1_guard_pc_window(void) { host_pc_hi = fm1_guard_pc_hi(fm1_guard_code_end, OC_HOST_ETEXT); host_pc_sets++; }
#endif
/* FL_XIP(off): the XIP address of flash offset off (hal/fm1_flash.h on the device; tests/hostsim.c for a host build) */
#define OC_MOD_STARTS 3u                                       /* (0.12.1) the places a module can start: sector index 0, 6 and 4 of the area (start numbers 0, 1, 2) */
#define OC_MOD_AREA 12u                                        /* sectors in the area */
static const uint8_t oc_mod_sect[OC_MOD_STARTS] = { 0u, 6u, 4u };
#define OC_MOD_START_SECTOR(i) ((uint32_t)oc_mod_sect[i])
#define OC_MOD_MAXLEN (8u * 0x1000u)                           /* the most any module may be (the Prophet's 8 sectors); oc_mod_geo has each engine's own */
/* (0.12.1) What a module of each engine looks like: how many sectors its image may take and which starts it may be linked for (a bit a start number).
 * SHAPES: 6 sectors, starts 0 and 1 (sectors 0 and 6). The Prophet: 8 sectors, starts 0 and 2 (sectors 0 and 4). Spans of different engines overlap, so
 * no SHAPES module can stand beside a Prophet module in a 12 sector area (the planner refuses; see oc_mod_plan). */
typedef struct { uint8_t engine, sectors, starts; } oc_mod_geo_t;
#ifndef OC_MOD_GEO_QUAL
#define OC_MOD_GEO_QUAL const                           /* (0.13.5) a host test defines it empty to change the geometries for one case (tests/card_install_test.c) */
#endif
static OC_MOD_GEO_QUAL oc_mod_geo_t oc_mod_geo[] = { { ENGI_SHAPES, 6u, 0x03u }, { ENGI_PROPHET, 8u, 0x05u },
                                                     { ENGI_WHEEL, 4u, 0x03u },     /* (0.14.0) WHEEL: 4 sectors at sector 0 or 6: beside SHAPES (0) or the Prophet (4) */
#if OC_CANDIDATES
                                                     { ENGI_SEM, 4u, 0x03u }, { ENGI_HADES, 4u, 0x03u },   /* (0.14.0) the candidates of a measurement build */
                                                     { ENGI_FMBELL, 4u, 0x03u }, { ENGI_SUPERSAW, 4u, 0x03u }, { ENGI_DDDD, 4u, 0x03u },    /* (0.14.1) op1r's synths: WHEEL's geometry */
#endif
                                                   };
static OC_MOD_GEO_QUAL oc_mod_geo_t *oc_mod_geo_of(uint32_t engine)
{
    uint32_t k;
    for (k = 0; k < sizeof oc_mod_geo / sizeof oc_mod_geo[0]; k++)
        if (oc_mod_geo[k].engine == engine)
            return &oc_mod_geo[k];
    return 0;
}
static int oc_mod_allowed(uint32_t engine, uint32_t i)      /* may a module of this engine start at start i */
{
    const oc_mod_geo_t *g = oc_mod_geo_of(engine);
    return g && i < OC_MOD_STARTS && ((g->starts >> i) & 1u) && OC_MOD_START_SECTOR(i) + g->sectors <= OC_MOD_AREA;
}
static int oc_mod_fits(uint32_t engine, uint32_t i, uint32_t len)
{
    return oc_mod_allowed(engine, i) && len <= (uint32_t)oc_mod_geo_of(engine)->sectors * 0x1000u;
}
static uint32_t oc_mod_nsec(uint32_t len) { return (len + 0xFFFu) / 0x1000u; }
/* do the sectors [sect(i), +ni) and [sect(j), +nj) share a sector */
static int oc_mod_overlap(uint32_t i, uint32_t ni, uint32_t j, uint32_t nj)
{
    return OC_MOD_START_SECTOR(i) < OC_MOD_START_SECTOR(j) + nj && OC_MOD_START_SECTOR(j) < OC_MOD_START_SECTOR(i) + ni;
}
#define OC_MOD_STATE_MAX 16384u                                /* a sanity limit on state_size (bytes of arena per part) */
enum { MD_EMPTY, MD_TORN, MD_REFUSED, MD_BAD_HDR, MD_BAD_CRC, MD_CRASHED, MD_NOSLOT, MD_OK,
       MD_BAD_TAIL };                                          /* oc_mod.state[i]; MD_BAD_TAIL (0.10.1) is last so the numbers the web page knows stay */

static struct {
    uint8_t state[OC_MOD_STARTS];                              /* MD_* */
    uint8_t why[OC_MOD_STARTS];                                /* MD_REFUSED: fw_module_check's code; MD_NOSLOT: eng_register's, negated */
    uint32_t engine[OC_MOD_STARTS];                            /* the engine number the header names (valid once the header is) */
    uint32_t length[OC_MOD_STARTS];
    uint32_t version[OC_MOD_STARTS];                           /* (0.11.0) the header's version word (valid with the engine) */
    uint32_t hot[OC_MOD_STARTS];                               /* (0.13.7) the address of its engine_t's hot set, checked (oc_mod_hot_check), 0 none */
    uint8_t nhot[OC_MOD_STARTS];                               /* (0.13.7) and its length */
} oc_mod;
/* the crash mark (core.h declares oc_modguard so that reset_intended can clear its count): survives a reset, not a power loss */
#define OC_MOD_UNCLEAN_MAX 3u                                  /* (0.10.1) sessions in a row that died with the module's engine in use: banned */

/* the XIP address a start runs at; the header there (device) or in the test's copy of the flash */
static uint32_t oc_mod_flash(uint32_t i) { return ST_MODULE_LO + OC_MOD_START_SECTOR(i) * 0x1000u; }
static uint32_t oc_mod_link(uint32_t i) { return FL_XIP(oc_mod_flash(i)); }
#ifndef OC_MOD_HOST_XIP
static const fw_mod_hdr_t *oc_mod_hdr_ptr(uint32_t i) { return (const fw_mod_hdr_t *)oc_mod_link(i); }   /* registered by address: the header is read in place */
#endif

static void oc_mod_guard_init(void)
{
    if (oc_modguard.magic != OC_MODGUARD_MAGIC) {
        oc_modguard.magic = OC_MODGUARD_MAGIC;
        oc_modguard.inuse = 0;
        oc_modguard.banned = 0;
        oc_modguard.run = 0;
        oc_modguard.unclean[0] = oc_modguard.unclean[1] = oc_modguard.unclean[2] = 0;
        oc_modguard.notice = 0;
    }
}
/* an install or a removal of start i: the ban and the use mark for it are gone */
static void oc_mod_unban(uint32_t i)
{
    oc_mod_guard_init();
    oc_modguard.banned &= ~(1u << i);
    oc_modguard.inuse &= ~(1u << i);
    oc_modguard.unclean[i] = 0;
    oc_modguard.notice &= (uint8_t)~(1u << i);
}
/* (0.10.1) the update loader's rule on the sectors a module of `len` bytes at start i covers: 0 ok; else the sector, counted from 1, whose last
 * 256 bytes would be taken for an update record; -1 when the flash would not read */
static int oc_mod_tails(uint32_t i, uint32_t len)
{
    uint32_t base = oc_mod_flash(i);                            /* (0.13.4) the rule is oc_pack.c's oc_rule_tails: one copy for the flash and for the card's file */
    return oc_rule_tails(oc_rd_flash, &base, (len + 0xFFFu) / 0x1000u * 0x1000u, (len + 0xFFFu) / 0x1000u);
}
/* (0.12.0) the module's page descriptor, if its engine_t has one (engine_t.pages), checked on the flash bytes (fw_pages_check): the UI follows pointers
 * in it, so one that leaves the image refuses the module. The engine_t is read at the device layout (4 byte pointers); the host builds (8 byte pointers,
 * fake link addresses) skip this and test fw_pages_check on hand-built bytes (tests/mod_pages_test.c). 0 = fine or none. */
/* (0.13.4) ctx: {a reader, its context, the module's link address}: an address of the module's own space is read from the image at the offset from its link address */
typedef struct { oc_rd_t rd; void *ctx; uint32_t link; } oc_mod_rdctx_t;
static int oc_mod_rd(void *ctx, uint32_t addr, void *buf, uint32_t n)
{
    const oc_mod_rdctx_t *b = ctx;
    return b->rd(b->ctx, addr - b->link, buf, n);
}
static int oc_mod_pages_check_rd(oc_rd_t rd, void *rctx, uint32_t link, const fw_mod_hdr_t *h)
{
    oc_mod_rdctx_t c;
    uint32_t p;
    uint8_t w[4];
    if (sizeof(void *) != 4u)
        return 0;
    c.rd = rd; c.ctx = rctx; c.link = link;
    if (h->engine_ptr + offsetof(engine_t, pages) + 4u > link + h->length ||
        oc_mod_rd(&c, h->engine_ptr + (uint32_t)offsetof(engine_t, pages), w, 4u))
        return FWP_ADDR;
    p = fwp_u32(w);
    if (!p)
        return 0;
    return fw_pages_check(oc_mod_rd, &c, p, link + FW_MOD_CODE_OFF, link + h->length);
}
/* (0.13.7) the module's hot set (core.h engine_t hot, nhot), read from the flash through rd: none, or at most ENG_HOT_MAX entries inside the code, word
 * aligned, each a table of 1 .. 16 KB inside the module's own code or the asset pack's tables. 0 good (*hot, *nhot set), else FWP_ADDR. Device builds only, as the
 * page descriptor's check (a host build's engine_t has other offsets): there a module has no hot set */
#define OC_MOD_HOT_MAX_BYTES 16384u
static int oc_mod_hot_check_rd(oc_rd_t rd, void *rctx, uint32_t link, const fw_mod_hdr_t *h, uint32_t *hot, uint8_t *nhot)
{
    oc_mod_rdctx_t c;
    uint8_t w[4], n;
    uint32_t lo = link + FW_MOD_CODE_OFF, hi = link + h->length, plo = FL_XIP(OC_PACK_LO) + OC_PACK_HDR, phi = FL_XIP(OC_PACK_LO) + OC_PACK_LEN;
    *hot = 0; *nhot = 0;
    if (sizeof(void *) != 4u)
        return 0;
    c.rd = rd; c.ctx = rctx; c.link = link;
    if (h->engine_ptr + sizeof(engine_t) > hi ||
        oc_mod_rd(&c, h->engine_ptr + (uint32_t)offsetof(engine_t, nhot), &n, 1u) ||
        oc_mod_rd(&c, h->engine_ptr + (uint32_t)offsetof(engine_t, hot), w, 4u))
        return FWP_ADDR;
    if (!n)
        return 0;
    uint32_t p = fwp_u32(w);
    if (n > ENG_HOT_MAX || (p & 3u) || p < lo || p > hi || n * (uint32_t)sizeof(eng_hot_t) > hi - p)
        return FWP_ADDR;
    for (uint32_t k = 0; k < n; k++) {
        uint8_t e[8];
        if (oc_mod_rd(&c, p + k * (uint32_t)sizeof(eng_hot_t), e, 8u))
            return FWP_ADDR;
        uint32_t src = fwp_u32(e), bytes = fwp_u32(e + 4);
        if (!bytes || bytes > OC_MOD_HOT_MAX_BYTES ||
            !((src >= lo && src <= hi && bytes <= hi - src) || (src >= plo && src <= phi && bytes <= phi - src)))
            return FWP_ADDR;
    }
    *hot = p; *nhot = n;
    return 0;
}
static int oc_mod_hot_check(uint32_t i, const fw_mod_hdr_t *h)
{
    uint32_t base = oc_mod_flash(i);
    return oc_mod_hot_check_rd(oc_rd_flash, &base, oc_mod_link(i), h, &oc_mod.hot[i], &oc_mod.nhot[i]);
}
static int oc_mod_pages_check(uint32_t i, const fw_mod_hdr_t *h)
{
    uint32_t base = oc_mod_flash(i);
    return oc_mod_pages_check_rd(oc_rd_flash, &base, oc_mod_link(i), h);
}
/* the header at start i read from the flash (st_read), its checks, then the code's CRC and the sector tails: the state */
static uint32_t oc_mod_check(uint32_t i, fw_mod_hdr_t *h)
{
    uint8_t buf[256];
    uint32_t crc = 0xFFFFFFFFu, off, len, link = oc_mod_link(i);
    int rc;
    memset(h, 0, sizeof *h);
    if (st_read(oc_mod_flash(i), h, sizeof *h))
        return MD_EMPTY;                                        /* a flash that will not read: nothing there for this boot */
    if (h->magic != FW_MOD_MAGIC)
        return MD_EMPTY;
    if (ocs_crc32((const uint8_t *)h, FW_MOD_HDR_BYTES - 4u) != h->hcrc)
        return MD_TORN;
    if ((rc = fw_module_check(h)) != FW_MOD_OK) {
        oc_mod.why[i] = (uint8_t)rc;
        return MD_REFUSED;
    }
    len = h->length;
    if (h->link_addr != link || len <= FW_MOD_CODE_OFF || (len & 3u) ||
        h->engine_ptr < link + FW_MOD_CODE_OFF || h->engine_ptr >= link + len || (h->engine_ptr & 3u) ||
        !eng_module_slot(h->engine) || !oc_mod_fits(h->engine, i, len) || h->state_size == 0 || h->state_size > OC_MOD_STATE_MAX ||
        h->pack_hash != OC_PACK_HASH)
        return MD_BAD_HDR;
    for (off = FW_MOD_CODE_OFF; off < len; off += sizeof buf) {
        uint32_t n = len - off > sizeof buf ? sizeof buf : len - off;
        if (st_read(oc_mod_flash(i) + off, buf, n))
            return MD_BAD_CRC;
        crc = ocs_crc_add(crc, buf, n);
        SMP_FEED();
    }
    if (~crc != h->crc)
        return MD_BAD_CRC;
    if (oc_mod_pages_check(i, h))
        return MD_BAD_HDR;                                      /* (0.12.0) a page descriptor that leaves the image */
    if (oc_mod_hot_check(i, h))
        return MD_BAD_HDR;                                      /* (0.13.7) a hot set that leaves the image and the pack */
    rc = oc_mod_tails(i, len);
    if (rc < 0)
        return MD_BAD_CRC;
    if (rc > 0) {
        oc_mod.why[i] = (uint8_t)rc;
        return MD_BAD_TAIL;
    }
    return MD_OK;
}

/* (0.11.1) The two stores of the engine table (ENGINES[e], eng_mod[e]) are ONE critical section in a running session (oc_mod_live, set by oc_mod_reload and
 * oc_mod_park's callers: the installer), so that CPU1 and the audio ISR never see the table and the header disagree. At power-on (persist_boot, before audio)
 * the interrupt state is left alone. */
static uint8_t oc_mod_live;
static int oc_mod_reg(uint32_t i, const fw_mod_hdr_t *h, const engine_t *engine)
{
    int rc;
    if (oc_mod_live) fm1_irq_off();
    rc = eng_register_hot(h, engine, (const eng_hot_t *)(uintptr_t)oc_mod.hot[i], oc_mod.nhot[i]);   /* (0.13.7) the hot set as oc_mod_check found it */
    if (oc_mod_live) fm1_irq_on();
    return rc;
}
static void oc_mod_unreg(uint32_t e)
{
    if (oc_mod_live) fm1_irq_off();
    eng_unregister(e);
    if (oc_mod_live) fm1_irq_on();
}
/* (0.11.1) The PC guard's window follows what is REGISTERED: the last byte of the highest registered module, 0 with none (then the window is _etext).
 * ONE helper for every path that registers or unregisters a module (oc_mod_boot, oc_mod_park, the error paths of oc_mod_begin), so that no path leaves
 * a window wider than the table. A start counts when its state is MD_OK and the engine it names is registered (a parked module keeps its state until
 * its start is erased). */
static void oc_mod_window(void)
{
    uint32_t i, end = 0;
    for (i = 0; i < OC_MOD_STARTS; i++) {
        uint32_t e = oc_mod.engine[i], last;
        if (oc_mod.state[i] != MD_OK || e >= NENGINES || !eng_mod[e])
            continue;
        last = oc_mod_link(i) + oc_mod.length[i] - 1u;
        if (end < last)
            end = last;
    }
    fm1_guard_code_end = end;
    fm1_guard_pc_window();
}

/* persist_boot, after oc_pack_boot: look at every start, register what is good. `crashed`: the last boot died in its first 30 s
 * (bootguard.failed). Safe to call again (the installer does, after an install or a removal): the registry is rebuilt. */
static int oc_mod_erase_start(uint32_t i);
static int oc_mod_erase_loser(uint32_t i);
static void oc_mod_boot(uint32_t crashed)
{
    uint32_t i, e, loser = 0;
    oc_mod_guard_init();
    for (e = 0; e < NENGINES; e++)
        if (eng_mod[e])
            oc_mod_unreg(e);
    memset(&oc_mod, 0, sizeof oc_mod);
    if (crashed) {
        oc_modguard.notice |= (uint8_t)(oc_modguard.inuse & ~oc_modguard.banned);
        oc_modguard.banned |= oc_modguard.inuse;                /* the module(s) in use when it died */
    }
    oc_modguard.inuse = 0;
    for (i = 0; i < OC_MOD_STARTS; i++) {                       /* what every start holds */
        fw_mod_hdr_t h;
        uint32_t st = oc_mod_check(i, &h);
        oc_mod.state[i] = (uint8_t)st;
        if (st == MD_EMPTY)
            continue;
        oc_mod.engine[i] = h.engine;
        oc_mod.length[i] = h.length;
        oc_mod.version[i] = h.version;
        if (st == MD_OK && (oc_modguard.banned & (1u << i)))
            oc_mod.state[i] = MD_CRASHED;
    }
    for (i = 0; i < OC_MOD_STARTS; i++) {
        uint32_t j;
        fw_mod_hdr_t h;
        if (oc_mod.state[i] != MD_OK)
            continue;
        /* (0.11.1) two valid modules of one engine (a cut between the commit of a replacement and the erase of the old one): the higher header
         * version wins; on equal versions the higher start (a replacement is written to the free start). The loser is not registered and is erased below. */
        for (j = 0; j < OC_MOD_STARTS; j++)
            if (j != i && oc_mod.state[j] == MD_OK && oc_mod.engine[j] == oc_mod.engine[i] &&
                (oc_mod.version[j] > oc_mod.version[i] || (oc_mod.version[j] == oc_mod.version[i] && j > i)))
                break;
        if (j < OC_MOD_STARTS) {
            oc_mod.state[i] = MD_NOSLOT;
            oc_mod.why[i] = (uint8_t)-EM_SLOT;
            loser |= 1u << i;
            continue;
        }
        (void)oc_mod_check(i, &h);
        {   int rc = eng_mod[h.engine] ? EM_SLOT : oc_mod_reg(i, oc_mod_hdr_ptr(i), (const engine_t *)h.engine_ptr);
            if (rc != EM_OK) {
                oc_mod.state[i] = MD_NOSLOT;
                oc_mod.why[i] = (uint8_t)-rc;
                continue;
            }
        }
    }
    oc_mod_window();                                            /* (0.10.1) a running session too: the window follows the registered modules (0.11.1: the one helper) */
    /* (0.11.1) the loser of two valid modules is erased now, lazily: only when its rival is the registered one and no old preset pair may still lie in
     * the area (lg_incomplete); an erase that fails leaves it for the next boot (it stays NOSLOT, so the next install still finds one usable module here) */
    if (loser && !lg_incomplete) {
        for (i = 0; i < OC_MOD_STARTS; i++)
            if ((loser & (1u << i)) && eng_mod[oc_mod.engine[i]])
                (void)oc_mod_erase_loser(i);
    }
}
/* (0.10.1) The boot: once at power-on and after every reset (project.c persist_boot), the session's accounting first, then oc_mod_boot.
 * `run` says whether the last session ended in an orderly reset (reset_intended clears it). If it did not, the session died (the watchdog, a CPU
 * exception, a hang: nothing else writes through this path), and every start whose module's engine was in use in it (the `inuse` bits, kept
 * in .noinit) has one more unclean session in a row; one that was not in use, or a clean end, starts its count again. OC_MOD_UNCLEAN_MAX in a
 * row bans the module as the 30 second rule does (until power is lost, or the start is installed or removed) and the screen says so once. The
 * 30 second rule (bootguard.failed, `crashed`) is unchanged and bans at the first death. */
static void oc_mod_power_on(uint32_t crashed)
{
    uint32_t i, dead;
    oc_mod_guard_init();
    dead = oc_modguard.run;
    oc_modguard.run = 1;
    for (i = 0; i < OC_MOD_STARTS; i++) {
        if (dead && (oc_modguard.inuse & (1u << i))) {
            if (oc_modguard.unclean[i] < 255u)
                oc_modguard.unclean[i]++;
        } else {
            oc_modguard.unclean[i] = 0;
        }
        if (oc_modguard.unclean[i] >= OC_MOD_UNCLEAN_MAX) {
            if (!(oc_modguard.banned & (1u << i)))
                oc_modguard.notice |= (uint8_t)(1u << i);
            oc_modguard.banned |= 1u << i;
            oc_modguard.unclean[i] = 0;
        }
    }
    oc_mod_boot(crashed);
}
/* "MODULE BANNED SHAPES", once, for a module banned at this boot (main.c after the project is loaded) */
static void oc_mod_ban_notice(void)
{
    uint32_t i;
    for (i = 0; i < OC_MOD_STARTS; i++) {
        if (!(oc_modguard.notice & (1u << i)))
            continue;
        oc_modguard.notice &= (uint8_t)~(1u << i);
        ui_say("MODULE BANNED ", oc_mod.engine[i] == ENGI_SHAPES ? "SHAPES" : oc_mod.engine[i] == ENGI_PROPHET ? "PROPHET" : "ENGINE");
        return;
    }
}
/* (0.11.1) set the use bit of the registered module that supplies `engine`. The project load calls it for every track as it gives the track its engine
 * (project_restore_runtime), so a death before the main loop's first pass (oc_mod_note_use) is still attributed to the module. */
static void oc_mod_note_engine(uint32_t engine)
{
    uint32_t i;
    for (i = 0; i < OC_MOD_STARTS; i++)
        if (oc_mod.state[i] == MD_OK && oc_mod.engine[i] == engine && engine < NENGINES && eng_mod[engine] == oc_mod_hdr_ptr(i))
            oc_modguard.inuse |= 1u << i;
}
/* main loop, all session (0.10.1; the first 30 s before): set the use bit of every registered module a track is playing */
static void oc_mod_note_use(void)
{
    uint32_t i, t;
    for (i = 0; i < OC_MOD_STARTS; i++) {
        if (oc_mod.state[i] != MD_OK)
            continue;
        for (t = 0; t < NTRK; t++)
            if (trk[t].engine == oc_mod.engine[i])
                oc_modguard.inuse |= 1u << i;
    }
}

/* ------------------------------------------------------------------------- installing (Orange Cat 0.10.0, step 6) ---
 * The web editor's ED_MOD_* (editor.c) or a .ocm on the card (card_browse.c) puts a module in the area; only while the transport is stopped, as
 * every flash write. The page reads ED_MOD_INFO for the engine it has, which names the start to use (oc_mod_plan), takes that variant's image
 * from the .ocm, and sends it:
 *   oc_mod_begin(engine, start, len)   the start must be oc_mod_plan's. Beside (the start is free): its 6 sectors are erased and nothing else is
 *                                      touched; the old module, if any, goes on playing and stays registered. In place (no other start is free and
 *                                      this one holds the engine's old module): the engine is taken out of the table and its tracks' voices and
 *                                      state are cleared (oc_mod_park), then the 6 sectors of the start are erased (and those of any other module
 *                                      for the same engine); from here the old module is gone: a cut leaves none (the built-in engine, if built
 *                                      in, or silence; the project keeps its engine numbers).
 *   oc_mod_write(off, data, n)         the code in order from FW_MOD_CODE_OFF, in 256 byte pages
 *   oc_mod_end(page)                   the 256 byte header page, checked against what was written (its CRC against the stream, the link address
 *                                      against the start, every field as at boot), programmed LAST (the commit). Then the area is read again as at
 *                                      boot and the module registered. Beside, the new start is first read back and checked as the loader checks
 *                                      it; if it fails it is erased and the old module stays (rc 7). If it passes, the old module is taken out of
 *                                      the table (oc_mod_park), its start erased (the header sector first), and the new one registered.
 *   oc_mod_remove(engine)              the engine taken out as above and its start(s) erased (engine 127: the whole area)
 * Where a module goes (oc_mod_plan, 0.10.1): the first empty start; else the first that holds something this firmware cannot use (another
 * firmware's module, damage, a cut-off install, a sector tail the update loader would erase); else, with no start free, the start that holds a
 * module for the same engine (an install in place: the old module is erased first); else none (rc 5). A start is "free" when it holds no usable
 * module, whatever it holds otherwise. oc_mod_inplace(engine, start) says whether an install at that start is in place. */
static struct {
    uint8_t writing;
    uint8_t start;
    uint8_t inplace;                                     /* (0.10.1) this install replaced the engine's old module where it stood */
    uint32_t engine, len, done, crc;
} mw;
enum { MI_OK = 0, MI_REFUSED = -1, MI_FLASH = -2, MI_PAIRS = -4, MI_NOFREE = -5, MI_BAD = -7, MI_TAIL = -8 };   /* MI_TAIL (0.10.1): a sector tail the update loader would erase */

static int oc_mod_has(uint32_t i, uint32_t engine)      /* start i holds a usable module of this engine, registered or not */
{
    return (oc_mod.state[i] == MD_OK || oc_mod.state[i] == MD_CRASHED || oc_mod.state[i] == MD_NOSLOT) && oc_mod.engine[i] == engine;
}
/* start i holds a module this firmware can run (usable), whatever its state is for the engine */
static int oc_mod_usable(uint32_t i)
{
    return oc_mod.state[i] == MD_OK || oc_mod.state[i] == MD_CRASHED || oc_mod.state[i] == MD_NOSLOT;
}
/* (0.12.1) The planner. A module of `engine` goes to one of the starts its geometry allows (oc_mod_geo). A candidate is
 *   BLOCKED when its span (the engine's sectors from the start) shares a sector with a usable module of ANOTHER engine: that module is never erased by an
 *            install; the user removes it first (oc_mod_blocker names it; the editor, the installer page and the card browser say
 *            "ENGINE X INSTALLED AT START N: REMOVE IT FIRST"). A module that is not usable (damaged, torn, for another firmware, a bad tail) does not block;
 *   OWN-OVERLAPPING when its span shares a sector with a usable module of the SAME engine: the install would have to erase that module first (in place);
 *   else it is BESIDE: written where nothing usable stands, the old module going on playing until the new one reads back good.
 * The choice: the first beside candidate with an empty start; else the first beside candidate that holds junk; else (in place) the candidate that holds
 * the engine's own module, else the first not blocked; else none. Examples in a 12 sector area: SHAPES at 0 -> a SHAPES replacement goes to 6 (beside);
 * a Prophet at 0 or 4 -> a Prophet replacement is always in place (8 sectors of 12 leave no room beside); SHAPES and a Prophet never fit together. */
/* (0.13.3) can modules of these two engines stand in the area together: some allowed start of each leaves their spans disjoint. SHAPES (6 sectors) and the Prophet
 * (8) never can in 12; the card browser's "Install set" asks before it writes anything */
static int oc_mod_pair_fits(uint32_t a, uint32_t b)
{
    OC_MOD_GEO_QUAL oc_mod_geo_t *ga = oc_mod_geo_of(a), *gb = oc_mod_geo_of(b);
    uint32_t i, j;
    if (!ga || !gb) return 0;
    for (i = 0; i < OC_MOD_STARTS; i++)
        for (j = 0; j < OC_MOD_STARTS; j++)
            if (oc_mod_allowed(a, i) && oc_mod_allowed(b, j) && !oc_mod_overlap(i, ga->sectors, j, gb->sectors)) return 1;
    return 0;
}
/* the sectors a usable module at start j holds for itself: its engine's geometry (a SHAPES module of one sector still keeps the six: a replacement of it
 * may take them), not just the sectors its length reaches */
static uint32_t oc_mod_reach(uint32_t j)
{
    const oc_mod_geo_t *g = oc_mod_geo_of(oc_mod.engine[j]);
    uint32_t n = g ? g->sectors : oc_mod_nsec(oc_mod.length[j]);
    return n ? n : 1u;
}
static int oc_mod_blocker(uint32_t engine, uint32_t c)                       /* the start that blocks an install at c, or -1 */
{
    uint32_t j, ns = oc_mod_geo_of(engine)->sectors;
    for (j = 0; j < OC_MOD_STARTS; j++)
        if (oc_mod_usable(j) && oc_mod.engine[j] != engine && oc_mod_overlap(c, ns, j, oc_mod_reach(j)))
            return (int)j;
    return -1;
}
static int oc_mod_own_overlap(uint32_t engine, uint32_t c)                  /* an install at c shares a sector with a usable module of this engine */
{
    uint32_t j, ns = oc_mod_geo_of(engine)->sectors;
    for (j = 0; j < OC_MOD_STARTS; j++)
        if (oc_mod_has(j, engine) && oc_mod_overlap(c, ns, j, oc_mod_reach(j)))
            return 1;
    return 0;
}
/* the start that stands in the way of this engine, or -1: only when no start can take it (oc_mod_plan < 0) and a usable module of another engine is why */
static int oc_mod_blocked_by(uint32_t engine)
{
    uint32_t c;
    int b = -1;
    if (!eng_module_slot(engine) || !oc_mod_geo_of(engine))
        return -1;
    for (c = 0; c < OC_MOD_STARTS; c++) {
        if (!oc_mod_allowed(engine, c))
            continue;
        if (oc_mod_blocker(engine, c) < 0)
            return -1;                                  /* a candidate is not blocked: nothing stands in the way */
        if (b < 0)
            b = oc_mod_blocker(engine, c);
    }
    return b;
}
/* the start an install of `engine` goes to, or -1 (0.10.1: a free start first, so a replacement is written beside the old module) */
static int oc_mod_plan(uint32_t engine)
{
    uint32_t c;
    if (!eng_module_slot(engine) || !oc_mod_geo_of(engine))
        return -1;
    for (c = 0; c < OC_MOD_STARTS; c++)
        if (oc_mod_allowed(engine, c) && oc_mod_blocker(engine, c) < 0 && !oc_mod_own_overlap(engine, c) && oc_mod.state[c] == MD_EMPTY) return (int)c;
    for (c = 0; c < OC_MOD_STARTS; c++)
        if (oc_mod_allowed(engine, c) && oc_mod_blocker(engine, c) < 0 && !oc_mod_own_overlap(engine, c) && !oc_mod_usable(c)) return (int)c;
    for (c = 0; c < OC_MOD_STARTS; c++)
        if (oc_mod_allowed(engine, c) && oc_mod_has(c, engine)) return (int)c;          /* in place, where the old module stands */
    for (c = 0; c < OC_MOD_STARTS; c++)
        if (oc_mod_allowed(engine, c) && oc_mod_blocker(engine, c) < 0) return (int)c;  /* in place, over a module that overlaps the span */
    return -1;
}
/* an install of `engine` at `start` replaces its old module where it stands (the old one is erased first; a cut leaves none) */
static int oc_mod_inplace(uint32_t engine, uint32_t start)
{
    return start < OC_MOD_STARTS && oc_mod_geo_of(engine) && oc_mod_own_overlap(engine, start);
}
/* the engine out of the table; the voices and the arena state of the tracks that play it cleared: nothing calls into the module after this */
static void oc_mod_park_voices(uint32_t engine)
{
    uint32_t t, i;
    for (t = 0; t < NTRK; t++) {
        if (trk[t].engine != engine) continue;
        for (i = 0; i < NVOICE; i++) {
            voice_t *v = &trk[t].v[i];
            v->active = v->gate = 0;
            v->stage = 0;
            v->env = v->env_out = 0;
        }
        eng_state_clear(t);
    }
}
static void oc_mod_park(uint32_t engine)
{
    oc_mod_live = 1;                                             /* (only the installer parks: a running session) */
    if (engine < NENGINES && eng_mod[engine])
        oc_mod_unreg(engine);
    oc_mod_live = 0;
    oc_mod_park_voices(engine);
    oc_mod_window();                                             /* (0.11.1) the engine is out of the table: the window follows at once, not at the next boot */
}
/* (0.11.1) oc_mod_boot from a running session (an install, a remove): every engine a module may supply is taken out of the table and its voices and arena
 * state cleared FIRST, then the area is read again (milliseconds of SPI). Without it a track on the engine renders the built-in slot meanwhile and
 * then the module, under live voices. The window is written last (oc_mod_boot's end), so it is never narrower than the code that may still run. */
static void oc_mod_reload(void)
{
    uint32_t e;
    oc_mod_live = 1;
    for (e = 0; e < NENGINES; e++) {
        if (!eng_module_slot(e)) continue;
        if (eng_mod[e])
            oc_mod_unreg(e);
        oc_mod_park_voices(e);
    }
    oc_mod_boot(0);
    oc_mod_live = 0;
}
/* sectors of the area erased in order from `first` (the header sector first), the watchdog fed between. 0 or the flash's error */
static int oc_mod_erase_span(uint32_t first, uint32_t n)
{
    uint32_t s;
    for (s = 0; s < n && first + s < OC_MOD_AREA; s++) {
        int rc;
        SMP_FEED();
        if ((rc = st_erase(ST_MODULE_LO + (first + s) * 0x1000u)) != 0) return rc;
    }
    return 0;
}
/* (0.12.1) start i taken out: a usable module's sectors (all those its engine's geometry gives it, as the six before: stale code left behind is not kept
 * around), the header sector only for anything else (its length is not to be trusted, and the sectors after it may belong to a module that stands in
 * the same span). A start without a magic is empty whatever the rest holds. */
static int oc_mod_erase_start(uint32_t i)
{
    uint32_t n = oc_mod_usable(i) && oc_mod_geo_of(oc_mod.engine[i]) ? oc_mod_geo_of(oc_mod.engine[i])->sectors : 1u;
    int rc;
    if (n > OC_MOD_AREA - OC_MOD_START_SECTOR(i)) n = OC_MOD_AREA - OC_MOD_START_SECTOR(i);
    if ((rc = oc_mod_erase_span(OC_MOD_START_SECTOR(i), n)) != 0) return rc;
    oc_mod_unban(i);
    oc_mod.state[i] = MD_EMPTY;
    return 0;
}
/* (0.12.2, found by tests/fuzz_oc_module.c) the loser of two valid modules of one engine, erased lazily at boot: its sectors EXCEPT those a registered module
 * stands in (two Prophet starts, sector 0 and sector 4, share the sectors 4 to 7: the erase of the loser at start 0 took the winner's header sector with it).
 * The loser's own header sector goes first (from that erase it is no module); a sector that belongs to the winner is not touched. */
static int oc_mod_erase_loser(uint32_t i)
{
    uint32_t n = oc_mod_geo_of(oc_mod.engine[i]) ? oc_mod_geo_of(oc_mod.engine[i])->sectors : 1u, q, j;
    int rc;
    for (q = 0; q < n && OC_MOD_START_SECTOR(i) + q < OC_MOD_AREA; q++) {
        uint32_t sec = OC_MOD_START_SECTOR(i) + q;
        int keep = 0;
        for (j = 0; j < OC_MOD_STARTS; j++) {
            const oc_mod_geo_t *g = oc_mod.state[j] == MD_OK && j != i ? oc_mod_geo_of(oc_mod.engine[j]) : 0;
            if (g && sec >= OC_MOD_START_SECTOR(j) && sec < OC_MOD_START_SECTOR(j) + g->sectors)
                keep = 1;
        }
        if (keep)
            continue;
        if ((rc = oc_mod_erase_span(sec, 1u)) != 0)
            return rc;
    }
    oc_mod_unban(i);
    oc_mod.state[i] = MD_EMPTY;
    return 0;
}
/* 0 ok; -1 refused (not the planned start, a bad length, an engine a module may not supply); -4 the move of the old preset pairs has not finished
 * (an old pair may still be in the area); -5 no start free; else flash */
static int oc_mod_begin(uint32_t engine, uint32_t start, uint32_t len)
{
    int plan, rc;
    uint32_t i;
    mw.writing = 0;
    if (!eng_module_slot(engine) || start >= OC_MOD_STARTS || len <= FW_MOD_CODE_OFF || (len & 3u) || !oc_mod_fits(engine, start, len))
        return MI_REFUSED;
    if (lg_incomplete)
        return MI_PAIRS;
    plan = oc_mod_plan(engine);
    if (plan < 0)
        return MI_NOFREE;                                /* (0.12.1) a usable module of another engine is in the way: oc_mod_blocked_by names it (ED_MOD_INFO) */
    if ((uint32_t)plan != start)
        return MI_REFUSED;
    mw.inplace = (uint8_t)oc_mod_inplace(engine, start);
    if (mw.inplace) {                                    /* in place: the old module out first, then every usable module of this engine */
        oc_mod_park(engine);
        for (i = 0; i < OC_MOD_STARTS; i++)
            if (i != start && oc_mod_has(i, engine))        /* (the old module at the start asked for is erased with the new span below, header sector first) */
                if ((rc = oc_mod_erase_start(i)) != 0) { oc_mod_window(); return rc; }   /* (0.11.1: this set the end to 0 and never wrote the window) */
    }
    if ((rc = oc_mod_erase_span(OC_MOD_START_SECTOR(start), oc_mod_geo_of(engine)->sectors)) != 0) {  /* (0.12.1) the new module's own sectors; beside, the old module stays registered and playable */
        if (mw.inplace) oc_mod_window();
        return rc;
    }
    oc_mod_unban(start);
    oc_mod.state[start] = MD_EMPTY;
    mw.writing = 1; mw.start = (uint8_t)start; mw.engine = engine; mw.len = len;
    mw.done = FW_MOD_CODE_OFF; mw.crc = 0xFFFFFFFFu;
    return MI_OK;
}
/* the code in order: n bytes at off (where the last write ended; the first at FW_MOD_CODE_OFF). 0 ok; -1 refused; else flash */
static int oc_mod_write(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *p = src;
    if (!mw.writing || off != mw.done || n > mw.len - mw.done)
        return MI_REFUSED;
    while (n) {
        uint32_t k = 256u - (off & 0xFFu);
        int rc;
        if (k > n) k = n;
        if ((rc = st_prog(oc_mod_flash(mw.start) + off, p, k)) != 0) { mw.writing = 0; return rc; }
        mw.crc = ocs_crc_add(mw.crc, p, k);
        p += k; off += k; n -= k;
        mw.done = off;
    }
    return MI_OK;
}
/* (0.13.4) the header page of an image (the 256 bytes that are programmed last) against what was written: the engine, the start it is for, its length (the CRC-32
 * of the code is compared by the callers). One copy for oc_mod_end (the code as it was programmed) and for the card's check of a file before anything is erased. 1 ok. */
static int oc_mod_hdr_matches(const uint8_t *pg, uint32_t engine, uint32_t start, uint32_t len)
{
    const fw_mod_hdr_t *h = (const fw_mod_hdr_t *)pg;
    return h->magic == FW_MOD_MAGIC && ocs_crc32(pg, FW_MOD_HDR_BYTES - 4u) == h->hcrc && fw_module_check(h) == FW_MOD_OK &&
           h->length == len && h->engine == engine && h->link_addr == oc_mod_link(start) &&
           h->engine_ptr >= h->link_addr + FW_MOD_CODE_OFF && h->engine_ptr < h->link_addr + h->length && !(h->engine_ptr & 3u) &&
           h->state_size != 0 && h->state_size <= OC_MOD_STATE_MAX && h->pack_hash == OC_PACK_HASH;
}
enum { MC_OK = 0, MC_HDR = -1, MC_CRC = -2, MC_PAGES = -3, MC_TAIL = -8, MC_READ = -9 };   /* oc_mod_image_check (MI_TAIL's -8 is the same verdict) */
/* (0.13.4) a whole module image (the header page, then the code to `len`) as rd gives it, for `engine` at `start`, checked as oc_mod_end and the boot check will:
 * the placement and length, the header page against the image (link address of the start, length, engine, state size, the pack hash, hcrc, fw_module_check),
 * the code's CRC-32, the page descriptor (device builds), the sector tails. 0 ok, else MC_*. Writes nothing. */
static int oc_mod_image_check(oc_rd_t rd, void *ctx, uint32_t engine, uint32_t start, uint32_t len)
{
    _Alignas(4) uint8_t pg[256];
    uint8_t buf[256];
    uint32_t crc = 0xFFFFFFFFu, off;
    int t;
    if (!eng_module_slot(engine) || start >= OC_MOD_STARTS || len <= FW_MOD_CODE_OFF || (len & 3u) || !oc_mod_fits(engine, start, len))
        return MC_HDR;
    if (rd(ctx, 0, pg, sizeof pg))
        return MC_READ;
    for (off = FW_MOD_CODE_OFF; off < len; off += sizeof buf) {
        uint32_t n = len - off > sizeof buf ? sizeof buf : len - off;
        if (rd(ctx, off, buf, n))
            return MC_READ;
        crc = ocs_crc_add(crc, buf, n);
        SMP_FEED();
    }
    if (!oc_mod_hdr_matches(pg, engine, start, len))
        return MC_HDR;
    if (((const fw_mod_hdr_t *)pg)->crc != ~crc)
        return MC_CRC;
    if (oc_mod_pages_check_rd(rd, ctx, oc_mod_link(start), (const fw_mod_hdr_t *)pg))
        return MC_PAGES;
    t = oc_rule_tails(rd, ctx, len, (len + 0xFFFu) / 0x1000u);
    return t < 0 ? MC_READ : t > 0 ? MC_TAIL : 0;
}
/* the header page (256 bytes) when all the code is in: 0 ok; -1 refused (the header does not match what was written, the start or the firmware);
 * -7 programmed but the area does not read as a module; -8 a sector tail would be taken for an update record (0.10.1; no header is programmed); else flash */
static int oc_mod_end(const void *page256)
{
    uint8_t pg[256];
    const fw_mod_hdr_t *h = (const fw_mod_hdr_t *)pg;
    int rc;
    memcpy(pg, page256, sizeof pg);
    if (!mw.writing || mw.done != mw.len)
        { mw.writing = 0; return MI_REFUSED; }
    mw.writing = 0;
    if (!oc_mod_hdr_matches(pg, mw.engine, mw.start, mw.len) || h->crc != ~mw.crc)
        return MI_REFUSED;
    if (oc_mod_tails(mw.start, mw.len) != 0)                               /* (0.10.1) the code as the flash has it: no sector tail like an update record */
        return MI_TAIL;
    if (oc_mod_pages_check(mw.start, h))                                   /* (0.13.4) the page descriptor (device builds) on the code as written, BEFORE the commit: a module the boot check would refuse gets no header, so it is never half-registered and a beside install has nothing to erase afterwards */
        return MI_REFUSED;
    if (oc_mod_hot_check(mw.start, h))                                     /* (0.13.7) and its hot set, for the same reason */
        return MI_REFUSED;
    if ((rc = st_prog(oc_mod_flash(mw.start), pg, sizeof pg)) != 0)       /* the commit, last */
        return rc;
    if (!mw.inplace) {                                                     /* beside (0.10.1): the old module goes only after the new one reads back good */
        fw_mod_hdr_t nh;
        uint32_t i, st = oc_mod_check(mw.start, &nh), old = 0;
        if (st != MD_OK && st != MD_NOSLOT) {
            (void)oc_mod_erase_span(OC_MOD_START_SECTOR(mw.start), oc_mod_geo_of(mw.engine)->sectors);   /* the new one is junk: out of the way, the old module untouched */
            oc_mod_reload();
            return MI_BAD;
        }
        for (i = 0; i < OC_MOD_STARTS; i++)
            if (i != mw.start && oc_mod_has(i, mw.engine)) old |= 1u << i;
        if (old) {
            oc_mod_park(mw.engine);                                        /* nothing calls into the old module from here */
            for (i = 0; i < OC_MOD_STARTS && !rc; i++)
                if (old & (1u << i)) rc = oc_mod_erase_start(i);           /* its header sector first: from the first erase the new module is the only valid one */
        }
        oc_mod_reload();                                                    /* registered as at boot: the new one */
        if (rc) return rc;                                                 /* the new module is in; the old one could not be erased (both valid: the next boot keeps the newer and erases the other) */
        return oc_mod.state[mw.start] == MD_OK ? MI_OK : MI_BAD;
    }
    oc_mod_reload();                                                        /* read as at boot: the module is registered when it is good */
    return oc_mod.state[mw.start] == MD_OK || oc_mod.state[mw.start] == MD_NOSLOT ? MI_OK : MI_BAD;
}
/* the module(s) of this engine out and their sectors erased (engine 127: both starts whatever they hold). 0 ok; -4; else flash */
static int oc_mod_remove(uint32_t engine)
{
    uint32_t i;
    int rc = 0;
    if (lg_incomplete)
        return MI_PAIRS;
    mw.writing = 0;
    if (engine != 127u && !eng_module_slot(engine))
        return MI_REFUSED;
    if (engine == 127u) { for (i = 0; i < NENGINES; i++) if (eng_mod[i]) oc_mod_park(i); }
    else oc_mod_park(engine);
    if (engine == 127u) {                                /* (0.12.1) the whole area, sector by sector: whatever the starts hold or do not */
        rc = oc_mod_erase_span(0u, OC_MOD_AREA);
        for (i = 0; i < OC_MOD_STARTS; i++) { oc_mod_unban(i); oc_mod.state[i] = MD_EMPTY; }
    } else {
        for (i = 0; i < OC_MOD_STARTS && !rc; i++)
            if (oc_mod_has(i, engine))
                rc = oc_mod_erase_start(i);
    }
    oc_mod_reload();
    return rc;
}
