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
 * Two valid modules for one engine number: the one at the lower start is registered, the other is MD_NOSLOT. (The installer never leaves
 * two; a crash between "write the new" and "erase the old" does, and either is a good SHAPES.)
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
 *   once; 0.10.0 left the old one until the next reset.
 *
 * Host tests: tests/oc_module_test.c (a simulated NOR: every refusal, the crash mark, a good module, two modules).
 *
 * Revisions:
 *   0.10.0: first version.
 *   0.10.1: the crash count beyond the first 30 s (oc_mod_power_on, oc_mod_ban_notice); the update loader's record rule on the sector tails (MD_BAD_TAIL, MI_TAIL, oc_mod_tails). A replacement is written beside the old
 *           module when another start is free (oc_mod_plan, oc_mod_inplace, mw.inplace); the header comment now says what the code does.
 */
#ifndef FM1_GUARD_CODE_END_DEFINED                             /* hal/fm1_guard.h declares it; a host build has no guard header */
static uint32_t fm1_guard_code_end;
#include "../hal/fm1_guard_pc.h"
#define OC_HOST_ETEXT 34078720u                                 /* a host build's _etext (any value above the image: the lint keeps hex addresses in hal/) */
static uint32_t host_pc_hi, host_pc_sets;                       /* what the device's LIMIT0_H would hold, and how often it was written */
static void fm1_guard_pc_window(void) { host_pc_hi = fm1_guard_pc_hi(fm1_guard_code_end, OC_HOST_ETEXT); host_pc_sets++; }
#endif
/* FL_XIP(off): the XIP address of flash offset off (hal/fm1_flash.h on the device; tests/hostsim.c for a host build) */
#define OC_MOD_STARTS 2u                                       /* the places a 6 sector module can be: sector index 0 and 6 of the area */
#define OC_MOD_START_SECTOR(i) ((i) * 6u)
#define OC_MOD_MAXLEN (6u * 0x1000u)                           /* the most a module may be: the sectors up to the next start (6) or the end of the area */
#define OC_MOD_STATE_MAX 16384u                                /* a sanity limit on state_size (bytes of arena per part) */
enum { MD_EMPTY, MD_TORN, MD_REFUSED, MD_BAD_HDR, MD_BAD_CRC, MD_CRASHED, MD_NOSLOT, MD_OK,
       MD_BAD_TAIL };                                          /* oc_mod.state[i]; MD_BAD_TAIL (0.10.1) is last so the numbers the web page knows stay */

static struct {
    uint8_t state[OC_MOD_STARTS];                              /* MD_* */
    uint8_t why[OC_MOD_STARTS];                                /* MD_REFUSED: fw_module_check's code; MD_NOSLOT: eng_register's, negated */
    uint32_t engine[OC_MOD_STARTS];                            /* the engine number the header names (valid once the header is) */
    uint32_t length[OC_MOD_STARTS];
    uint32_t version[OC_MOD_STARTS];                           /* (0.11.0) the header's version word (valid with the engine) */
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
        oc_modguard.unclean[0] = oc_modguard.unclean[1] = 0;
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
    uint8_t r[80];
    uint32_t s, n = (len + 0xFFFu) / 0x1000u;
    for (s = 0; s < n; s++) {
        if (st_read(oc_mod_flash(i) + s * 0x1000u + 0x1000u - 0x100u, r, sizeof r))
            return -1;
        if (pk_record_like(r))
            return (int)(s + 1u);
    }
    return 0;
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
    if (h->link_addr != link || len <= FW_MOD_CODE_OFF || len > OC_MOD_MAXLEN || (len & 3u) ||
        h->engine_ptr < link + FW_MOD_CODE_OFF || h->engine_ptr >= link + len || (h->engine_ptr & 3u) ||
        !eng_module_slot(h->engine) || h->state_size == 0 || h->state_size > OC_MOD_STATE_MAX ||
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
    rc = oc_mod_tails(i, len);
    if (rc < 0)
        return MD_BAD_CRC;
    if (rc > 0) {
        oc_mod.why[i] = (uint8_t)rc;
        return MD_BAD_TAIL;
    }
    return MD_OK;
}

/* persist_boot, after oc_pack_boot: look at every start, register what is good. `crashed`: the last boot died in its first 30 s
 * (bootguard.failed). Safe to call again (the installer does, after an install or a removal): the registry is rebuilt. */
static void oc_mod_boot(uint32_t crashed)
{
    uint32_t i, e;
    oc_mod_guard_init();
    for (e = 0; e < NENGINES; e++)
        if (eng_mod[e])
            eng_unregister(e);
    memset(&oc_mod, 0, sizeof oc_mod);
    fm1_guard_code_end = 0;
    if (crashed) {
        oc_modguard.notice |= (uint8_t)(oc_modguard.inuse & ~oc_modguard.banned);
        oc_modguard.banned |= oc_modguard.inuse;                /* the module(s) in use when it died */
    }
    oc_modguard.inuse = 0;
    for (i = 0; i < OC_MOD_STARTS; i++) {
        fw_mod_hdr_t h;
        uint32_t st = oc_mod_check(i, &h);
        oc_mod.state[i] = (uint8_t)st;
        if (st == MD_EMPTY)
            continue;
        oc_mod.engine[i] = h.engine;
        oc_mod.length[i] = h.length;
        oc_mod.version[i] = h.version;
        if (st != MD_OK)
            continue;
        if (oc_modguard.banned & (1u << i)) {
            oc_mod.state[i] = MD_CRASHED;
            continue;
        }
        {   int rc = eng_mod[h.engine] ? EM_SLOT : eng_register(oc_mod_hdr_ptr(i), (const engine_t *)h.engine_ptr);   /* (one already there: the lower start won) */
            uint32_t end = oc_mod_link(i) + h.length - 1u;
            if (rc != EM_OK) {
                oc_mod.state[i] = MD_NOSLOT;
                oc_mod.why[i] = (uint8_t)-rc;
                continue;
            }
            if (fm1_guard_code_end < end)
                fm1_guard_code_end = end;
        }
    }
    fm1_guard_pc_window();                                      /* (0.10.1) a running session too: the window follows the registered modules */
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
        ui_say("MODULE BANNED ", oc_mod.engine[i] == ENGI_SHAPES ? "SHAPES" : "ENGINE");
        return;
    }
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
/* the start an install of `engine` goes to, or -1 (0.10.1: a free start first, so a replacement is written beside the old module) */
static int oc_mod_plan(uint32_t engine)
{
    uint32_t i;
    if (!eng_module_slot(engine))
        return -1;
    for (i = 0; i < OC_MOD_STARTS; i++)
        if (oc_mod.state[i] == MD_EMPTY) return (int)i;
    for (i = 0; i < OC_MOD_STARTS; i++)
        if (!oc_mod_usable(i)) return (int)i;
    for (i = 0; i < OC_MOD_STARTS; i++)
        if (oc_mod_has(i, engine)) return (int)i;         /* no start free: in place */
    return -1;
}
/* an install of `engine` at `start` replaces its old module where it stands (the old one is erased first; a cut leaves none) */
static int oc_mod_inplace(uint32_t engine, uint32_t start)
{
    return start < OC_MOD_STARTS && oc_mod_has(start, engine);
}
/* the engine out of the table; the voices and the arena state of the tracks that play it cleared: nothing calls into the module after this */
static void oc_mod_park(uint32_t engine)
{
    uint32_t t, i;
    if (engine < NENGINES && eng_mod[engine])
        eng_unregister(engine);
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
static int oc_mod_erase_start(uint32_t i)
{
    uint32_t s;
    for (s = 0; s < 6u; s++) {
        int rc;
        SMP_FEED();
        if ((rc = st_erase(oc_mod_flash(i) + s * 0x1000u)) != 0) return rc;
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
    if (!eng_module_slot(engine) || start >= OC_MOD_STARTS || len <= FW_MOD_CODE_OFF || len > OC_MOD_MAXLEN || (len & 3u))
        return MI_REFUSED;
    if (lg_incomplete)
        return MI_PAIRS;
    plan = oc_mod_plan(engine);
    if (plan < 0)
        return MI_NOFREE;
    if ((uint32_t)plan != start)
        return MI_REFUSED;
    mw.inplace = (uint8_t)oc_mod_inplace(engine, start);
    if (mw.inplace) {                                    /* in place: the old module out first, then every usable module of this engine, and the start asked for */
        oc_mod_park(engine);
        for (i = 0; i < OC_MOD_STARTS; i++)
            if (i == start || oc_mod_has(i, engine))
                if ((rc = oc_mod_erase_start(i)) != 0) { fm1_guard_code_end = 0; return rc; }
    } else if ((rc = oc_mod_erase_start(start)) != 0) {  /* beside: only the free start; the old module stays registered and playable */
        return rc;
    }
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
    if (h->magic != FW_MOD_MAGIC || ocs_crc32(pg, FW_MOD_HDR_BYTES - 4u) != h->hcrc || fw_module_check(h) != FW_MOD_OK ||
        h->length != mw.len || h->engine != mw.engine || h->link_addr != oc_mod_link(mw.start) || h->crc != ~mw.crc ||
        h->engine_ptr < h->link_addr + FW_MOD_CODE_OFF || h->engine_ptr >= h->link_addr + h->length || (h->engine_ptr & 3u) ||
        h->state_size == 0 || h->state_size > OC_MOD_STATE_MAX || h->pack_hash != OC_PACK_HASH)
        return MI_REFUSED;
    if (oc_mod_tails(mw.start, mw.len) != 0)                               /* (0.10.1) the code as the flash has it: no sector tail like an update record */
        return MI_TAIL;
    if ((rc = st_prog(oc_mod_flash(mw.start), pg, sizeof pg)) != 0)       /* the commit, last */
        return rc;
    if (!mw.inplace) {                                                     /* beside (0.10.1): the old module goes only after the new one reads back good */
        fw_mod_hdr_t nh;
        uint32_t i, st = oc_mod_check(mw.start, &nh), old = 0;
        if (st != MD_OK && st != MD_NOSLOT) {
            oc_mod_erase_start(mw.start);                                  /* the new one is junk: out of the way, the old module untouched */
            oc_mod_boot(0);
            return MI_BAD;
        }
        for (i = 0; i < OC_MOD_STARTS; i++)
            if (i != mw.start && oc_mod_has(i, mw.engine)) old |= 1u << i;
        if (old) {
            oc_mod_park(mw.engine);                                        /* nothing calls into the old module from here */
            for (i = 0; i < OC_MOD_STARTS && !rc; i++)
                if (old & (1u << i)) rc = oc_mod_erase_start(i);           /* its header sector first: from the first erase the new module is the only valid one */
        }
        oc_mod_boot(0);                                                    /* registered as at boot: the new one */
        if (rc) return rc;                                                 /* the new module is in; the old one could not be erased (both valid: the lower start is used) */
        return oc_mod.state[mw.start] == MD_OK ? MI_OK : MI_BAD;
    }
    oc_mod_boot(0);                                                        /* read as at boot: the module is registered when it is good */
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
    for (i = 0; i < OC_MOD_STARTS && !rc; i++)
        if (engine == 127u || oc_mod_has(i, engine))
            rc = oc_mod_erase_start(i);
    oc_mod_boot(0);
    return rc;
}
