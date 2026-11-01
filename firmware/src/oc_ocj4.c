/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 * Orange Cat, Release 3 (branch r3-format, OC_R3_FORMAT): the project record OCJ4 and the slot's store behind the FBKK
 * staging. Included by oc_project.c under the switch, after the record format before (whose reader stays for the
 * conversion, Part 3, and whose FBKK bank reader, ocj_read_banks, this file shares). The design:
 * docs/orangecat/PROJECT_FORMAT_R3.md sections 4, 5 and 8, UI_BUILD.md "Release 3 format revision, Part 1".
 *
 * The record (type OCS_PROJECT, id 0x8000 | slot; one to four parts)
 *   head: magic "OCJ4" u32, version u8 (1), head length u8 (12), rsv u16, body length u32; then sections, each
 *   `tag u32, length u32, bytes`, plain (no run-length coding: a reader seeks; the zero-heavy parts of the format before,
 *   the notes and the banks' steps, are in the chunks now). The sections written, in this order:
 *     HEAD  name 12, parts u8, sel u8, seed u32, uid u32, revision u32, profile name 16, profile id u32, next stable id
 *           u32, phys u8, fm6_fn_ok u8                                                                 (52 bytes known)
 *     GLOB  count u16, count x i16: the globals g[] by count (a later firmware's extra ones take their defaults)
 *     TRKS  a table (count u16, entries of `elen u16, bytes`), one entry a track: stable id u32, engine slot u8, module
 *           version u32, module id u32 (0), voice limit u8 (0), preset u8, mpc octave i8, user slot u16, active bank u8,
 *           Prophet origin u8, flags u8 (bit 0: automation on), the eight banks' phrase ids + 1 (u8 each), count u16,
 *           count x i16: the parameters by count (the first count - 8 the common ids in order, the last 8 P_E0..P_E7)
 *     NATV  a table: track u8, kind u8 (1 FM6 packed + its functions, 2 CZ-1, 3 Prophet, 4 DRUM; 5 DRUM-X reserved), the
 *           bytes. The track's own engine's block is always written; another engine's only when it differs from its
 *           kind's init patch (UI_BUILD.md, review finding 14)
 *     MCHN  Melodee's own chain: chain_config_t, then chain_patterns (CHAIN_ROWS x NTRK)
 *     LIST  ocl_lists_t without its scenes: chains, song, tchain, cur, row, mode, launch, groove, gamt
 *     SCNS  version u8 (1), count u8 (8), the scenes as today (8 x NTRK x 2), used u16 (the scene_used mask)
 *     PERF  the Performance Mode table: length 0 until Part 7 (carried if a record has one)
 *     PHRD  sparse: count u8, count x (id + 1 u8, chunk u8, LEN u8, CRC-32 u32, reserved u16), then 32 bytes: a
 *           generation bit a chunk number (oc_pool_r3.c)
 *     FRZR  folders u8, folders x char[32] (the take folders the entries name, by name: no unit-local index in the
 *           record), then a table: entries of `id + 1 u8, frzp_t (32 B; its dir and want are indices into the folders)`
 *     SMPR  a table: kind u8 (1 the internal slot, 2 a card file), name 12, length u32, CRC-32 u32, tracks u8, zones u16
 *     FXRC, REND, TSTA, ROUT  length 0 (carried whole when a record has them)
 *     then every section whose tag this firmware does not know, as it was
 *   Unknown sections, the bytes after a known section's fields, the bytes after a table entry's fields and the entries
 *   past the count a writer knows all survive a save: the writer carries them from the record the music was loaded
 *   from (the slot of proj_cur whose HEAD uid is the music's), read from the flash while the new record is written.
 *
 * In RAM: ocj4_meta (the fields no FBKK has: uid, revision, profile, seed, the track ids, the module versions, the
 * voice limits, SMPR; about 470 B), ocj_dir (the directory a read of another slot goes through, 289 B), ocj4_idx (the
 * old record's section index, 288 B), ocj4_gen (one generator for a foreign project's banks).
 *
 * Revisions:
 *   Part 1 (October 31, 2026): first version.
 */
#define OCJ4_MAGIC 0x344A434Fu                 /* "OCJ4" */
#define OCJ4_VERSION 1u
#define OCJ4_HLEN 12u
#define OCJ4_MAX (8u + PROJ_STORE_SIZE)        /* a record is built in, and read into, the staging's project part */
#define OCJ4_TAG(a, b, c, d) ((uint32_t)(a) | (uint32_t)(b) << 8 | (uint32_t)(c) << 16 | (uint32_t)(d) << 24)
#define T_HEAD OCJ4_TAG('H', 'E', 'A', 'D')
#define T_GLOB OCJ4_TAG('G', 'L', 'O', 'B')
#define T_TRKS OCJ4_TAG('T', 'R', 'K', 'S')
#define T_NATV OCJ4_TAG('N', 'A', 'T', 'V')
#define T_MCHN OCJ4_TAG('M', 'C', 'H', 'N')
#define T_LIST OCJ4_TAG('L', 'I', 'S', 'T')
#define T_SCNS OCJ4_TAG('S', 'C', 'N', 'S')
#define T_PERF OCJ4_TAG('P', 'E', 'R', 'F')
#define T_PHRD OCJ4_TAG('P', 'H', 'R', 'D')
#define T_FRZR OCJ4_TAG('F', 'R', 'Z', 'R')
#define T_SMPR OCJ4_TAG('S', 'M', 'P', 'R')
#define T_FXRC OCJ4_TAG('F', 'X', 'R', 'C')
#define T_REND OCJ4_TAG('R', 'E', 'N', 'D')
#define T_TSTA OCJ4_TAG('T', 'S', 'T', 'A')
#define T_ROUT OCJ4_TAG('R', 'O', 'U', 'T')
#define OCJ4_HEAD_KNOWN 52u
#define OCJ4_TRK_KNOWN (31u + 2u * P_COUNT)
#define OCJ4_LIST_KNOWN (OCL_NCHAIN * OCL_NENT * 2u + OCL_NROW * NTRK + 3u * NTRK + 2u + 2u * NTRK)
#define OCJ4_SCNS_KNOWN (2u + OCL_NSCENE * NTRK * 2u + 2u)
#define OCJ4_MCHN_KNOWN ((uint32_t)sizeof(chain_config_t) + CHAIN_ROWS * NTRK)
#define OCJ4_PHRD_ENT 9u
#define OCJ4_SMP_ENT 24u
#define OCJ4_SMP_MAX 16u
#define OCJ4_FRZ_FOLD 32u
#define OCJ4_FRZ_ENT 33u
#define OCJ4_IDX_MAX 32u
#if defined(FRZ_AUDIO) || defined(OCJ4_FRZ_TEST)   /* the card code's freeze table (card_freeze.c), or the host test's stand-in */
#define OCJ4_FRZ 1
#else
#define OCJ4_FRZ 0
#endif
#define OCJ4_FRZ_LIVE 64u                      /* the live frozen-phrase table is still indexed by id, 1..64 (Part 1) */
#define OCJ4_FRZ_MIN (OCJ4_FRZ ? OCJ4_FRZ_LIVE : 0u)   /* entries of ids above this are carried; below it the live table writes them */
#define FZD_N_MAX 16u                          /* usb_files.c's FZD_N (asserted equal there): the take folders a unit keeps */
#define OCJ4_NATV_MAX 160u                     /* the largest native block (FM6's 128 + 16, CZ-1's 144, the Prophet's 138) */
_Static_assert(FM6_PACKED + FM6_NFN <= OCJ4_NATV_MAX && CZ_BYTES <= OCJ4_NATV_MAX && sizeof(p5_patch_t) <= OCJ4_NATV_MAX &&
               DRUM_WIRE_BYTES <= OCJ4_NATV_MAX, "a native block fits its buffer");
enum { NATV_FM6 = 1, NATV_CZ, NATV_P5, NATV_DRUM, NATV_DXK };
enum { SMP_NONE, SMP_RESOLVED, SMP_CHANGED, SMP_MISSING };

typedef struct {
    uint32_t uid, rev, seed, next_id, prof_id;
    char prof[16];
    uint32_t trk_id[NTRK], mod_ver[NTRK];
    uint8_t vlim[NTRK];
    uint8_t scns_foreign;                      /* the record's SCNS is of a version this firmware does not read: carried whole */
    uint8_t nsmp;
    struct { uint8_t kind; char name[12]; uint32_t len, crc; uint8_t tracks; uint16_t zones; } smp[OCJ4_SMP_MAX];
    uint8_t smp_state[NTRK];                   /* SMP_*: how each track's sample reference resolved at the load */
} ocj4_meta_t;
static ocj4_meta_t ocj4_meta __attribute__((section(".pool")));   /* the music loaded's */
static ocp_dir_t ocj_dir __attribute__((section(".pool")));       /* the directory a read of another slot goes through */
static ocp_gen_t ocj4_gen __attribute__((section(".pool")));      /* a foreign project's banks, one at a time */
static struct { uint32_t tag, off, len; } ocj4_idx[OCJ4_IDX_MAX] __attribute__((section(".pool")));
static uint32_t ocj4_nidx;                     /* the sections of the record being carried (0: none) */
static uint8_t ocj4_idx_full;                  /* the record being carried has more sections than the index holds: the save is refused */
static ocs_rec_t ocj4_old;                     /* .. that record */
static int ocj_read(uint32_t slot, uint8_t *raw);
static void ocj4_old_chunks_cleanup(void);        /* (oc_convert.c) */

/* ------------------------------------------------------------ little-endian --- */
static uint32_t rd16(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8; }
static uint32_t rd32(const uint8_t *p) { return rd16(p) | rd16(p + 2) << 16; }
static void wr16(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void wr32(uint8_t *p, uint32_t v) { wr16(p, v); wr16(p + 2, v >> 16); }

/* ------------------------------------------------------------ the init patches (NATV) --- */
/* the init patch of a kind, as a project of a track that never played that engine stores it */
static void ocj4_natv_init(uint32_t kind, uint8_t *out)
{
    if (kind == NATV_FM6) { memcpy(out, FM6_INIT, FM6_PACKED); memcpy(out + FM6_PACKED, FM6_FNDEF, FM6_NFN); }
    else if (kind == NATV_CZ) cz_patch_init(out);
    else if (kind == NATV_P5) p5_patch_init((p5_patch_t *)(void *)out);
    else memset(out, 0, DRUM_WIRE_BYTES);
}
static uint32_t ocj4_natv_size(uint32_t kind)
{
    return kind == NATV_FM6 ? FM6_PACKED + FM6_NFN : kind == NATV_CZ ? CZ_BYTES : kind == NATV_P5 ? (uint32_t)sizeof(p5_patch_t) :
           kind == NATV_DRUM ? DRUM_WIRE_BYTES : 0u;
}
/* track k's block of a kind as the FBKK's project part has it */
static void ocj4_natv_get(const project_t *q, uint32_t k, uint32_t kind, uint8_t *out)
{
    if (kind == NATV_FM6) { memcpy(out, q->fm6[k], FM6_PACKED); memcpy(out + FM6_PACKED, q->fm6_fn[k], FM6_NFN); }
    else if (kind == NATV_CZ) memcpy(out, q->cz[k].raw, CZ_BYTES);
    else if (kind == NATV_P5) memcpy(out, &q->p5[k], sizeof(p5_patch_t));
    else drum_patch_pack(out, &q->drum[k]);
}
/* .. and put back (0 when the bytes do not read) */
static int ocj4_natv_put(project_t *q, uint32_t k, uint32_t kind, const uint8_t *in)
{
    if (kind == NATV_FM6) {
        for (uint32_t i = 0; i < FM6_PACKED; i++) if (in[i] > 127u) return 0;
        memcpy(q->fm6[k], in, FM6_PACKED);
        memcpy(q->fm6_fn[k], fm6_fn_ok(in + FM6_PACKED) ? in + FM6_PACKED : FM6_FNDEF, FM6_NFN);
        return 1;
    }
    if (kind == NATV_CZ) { if (!cz_patch_valid(in)) return 0; memcpy(q->cz[k].raw, in, CZ_BYTES); return 1; }
    if (kind == NATV_P5) { p5_patch_t p; memcpy(&p, in, sizeof p); if (!p5_patch_valid(&p)) return 0; q->p5[k] = p; return 1; }
    return drum_patch_unpack(&q->drum[k], in);
}
/* the kind of native block engine e plays (0 none) */
static uint32_t ocj4_natv_kind_of(uint32_t e)
{
    return e == ENGI_FM6 ? NATV_FM6 : e == ENGI_CZ ? NATV_CZ : e == ENGI_PROPHET ? NATV_P5 : e == ENGI_DRUM ? NATV_DRUM : 0u;
}

/* ------------------------------------------------------------ the writer --- */
typedef struct { uint8_t *b; uint32_t pos, max, sec, ent; int over; } ocj4_w_t;
static void w_mem(ocj4_w_t *w, const void *p, uint32_t n)
{
    if (w->pos + n > w->max) { w->over = 1; w->pos = w->max; return; }
    if (p) memcpy(w->b + w->pos, p, n); else memset(w->b + w->pos, 0, n);
    w->pos += n;
}
static void w8(ocj4_w_t *w, uint32_t v) { uint8_t b = (uint8_t)v; w_mem(w, &b, 1); }
static void w16(ocj4_w_t *w, uint32_t v) { uint8_t b[2]; wr16(b, v); w_mem(w, b, 2); }
static void w32(ocj4_w_t *w, uint32_t v) { uint8_t b[4]; wr32(b, v); w_mem(w, b, 4); }
static void w_sec(ocj4_w_t *w, uint32_t tag) { w32(w, tag); w->sec = w->pos; w32(w, 0); }
static void w_sec_end(ocj4_w_t *w) { if (!w->over) wr32(w->b + w->sec, w->pos - w->sec - 4u); }
static void w_ent(ocj4_w_t *w) { w->ent = w->pos; w16(w, 0); }
static void w_ent_end(ocj4_w_t *w) { if (!w->over) wr16(w->b + w->ent, w->pos - w->ent - 2u); }
/* n bytes of the old record from off appended */
static void w_old(ocj4_w_t *w, uint32_t off, uint32_t n)
{
    if (w->pos + n > w->max) { w->over = 1; w->pos = w->max; return; }
    if (ocs_at(&ocj4_old, off, w->b + w->pos, n)) { w->over = 1; return; }
    w->pos += n;
}

/* the old record's sections indexed (ocj4_idx; 0 sections when it does not read or there is none) */
static void ocj4_index_old(void)
{
    uint8_t h[OCJ4_HLEN], s[8];
    uint32_t pos, blen;
    ocj4_nidx = 0; ocj4_idx_full = 0;
    if (ocj4_old.total < OCJ4_HLEN || ocs_at(&ocj4_old, 0, h, OCJ4_HLEN) || rd32(h) != OCJ4_MAGIC || h[5] != OCJ4_HLEN) return;
    blen = rd32(h + 8);
    if (blen != ocj4_old.total - OCJ4_HLEN) return;
    for (pos = OCJ4_HLEN; pos + 8u <= ocj4_old.total; pos += 8u + ocj4_idx[ocj4_nidx - 1u].len) {
        if (ocj4_nidx == OCJ4_IDX_MAX) { ocj4_idx_full = 1; return; }
        if (ocs_at(&ocj4_old, pos, s, 8)) { ocj4_nidx = 0; return; }
        ocj4_idx[ocj4_nidx].tag = rd32(s); ocj4_idx[ocj4_nidx].off = pos + 8u; ocj4_idx[ocj4_nidx].len = rd32(s + 4);
        if (ocj4_idx[ocj4_nidx].len > ocj4_old.total - pos - 8u) { ocj4_nidx = 0; return; }
        ocj4_nidx++;
    }
}
static int ocj4_old_find(uint32_t tag)
{
    for (uint32_t i = 0; i < ocj4_nidx; i++) if (ocj4_idx[i].tag == tag) return (int)i;
    return -1;
}
/* entry i of the old table section ix: its bytes' offset and length. 0, or -1 none */
static int ocj4_old_entry(int ix, uint32_t i, uint32_t *off, uint32_t *elen)
{
    uint8_t b[2];
    uint32_t pos, end, count;
    if (ix < 0 || ocj4_idx[ix].len < 2u) return -1;
    pos = ocj4_idx[ix].off; end = pos + ocj4_idx[ix].len;
    if (ocs_at(&ocj4_old, pos, b, 2)) return -1;
    count = rd16(b);
    pos += 2u;
    if (i >= count) return -1;
    for (uint32_t e = 0; ; e++) {
        if (pos + 2u > end || ocs_at(&ocj4_old, pos, b, 2)) return -1;
        *elen = rd16(b); *off = pos + 2u;
        if (*off + *elen > end) return -1;
        if (e == i) return 0;
        pos = *off + *elen;
    }
}
static uint32_t ocj4_old_count(int ix) __attribute__((unused));
static uint32_t ocj4_old_count(int ix)
{
    uint8_t b[2];
    return ix >= 0 && ocj4_idx[ix].len >= 2u && !ocs_at(&ocj4_old, ocj4_idx[ix].off, b, 2) ? rd16(b) : 0u;
}
/* a fixed section ends: the old record's bytes past the fields known here are carried */
static void w_sec_end_tail(ocj4_w_t *w, uint32_t tag, uint32_t known)
{
    int ix = ocj4_old_find(tag);
    if (ix >= 0 && ocj4_idx[ix].len > known) w_old(w, ocj4_idx[ix].off + known, ocj4_idx[ix].len - known);
    w_sec_end(w);
}
/* a table entry ends: the old entry i's bytes past the fields known here are carried */
static void w_ent_end_tail(ocj4_w_t *w, uint32_t tag, uint32_t i, uint32_t known)
{
    uint32_t off, elen;
    if (!ocj4_old_entry(ocj4_old_find(tag), i, &off, &elen) && elen > known) w_old(w, off + known, elen - known);
    w_ent_end(w);
}
/* the old table's entries from index `from` on, whole */
static uint32_t w_old_entries(ocj4_w_t *w, uint32_t tag, uint32_t from)
{
    uint32_t off, elen, n = 0;
    int ix = ocj4_old_find(tag);
    for (uint32_t i = from; !ocj4_old_entry(ix, i, &off, &elen); i++) { w_ent(w); w_old(w, off, elen); w_ent_end(w); n++; }
    return n;
}
/* a section this firmware has no content for: the old one whole, else length 0 */
static void w_sec_carry(ocj4_w_t *w, uint32_t tag)
{
    int ix = ocj4_old_find(tag);
    w_sec(w, tag);
    if (ix >= 0) w_old(w, ocj4_idx[ix].off, ocj4_idx[ix].len);
    w_sec_end(w);
}
static int ocj4_known_tag(uint32_t tag)
{
    static const uint32_t K[] = {T_HEAD, T_GLOB, T_TRKS, T_NATV, T_MCHN, T_LIST, T_SCNS, T_PERF, T_PHRD, T_FRZR, T_SMPR,
                                 T_FXRC, T_REND, T_TSTA, T_ROUT};
    for (uint32_t i = 0; i < sizeof K / sizeof K[0]; i++) if (K[i] == tag) return 1;
    return 0;
}

/* the FRZR section's folders, written: the old record's (so its carried entries' indices hold) then the live
 * table's, by name, without doubles */
#if OCJ4_FRZ
typedef struct { uint32_t (*get)(const char *name); const char *(*name)(uint32_t idx); } ocj4_frzd_t;
static const ocj4_frzd_t *ocj4_frzd;           /* the card code's folder table (usb_files.c); 0 in a build without it */
/* where FRZR's entries and folder names come from: the live table (a save), or the unit's old OCS_FREEZE record split by a project (the
 * conversion, oc_convert.c). get(i, f): entry i (phrase i + 1) into *f, 1 when there is one */
static int ocj4_frz_live_get(uint32_t i, frzp_t *f) { if (i >= FZP_N || !frzp[i].take) return 0; *f = frzp[i]; return 1; }
static const char *ocj4_frz_live_name(uint32_t idx) { return ocj4_frzd ? ocj4_frzd->name(idx) : ""; }
static int (*ocj4_frz_get)(uint32_t, frzp_t *) = ocj4_frz_live_get;
static const char *(*ocj4_frz_name)(uint32_t) = ocj4_frz_live_name;
#endif
static int ocj4_name_eq(const char *a, const char *b)   /* two folder names, at most OCJ4_FRZ_FOLD - 1 characters */
{
    for (uint32_t i = 0; i < OCJ4_FRZ_FOLD; i++) { if (a[i] != b[i]) return 0; if (!a[i]) return 1; }
    return 1;
}
static uint32_t ocj4_frz_oldfolders(void)      /* the old FRZR's folder count (its entries are carried by index) */
{
    uint8_t n;
    int ix = ocj4_old_find(T_FRZR);
    return ix >= 0 && ocj4_idx[ix].len >= 1u && !ocs_at(&ocj4_old, ocj4_idx[ix].off, &n, 1) && n <= FZD_N_MAX &&
           1u + n * OCJ4_FRZ_FOLD <= ocj4_idx[ix].len ? n : 0u;   /* (a table claiming more than a unit keeps: not read) */
}
/* the FRZR section: see the top. The live table's entries (ids 1..64) and the old record's entries above them, carried
 * with their folder indices mapped into a table of only the folders something names (so dead names never fill it).
 * A table that would need more than FZD_N_MAX folders fails the save (w->over) rather than name a wrong folder */
static void w_frzr(ocj4_w_t *w)
{
    uint32_t nold = ocj4_frz_oldfolders(), n = 0, i, ocount = 0, eoff = 0, send = 0, pos;
    int ix = ocj4_old_find(T_FRZR);
    char fold[FZD_N_MAX][OCJ4_FRZ_FOLD];
    uint8_t omap[FZD_N_MAX], map[FZD_N_MAX];   /* the old table's index -> this one's (0xFF: not named); the unit's -> this one's */
    memset(fold, 0, sizeof fold);
    memset(omap, 0xFF, sizeof omap);
    memset(map, 0, sizeof map);
    w_sec(w, T_FRZR);
    if (ix >= 0) {
        uint8_t b[2];
        send = ocj4_idx[ix].len;
        eoff = 1u + nold * OCJ4_FRZ_FOLD;
        if (eoff + 2u <= send && !ocs_at(&ocj4_old, ocj4_idx[ix].off + eoff, b, 2)) ocount = rd16(b);
        eoff += 2u;
    }
    /* pass 1: the folders the carried entries name, first */
    pos = eoff;
    for (i = 0; i < ocount; i++) {
        uint8_t hd[33], b[2];
        uint32_t elen;
        if (pos + 2u > send || ocs_at(&ocj4_old, ocj4_idx[ix].off + pos, b, 2)) break;
        elen = rd16(b);
        if (pos + 2u + elen > send) break;
        if (elen >= 33u && !ocs_at(&ocj4_old, ocj4_idx[ix].off + pos + 2u, hd, 33) && hd[0] > OCJ4_FRZ_MIN) {
            for (uint32_t which = 0; which < 2u; which++) {
                uint32_t o = hd[which ? 18 : 17];
                if (o < nold && omap[o] == 0xFFu) {
                    (void)ocs_at(&ocj4_old, ocj4_idx[ix].off + 1u + o * OCJ4_FRZ_FOLD, fold[n], OCJ4_FRZ_FOLD);
                    fold[n][OCJ4_FRZ_FOLD - 1u] = 0;
                    omap[o] = (uint8_t)n++;
                }
            }
        }
        pos += 2u + elen;
    }
#if OCJ4_FRZ
    for (i = 0; i < FZP_N && i < OCJ4_FRZ_LIVE; i++) {   /* the folders the live entries name */
        frzp_t fe;
        if (!ocj4_frz_get(i, &fe)) continue;
        for (uint32_t which = 0; which < 2u; which++) {
            uint32_t idx = which ? fe.want : fe.dir, j;
            const char *nm = ocj4_frz_name(idx);
            if (idx >= FZD_N_MAX) continue;
            for (j = 0; j < n && !ocj4_name_eq(fold[j], nm); j++)
                ;
            if (j == n) {
                uint32_t l = str_len(nm) < OCJ4_FRZ_FOLD - 1u ? str_len(nm) : OCJ4_FRZ_FOLD - 1u;
                if (n == FZD_N_MAX) { w->over = 1; return; }
                memset(fold[n], 0, OCJ4_FRZ_FOLD); memcpy(fold[n], nm, l); n++;
            }
            map[idx] = (uint8_t)j;
        }
    }
#endif
    w8(w, n);
    for (i = 0; i < n; i++) w_mem(w, fold[i], OCJ4_FRZ_FOLD);
    {
        uint32_t count = 0, at = w->pos;
        w16(w, 0);
#if OCJ4_FRZ
        for (i = 0; i < FZP_N && i < OCJ4_FRZ_LIVE; i++) {
            frzp_t f;
            if (!ocj4_frz_get(i, &f)) continue;
            f.dir = f.dir < FZD_N_MAX ? map[f.dir] : 0u;
            f.want = f.want < FZD_N_MAX ? map[f.want] : 0u;
            w_ent(w); w8(w, i + 1u); w_mem(w, &f, sizeof f); w_ent_end(w);
            count++;
        }
#endif
        pos = eoff;                            /* pass 2: the carried entries, their folder bytes mapped */
        for (i = 0; i < ocount; i++) {
            uint8_t hd[33], b[2];
            uint32_t elen;
            if (pos + 2u > send || ocs_at(&ocj4_old, ocj4_idx[ix].off + pos, b, 2)) break;
            elen = rd16(b);
            if (pos + 2u + elen > send) break;
            if (elen >= 33u && !ocs_at(&ocj4_old, ocj4_idx[ix].off + pos + 2u, hd, 33) && hd[0] > OCJ4_FRZ_MIN) {
                hd[17] = hd[17] < nold && omap[hd[17]] != 0xFFu ? omap[hd[17]] : 0u;
                hd[18] = hd[18] < nold && omap[hd[18]] != 0xFFu ? omap[hd[18]] : 0u;
                w_ent(w); w_mem(w, hd, 33);
                if (elen > 33u) w_old(w, ocj4_idx[ix].off + pos + 2u + 33u, elen - 33u);
                w_ent_end(w);
                count++;
            }
            pos += 2u + elen;
        }
        if (!w->over) wr16(w->b + at, count);
    }
    w_sec_end(w);
}

/* the project record built into buf (max bytes): the FBKK staging raw and its project part q, the directory d (the
 * slot's, as the chunks were written), each bank's phrase pid, the lists l, the metadata m, and the record the music
 * came from (ocj4_old, indexed) for the bytes this firmware carries. Its length, or 0 (too long for the staging) */
static uint32_t ocj4_build(uint8_t *buf, uint32_t max, const project_t *q, const uint8_t *raw, const ocp_dir_t *d,
                           const uint8_t (*pid)[NPAT], const ocl_lists_t *l, const ocj4_meta_t *m,
                           const uint8_t *plen, const uint32_t *phash)
{
    ocj4_w_t w = {buf, 0, max, 0, 0, 0};
    uint32_t k, i;
    w32(&w, OCJ4_MAGIC); w8(&w, OCJ4_VERSION); w8(&w, OCJ4_HLEN); w16(&w, 0); w32(&w, 0);
    /* HEAD */
    w_sec(&w, T_HEAD);
    w_mem(&w, q->name, PROJ_NAME_LEN); w8(&w, q->parts); w8(&w, q->sel);
    w32(&w, m->seed); w32(&w, m->uid); w32(&w, m->rev); w_mem(&w, m->prof, 16); w32(&w, m->prof_id); w32(&w, m->next_id);
    w8(&w, q->phys); w8(&w, q->fm6_fn_ok);
    w_sec_end_tail(&w, T_HEAD, OCJ4_HEAD_KNOWN);
    /* GLOB */
    w_sec(&w, T_GLOB);
    w16(&w, G_COUNT);
    for (i = 0; i < G_COUNT; i++) w16(&w, (uint16_t)q->g[i]);
    w_sec_end_tail(&w, T_GLOB, 2u + 2u * G_COUNT);
    /* TRKS */
    w_sec(&w, T_TRKS);
    {
        uint32_t cat = w.pos;
        w16(&w, 0);
        for (k = 0; k < NTRK; k++) {
            w_ent(&w);
            w32(&w, m->trk_id[k]); w8(&w, q->t[k].engine); w32(&w, m->mod_ver[k]); w32(&w, 0); w8(&w, m->vlim[k]);
            w8(&w, q->t[k].preset); w8(&w, (uint8_t)q->t[k].mpc_octave); w16(&w, q->user[k]); w8(&w, raw[BANK_ACTIVE_OFF + k]);
            w8(&w, proj_p5_origin_value(q, k)); w8(&w, (q->motion.on >> k) & 1u);
            w_mem(&w, pid[k], NPAT);
            w16(&w, P_COUNT);
            for (i = 0; i < P_COUNT; i++) w16(&w, (uint16_t)q->t[k].p[i]);
            w_ent_end_tail(&w, T_TRKS, k, OCJ4_TRK_KNOWN);
        }
        { uint32_t extra = w_old_entries(&w, T_TRKS, NTRK); if (!w.over) wr16(buf + cat, NTRK + extra); }   /* (the count of the entries written) */
    }
    w_sec_end(&w);
    /* NATV: the own engine's block always, the others when they differ from the init; unknown kinds carried */
    w_sec(&w, T_NATV);
    {
        uint32_t count = 0, at = w.pos, off, elen;
        int ix = ocj4_old_find(T_NATV);
        w16(&w, 0);
        for (k = 0; k < NTRK; k++)
            for (uint32_t kind = NATV_FM6; kind <= NATV_DRUM; kind++) {
                uint8_t blk[OCJ4_NATV_MAX], ini[OCJ4_NATV_MAX];
                uint32_t n = ocj4_natv_size(kind);
                ocj4_natv_get(q, k, kind, blk);
                if (kind != ocj4_natv_kind_of(q->t[k].engine % NENGINES)) {
                    ocj4_natv_init(kind, ini);
                    if (!memcmp(blk, ini, n)) continue;
                }
                w_ent(&w); w8(&w, k); w8(&w, kind); w_mem(&w, blk, n); w_ent_end(&w);
                count++;
            }
        for (i = 0; !ocj4_old_entry(ix, i, &off, &elen); i++) {
            uint8_t tk[2];
            if (elen < 2u || ocs_at(&ocj4_old, off, tk, 2) || (tk[1] >= NATV_FM6 && tk[1] <= NATV_DRUM)) continue;
            w_ent(&w); w_old(&w, off, elen); w_ent_end(&w);
            count++;
        }
        if (!w.over) wr16(buf + at, count);
    }
    w_sec_end(&w);
    /* MCHN */
    w_sec(&w, T_MCHN);
    w_mem(&w, &q->chain, sizeof q->chain); w_mem(&w, raw + BANK_CHAIN_OFF, CHAIN_ROWS * NTRK);
    w_sec_end_tail(&w, T_MCHN, OCJ4_MCHN_KNOWN);
    /* LIST */
    w_sec(&w, T_LIST);
    w_mem(&w, l->chain, sizeof l->chain); w_mem(&w, l->song, sizeof l->song);
    w_mem(&w, l->tchain, NTRK); w_mem(&w, l->cur, NTRK); w_mem(&w, l->row, NTRK); w8(&w, l->mode); w8(&w, l->launch);
    w_mem(&w, l->groove, NTRK); w_mem(&w, l->gamt, NTRK);
    w_sec_end_tail(&w, T_LIST, OCJ4_LIST_KNOWN);
    /* SCNS */
    if (m->scns_foreign) {
        w_sec_carry(&w, T_SCNS);
    } else {
        w_sec(&w, T_SCNS);
        w8(&w, 1); w8(&w, OCL_NSCENE); w_mem(&w, l->scene, sizeof l->scene); w16(&w, l->scene_used);
        w_sec_end_tail(&w, T_SCNS, OCJ4_SCNS_KNOWN);
    }
    w_sec_carry(&w, T_PERF);
    /* PHRD: sparse; the reserved field of a phrase carried from the old directory by id */
    w_sec(&w, T_PHRD);
    {
        uint32_t count = 0, at = w.pos;
        int ix = ocj4_old_find(T_PHRD);
        w8(&w, 0);
        for (i = 0; i < OCP_N; i++) {
            uint32_t rsv = 0;
            if (!d->chunk[i]) continue;
            if (ix >= 0 && ocj4_idx[ix].len >= 1u) {   /* (the old entry of this id: its reserved field) */
                uint8_t n, e[OCJ4_PHRD_ENT];
                if (!ocs_at(&ocj4_old, ocj4_idx[ix].off, &n, 1))
                    for (uint32_t j = 0; j < n; j++)
                        if (!ocs_at(&ocj4_old, ocj4_idx[ix].off + 1u + j * OCJ4_PHRD_ENT, e, OCJ4_PHRD_ENT) && e[0] == i + 1u) { rsv = rd16(e + 7); break; }
            }
            if (plen) {
                w8(&w, i + 1u); w8(&w, d->chunk[i]); w8(&w, plen[i]); w32(&w, phash[i]); w16(&w, rsv);
            } else {                           /* (a foreign project's: from the chunks just written) */
                ocs_rec_t r;
                ocp_rd_t rd;
                uint32_t ln = 0, hs = 0;
                if (ocp_r3_open(d, i, &r, &rd) == 1) { ln = ocp_len_of(&r, rd.off, rd.end - rd.off); hs = ocp_hash_of(&r, rd.off, rd.end - rd.off); }
                w8(&w, i + 1u); w8(&w, d->chunk[i]); w8(&w, ln); w32(&w, hs); w16(&w, rsv);
            }
            count++;
        }
        w_mem(&w, d->gen, sizeof d->gen);
        if (!w.over) buf[at] = (uint8_t)count;
    }
    w_sec_end(&w);
    w_frzr(&w);
    /* SMPR */
    w_sec(&w, T_SMPR);
    {
        uint32_t cat = w.pos;
        w16(&w, 0);
        for (i = 0; i < m->nsmp; i++) {
            w_ent(&w);
            w8(&w, m->smp[i].kind); w_mem(&w, m->smp[i].name, 12); w32(&w, m->smp[i].len); w32(&w, m->smp[i].crc);
            w8(&w, m->smp[i].tracks); w16(&w, m->smp[i].zones);
            w_ent_end_tail(&w, T_SMPR, i, OCJ4_SMP_ENT);
        }
        { uint32_t extra = w_old_entries(&w, T_SMPR, m->nsmp); if (!w.over) wr16(buf + cat, m->nsmp + extra); }
    }
    w_sec_end(&w);
    w_sec_carry(&w, T_FXRC); w_sec_carry(&w, T_REND); w_sec_carry(&w, T_TSTA); w_sec_carry(&w, T_ROUT);
    for (i = 0; i < ocj4_nidx; i++)            /* every section this firmware does not know, as it was */
        if (!ocj4_known_tag(ocj4_idx[i].tag)) { w_sec(&w, ocj4_idx[i].tag); w_old(&w, ocj4_idx[i].off, ocj4_idx[i].len); w_sec_end(&w); }
    if (w.over) return 0;
    wr32(buf + 8, w.pos - OCJ4_HLEN);
    return w.pos;
}

/* ------------------------------------------------------------ the reader --- */
/* a record's PHRD section walked on the flash (the pool asks for a slot's directory without the staging): see
 * oc_pool_r3.c for the arguments. 0 read, -1 no record (or one that does not read: an empty directory) */
static int ocj4_phrd_read(uint32_t slot, ocp_dir_t *d, uint8_t named[OCP_NCHUNK / 8u], uint8_t *len, uint32_t *hash)
{
    ocs_rec_t r;
    uint8_t h[OCJ4_HLEN], s[8], e[OCJ4_PHRD_ENT];
    uint32_t pos, n;
    memset(d, 0, sizeof *d);
    d->slot = (uint8_t)slot;
    memset(named, 0, OCP_NCHUNK / 8u);
    if (len) memset(len, 0, OCP_N);
    if (hash) memset(hash, 0, OCP_N * sizeof *hash);
    if (slot >= OCP_SCRATCH || ocs_open(OCS_PROJECT, OCJ4_RID(slot), &r)) return -1;
    if (r.total < OCJ4_HLEN || ocs_at(&r, 0, h, OCJ4_HLEN) || rd32(h) != OCJ4_MAGIC || h[5] != OCJ4_HLEN) return -1;
    for (pos = OCJ4_HLEN; pos + 8u <= r.total; pos += 8u + rd32(s + 4)) {
        if (ocs_at(&r, pos, s, 8) || rd32(s + 4) > r.total - pos - 8u) return -1;
        if (rd32(s) != T_PHRD) continue;
        if (rd32(s + 4) < 1u || ocs_at(&r, pos + 8u, &n, 1)) return -1;
        n &= 255u;
        if (rd32(s + 4) < 1u + n * OCJ4_PHRD_ENT + OCP_NCHUNK / 8u) return -1;
        for (uint32_t j = 0; j < n; j++) {
            uint32_t id, c;
            if (ocs_at(&r, pos + 9u + j * OCJ4_PHRD_ENT, e, OCJ4_PHRD_ENT)) return -1;
            id = e[0]; c = e[1];
            if (!id || id > OCP_N || !c) continue;
            d->chunk[id - 1u] = (uint8_t)c;
            ocp_bit_set(named, c, 1);
            if (len) len[id - 1u] = e[2];
            if (hash) hash[id - 1u] = rd32(e + 3);
        }
        return ocs_at(&r, pos + 9u + n * OCJ4_PHRD_ENT, d->gen, OCP_NCHUNK / 8u) ? -1 : 0;
    }
    return 0;                                  /* (a record with no PHRD: an empty directory) */
}

/* the record in buf (len bytes) parsed: q as its project part (its steps, notes and motion events are the phrases',
 * read after), ocj_h's pid / active / chain / name / fn, ocj_l, and the metadata into m when given (a load; a read
 * for the slot list or the backup leaves the music's alone). 0, or -1 (not a record this firmware reads) */
static int ocj4_parse(const uint8_t *buf, uint32_t len, project_t *q, ocj4_meta_t *m)
{
    uint32_t pos, k, i, got = 0;
    if (len < OCJ4_HLEN || rd32(buf) != OCJ4_MAGIC || buf[4] != OCJ4_VERSION || buf[5] != OCJ4_HLEN || rd32(buf + 8) != len - OCJ4_HLEN)
        return -1;
    memset(q, 0, sizeof *q);
    q->magic = PROJ_MAGIC; q->size = sizeof *q; q->parts = NTRK; q->phys = PROJ_PHYS;
    for (i = 0; i < G_COUNT; i++) q->g[i] = GP[i].def;
    memset(&ocj_h, 0, sizeof ocj_h);
    ocj_h.magic = OCJ4_MAGIC;
    ocl_lists_init(&ocj_l);
    chain_defaults(&q->chain);
    for (k = 0; k < NTRK; k++) {               /* every block its kind's init until NATV says otherwise */
        uint8_t ini[OCJ4_NATV_MAX];
        for (uint32_t kind = NATV_FM6; kind <= NATV_DRUM; kind++) { ocj4_natv_init(kind, ini); (void)ocj4_natv_put(q, k, kind, ini); }
    }
    if (m) { uint8_t st[NTRK]; memcpy(st, m->smp_state, NTRK); memset(m, 0, sizeof *m); memcpy(m->smp_state, st, NTRK); }
    for (pos = OCJ4_HLEN; pos + 8u <= len; pos += 8u + rd32(buf + pos + 4)) {
        uint32_t tag = rd32(buf + pos), n = rd32(buf + pos + 4);
        const uint8_t *b = buf + pos + 8u;
        if (n > len - pos - 8u) return -1;
        if (tag == T_HEAD && n >= OCJ4_HEAD_KNOWN) {
            memcpy(q->name, b, PROJ_NAME_LEN); memcpy(ocj_h.name, b, PROJ_NAME_LEN);
            q->parts = b[12]; q->sel = b[13];
            if (m) { m->seed = rd32(b + 14); m->uid = rd32(b + 18); m->rev = rd32(b + 22); memcpy(m->prof, b + 26, 16); m->prof_id = rd32(b + 42); m->next_id = rd32(b + 46); }
            q->phys = b[50]; q->fm6_fn_ok = b[51] != 0;
            got |= 1u;
        } else if (tag == T_GLOB && n >= 2u) {
            uint32_t c = rd16(b);
            if (2u + 2u * c > n) return -1;
            for (i = 0; i < c && i < G_COUNT; i++) q->g[i] = (int16_t)rd16(b + 2u + 2u * i);
        } else if (tag == T_TRKS && n >= 2u) {
            uint32_t c = rd16(b), at = 2;
            for (k = 0; k < c && k < NTRK; k++) {
                uint32_t elen, np;
                const uint8_t *e;
                if (at + 2u > n) return -1;
                elen = rd16(b + at); e = b + at + 2u; at += 2u + elen;
                if (at > n || elen < 31u) return -1;
                if (m) { m->trk_id[k] = rd32(e); m->mod_ver[k] = rd32(e + 5); m->vlim[k] = e[13]; }
                q->t[k].engine = e[4]; q->t[k].preset = e[14]; q->t[k].mpc_octave = (int8_t)e[15]; q->user[k] = (uint16_t)rd16(e + 16);
                ocj_h.active[k] = e[18]; q->pattern[k] = e[18];
                *proj_p5_origin(q, k) = e[19];
                if (e[20] & 1u) q->motion.on |= (uint8_t)(1u << k);
                memcpy(ocj_h.pid[k], e + 21, NPAT);
                np = rd16(e + 29);
                if (np < 8u || np > P_COUNT || 31u + 2u * np > elen) return -1;
                {
                    int16_t values[P_COUNT], def[P_COUNT];
                    uint32_t eng = q->t[k].engine % NENGINES;
                    for (i = 0; i < np; i++) values[i] = (int16_t)rd16(e + 31u + 2u * i);
                    for (i = 0; i < P_COUNT; i++) def[i] = param_desc_of(eng, i)->def;
                    params_by_count(q->t[k].p, values, np, def);
                }
            }
            got |= 2u;
        } else if (tag == T_NATV && n >= 2u) {
            uint32_t c = rd16(b), at = 2;
            for (i = 0; i < c; i++) {
                uint32_t elen, kind, sz;
                const uint8_t *e;
                if (at + 2u > n) return -1;
                elen = rd16(b + at); e = b + at + 2u; at += 2u + elen;
                if (at > n || elen < 2u) return -1;
                kind = e[1]; sz = ocj4_natv_size(kind);
                if (e[0] >= NTRK || !sz) continue;   /* (a kind this firmware does not know: carried, not read) */
                if (elen < 2u + sz || !ocj4_natv_put(q, e[0], kind, e + 2)) return -1;
            }
        } else if (tag == T_MCHN && n >= OCJ4_MCHN_KNOWN) {
            memcpy(&q->chain, b, sizeof q->chain);
            memcpy(ocj_h.chain, b + sizeof q->chain, CHAIN_ROWS * NTRK);
        } else if (tag == T_LIST && n >= OCJ4_LIST_KNOWN) {
            const uint8_t *p = b;
            memcpy(ocj_l.chain, p, sizeof ocj_l.chain); p += sizeof ocj_l.chain;
            memcpy(ocj_l.song, p, sizeof ocj_l.song); p += sizeof ocj_l.song;
            memcpy(ocj_l.tchain, p, NTRK); p += NTRK; memcpy(ocj_l.cur, p, NTRK); p += NTRK; memcpy(ocj_l.row, p, NTRK); p += NTRK;
            ocj_l.mode = p[0]; ocj_l.launch = p[1]; p += 2;
            memcpy(ocj_l.groove, p, NTRK); p += NTRK; memcpy(ocj_l.gamt, p, NTRK);
        } else if (tag == T_SCNS && n >= 2u) {
            if (b[0] == 1u && b[1] == OCL_NSCENE && n >= OCJ4_SCNS_KNOWN) {
                memcpy(ocj_l.scene, b + 2, sizeof ocj_l.scene);
                ocj_l.scene_used = (uint8_t)rd16(b + 2u + sizeof ocj_l.scene);
            } else if (m) {
                m->scns_foreign = 1;           /* (a version this firmware does not read: kept, plays no scenes) */
            }
        } else if (tag == T_SMPR && n >= 2u) {
            uint32_t c = rd16(b), at = 2;
            for (i = 0; i < c; i++) {          /* (walked whole, whether the metadata is wanted or not: the count must be true) */
                uint32_t elen;
                const uint8_t *e;
                if (at + 2u > n) return -1;
                elen = rd16(b + at); e = b + at + 2u; at += 2u + elen;
                if (at > n || elen < OCJ4_SMP_ENT) return -1;
                if (!m || m->nsmp >= OCJ4_SMP_MAX) continue;
                m->smp[m->nsmp].kind = e[0]; memcpy(m->smp[m->nsmp].name, e + 1, 12);
                m->smp[m->nsmp].len = rd32(e + 13); m->smp[m->nsmp].crc = rd32(e + 17);
                m->smp[m->nsmp].tracks = e[21]; m->smp[m->nsmp].zones = (uint16_t)rd16(e + 22);
                m->nsmp++;
            }
        }
    }
    if ((got & 3u) != 3u || !chain_valid(&q->chain)) return -1;
    for (k = 0; k < NTRK; k++) {
        if (ocj_h.active[k] >= NPAT) return -1;
        for (i = 0; i < NPAT; i++) if (ocj_h.pid[k][i] > OCP_N) return -1;
    }
    ocl_lists_fix(&ocj_l);
    if (q->fm6_fn_ok) {                        /* ocj_h.fn: the FBKK's function block (its magic, then the settings) */
        uint32_t mg = BANK_FN_MAGIC;
        memcpy(ocj_h.fn, &mg, 4);
        memcpy(ocj_h.fn + 4, q->fm6_fn, sizeof q->fm6_fn);
    }
    return 0;
}

/* the record of slot `slot` read whole into buf (max bytes): its length, or -1 */
static int ocj4_read_whole(uint32_t slot, uint8_t *buf, uint32_t max)
{
    ocs_rec_t r;
    if (ocs_open(OCS_PROJECT, OCJ4_RID(slot), &r) || r.total > max || ocs_at(&r, 0, buf, r.total)) return -1;
    return (int)r.total;
}

/* ------------------------------------------------------------ FRZR in, SMPR resolved --- */
#if OCJ4_FRZ
/* the record in buf: its FRZR into the live table (ids 1..64; the folders by name through the card code) */
static void ocj4_frzr_in(const uint8_t *buf, uint32_t len)
{
    uint32_t pos;
    memset(frzp, 0, sizeof frzp);
    for (pos = OCJ4_HLEN; pos + 8u <= len; pos += 8u + rd32(buf + pos + 4)) {
        uint32_t n = rd32(buf + pos + 4), nf, count, at;
        const uint8_t *b = buf + pos + 8u;
        uint8_t map[FZD_N_MAX];
        if (n > len - pos - 8u) return;
        if (rd32(buf + pos) != T_FRZR || n < 3u) continue;
        nf = b[0];
        if (1u + nf * OCJ4_FRZ_FOLD + 2u > n) return;
        for (uint32_t j = 0; j < FZD_N_MAX; j++) {
            char name[OCJ4_FRZ_FOLD];
            uint32_t idx = 0;
            if (j < nf) {
                memcpy(name, b + 1u + j * OCJ4_FRZ_FOLD, OCJ4_FRZ_FOLD);
                name[OCJ4_FRZ_FOLD - 1u] = 0;
                if (ocj4_frzd && name[0]) idx = ocj4_frzd->get(name);
            }
            map[j] = (uint8_t)(idx < FZD_N_MAX ? idx : 0u);
        }
        at = 1u + nf * OCJ4_FRZ_FOLD;
        count = rd16(b + at); at += 2u;
        for (uint32_t e = 0; e < count; e++) {
            uint32_t elen, id;
            if (at + 2u > n) return;
            elen = rd16(b + at); at += 2u;
            if (at + elen > n) return;
            id = elen >= OCJ4_FRZ_ENT ? b[at] : 0u;
            if (id && id <= OCJ4_FRZ_LIVE && id <= FZP_N) {
                frzp_t *f = &frzp[id - 1u];
                memcpy(f, b + at + 1u, sizeof *f);
                f->dir = f->dir < FZD_N_MAX ? map[f->dir] : 0u;
                f->want = f->want < FZD_N_MAX ? map[f->want] : 0u;
            }
            at += elen;
        }
        frzp_dirty = 1;                        /* (the store's mirror follows) */
        return;
    }
}
#endif

/* the music's sample references resolved against the unit (8.3): each track's state, a toast for a changed or
 * missing one. The stored reference is never rewritten here */
static void ocj4_smpr_resolve(ocj4_meta_t *m)
{
    memset(m->smp_state, 0, sizeof m->smp_state);
    for (uint32_t i = 0; i < m->nsmp; i++) {
        uint32_t st = SMP_MISSING;
        char nm[12];
        memset(nm, 0, sizeof nm);
        if (m->smp[i].kind == 1u && smp.state == SMP_FULL) {
            memcpy(nm, smp.hdr.name, 8);
            if (!memcmp(nm, m->smp[i].name, 12))
                st = smp.hdr.data_len == m->smp[i].len && smp.hdr.crc == m->smp[i].crc ? SMP_RESOLVED : SMP_CHANGED;
        }
        for (uint32_t k = 0; k < NTRK; k++)
            if ((m->smp[i].tracks >> k) & 1u) {
                m->smp_state[k] = (uint8_t)st;
                if (st != SMP_RESOLVED) {
                    char msg[20] = "SAMPLE MISSING T1";
                    if (st == SMP_CHANGED) memcpy(msg + 7, "CHANGED", 7);
                    msg[16] = (char)('1' + k);
                    ui_message(msg);
                }
            }
    }
}
/* the music's references before a save: tracks on the sample engine with no reference of the internal slot yet get
 * one (the slot as it is now). A stored reference is written back as it is, whether it resolves or not (8.3: only
 * RELINK, the interface's, rewrites one; so a unit that lacks the sample does not rewrite the project's memory of it) */
static void ocj4_smpr_refresh(ocj4_meta_t *m, const project_t *q)
{
    uint32_t tracks = 0, i;
    for (uint32_t k = 0; k < NTRK; k++) if (q->t[k].engine % NENGINES == ENGI_SLICE) tracks |= 1u << k;
    for (i = 0; i < m->nsmp; i++) if (m->smp[i].kind == 1u) return;
    if (!tracks || smp.state != SMP_FULL || m->nsmp >= OCJ4_SMP_MAX) return;
    memset(&m->smp[m->nsmp], 0, sizeof m->smp[0]);
    m->smp[m->nsmp].kind = 1;
    memcpy(m->smp[m->nsmp].name, smp.hdr.name, 8);
    m->smp[m->nsmp].len = smp.hdr.data_len; m->smp[m->nsmp].crc = smp.hdr.crc;
    m->smp[m->nsmp].tracks = (uint8_t)tracks; m->smp[m->nsmp].zones = (uint16_t)((1u << (smp.hdr.nz > 16u ? 16u : smp.hdr.nz)) - 1u);
    m->nsmp++;
}

/* the metadata of music that has none yet (a new project's first save): the uid and the seed from what is at hand,
 * the track ids from the seed */
static void ocj4_meta_fresh(ocj4_meta_t *m, const uint8_t *raw)
{
    uint32_t h = fnv(2166136261u, raw, BANK_STORE_SIZE), t = fm1_ms, c = ocs_cursor;
    memset(m, 0, sizeof *m);
    h = fnv(h, &t, sizeof t);
    h = fnv(h, &c, sizeof c);
    m->uid = h ? h : 1u;
    m->seed = fnv(h, "seed", 4);
    for (uint32_t k = 0; k < NTRK; k++) m->trk_id[k] = fnv(fnv(m->seed, "trk", 3), &k, 1);
    m->next_id = 1;
}

/* ------------------------------------------------------------ the slot's store --- */
/* the FBKK's bank (k, b) as a phrase generator (a foreign project's banks become its phrases) */
static const uint8_t *ocj4_raw;                /* the FBKK being written (ocj_write) */
static int ocj4_regen(ocp_cw_t *w, uint32_t id)
{
    for (uint32_t k = 0; k < NTRK; k++)
        for (uint32_t b = 0; b < NPAT; b++)
            if (ocj_h.pid[k][b] == id + 1u) {
                ocj_gen_fbkk(&ocj4_gen, &proj_scratch, ocj4_raw, k, b, 0);
                w->g = &ocj4_gen;
                return 0;
            }
    return -1;
}
/* a foreign FBKK (raw; its project part q) into slot `slot`: its banks as phrases 1.. in order, packed into chunks of
 * the slot under the generation opposite to the one its record names, the directory into ocj_dir. 0; 2 no room (the
 * chunks' parts plus `more`, the record's); else as the store */
static int ocj4_foreign_chunks(uint32_t slot, const project_t *q, const uint8_t *raw, uint32_t more)
{
    uint32_t id = 0, c = 1, fill = 1, parts = 0, k, b, cc;
    int rc;
    if (ocj4_phrd_read(slot, &ocp_sdir, ocp_snamed, 0, 0)) { memset(&ocp_sdir, 0, sizeof ocp_sdir); memset(ocp_snamed, 0, sizeof ocp_snamed); }
    memset(&ocj_dir, 0, sizeof ocj_dir);
    ocj_dir.slot = (uint8_t)slot;
    memset(ocj_h.pid, 0, sizeof ocj_h.pid);
    for (k = 0; k < NTRK; k++)                 /* pass 1: each bank's phrase number and chunk (sequential fill) */
        for (b = 0; b < NPAT; b++) {
            uint32_t len = ocj_gen_fbkk(&ocj4_gen, q, raw, k, b, 0);
            if (!len) continue;
            if (id >= OCP_N) return 2;
            ocj_h.pid[k][b] = (uint8_t)(id + 1u);
            if (fill > 1u && fill + 3u + len > OCP_PART) { parts += ocp_parts_of(fill); c++; fill = 1; }
            ocj_dir.chunk[id] = (uint8_t)c;
            fill += 3u + len;
            if (fill > OCP_PART) { parts += ocp_parts_of(fill); c++; fill = 1; }   /* (a phrase larger than a part: alone) */
            id++;
        }
    if (fill > 1u) parts += ocp_parts_of(fill);
    if (ocs_free_count() < parts + more) return 2;
    for (cc = 1; cc <= c; cc++) {              /* pass 2: each chunk written, its phrases generated as they stream */
        uint32_t g = ocp_bit(ocp_snamed, cc) ? !ocp_bit(ocp_sdir.gen, cc) : 0u, any = 0;
        memset(&ocp_cw, 0, sizeof ocp_cw);
        ocp_cw.src.get = ocp_cw_get;
        ocp_cw.regen = ocj4_regen;
        memset(&ocp_otab, 0, sizeof ocp_otab);
        memset(&ocp_ntab, 0, sizeof ocp_ntab);
        for (id = 0; id < OCP_N; id++) {
            if (ocj_dir.chunk[id] != cc) continue;
            if (ocj4_regen(&ocp_cw, id) || ocp_ntab_add(id, ocj4_gen.len, OCP_SRC_GEN)) return -1;
            any = 1;
        }
        if (!any) continue;
        rc = ocs_write_src(OCS_POOL, ocp_rid(slot, g, cc), &ocp_cw.src, ocp_ntab_total());
        if (rc) return rc == -1 ? 2 : rc;
        ocp_bit_set(ocj_dir.gen, cc, g);
        OC_FEED();
    }
    return 0;
}

/* FBKK raw (len bytes, BANK_STORE_SIZE; 0: the slot emptied) into slot. 0 done; 4 no room (PROJECT FULL: nothing written);
 * 2 not (a flash error: the slot's record as it was). raw is used, and left as the slot reads back (ocj_read) when it is done.
 *   The music's own staging (xs: proj_pid and proj_lists go with it; every bank kept to the live pool first,
 *   project.c): the live pool is the slot's (the record names its live generations) or another's (every live chunk
 *   copied into the slot first, the source's orphans swept after). A foreign FBKK (a backup's, Melodee's): its banks
 *   become the slot's phrases 1..; when the slot is the live pool's, the live phrases move to the scratch first, so the
 *   music in RAM keeps them. The record, written last, is the commit; then the live tables are read again from the
 *   flash and the slot swept */
static int ocj_write(uint32_t slot, uint8_t *raw, uint32_t len)
{
    project_t *q = &proj_scratch;
    ocp_dir_t *d = &ocp.dir;
    ocj4_meta_t *m = &ocj4_meta;
    uint32_t rlen, live = ocp.dir.slot;
    int xs, rc, had_old = 0;
    slot &= 3u;
    if (!len) {                                /* emptied: the record (and the format before's, an OLD FORMAT slot) */
        int had_old = ocs_exists(OCS_PROJECT, slot);
        if (ocs_delete(OCS_PROJECT, OCJ4_RID(slot)) || ocs_delete(OCS_PROJECT, slot)) return 2;
        if (had_old) ocj4_old_chunks_cleanup();            /* (a CLEAR of an OLD FORMAT slot: its phrases may be nobody's now) */
        if (slot == live) memset(ocp.cnamed, 0, sizeof ocp.cnamed);   /* (the music keeps its phrases, uncommitted) */
        else ocp_r3_sweep(slot);
        return 0;
    }
    xs = proj_x_tag == proj_hash(raw, BANK_STORE_SIZE);
    if (len != BANK_STORE_SIZE || !bank_valid(raw, len)) return 2;
    ocj4_raw = raw;
    memset(&ocj4_old, 0, sizeof ocj4_old);
    ocj4_nidx = 0;
    if (xs) {
        if (!m->uid) ocj4_meta_fresh(m, raw);
        if (proj_cur < 4u && !ocs_open(OCS_PROJECT, OCJ4_RID(proj_cur), &ocj4_old)) {   /* the record the music came from */
            uint8_t hd[OCJ4_HLEN + 8u + OCJ4_HEAD_KNOWN];
            ocj4_index_old();
            if (ocj4_idx_full) return 2;       /* (more sections than carried safely: refused, not cut short) */
            if (ocj4_nidx && ocj4_idx[0].tag == T_HEAD && ocj4_idx[0].len >= OCJ4_HEAD_KNOWN &&
                !ocs_at(&ocj4_old, ocj4_idx[0].off, hd, OCJ4_HEAD_KNOWN) && rd32(hd + 18) == m->uid && rd32(hd + 22) <= m->rev) {
                /* (its uid is the music's: its foreign bytes are carried) */
            } else {
                ocj4_nidx = 0;                 /* (another project's record, or newer than the music: nothing carried) */
            }
        }
        memcpy(ocj_h.pid, proj_pid, sizeof ocj_h.pid);
        ocj_l = proj_lists;
        ocj4_smpr_refresh(m, q);
        m->rev++;
        if (slot != live) {                    /* every live chunk into the slot; the record names that directory */
            rc = ocp_r3_copy_slot(slot, &ocj_dir, ocp_parts_of(OCJ4_MAX));
            if (rc) { m->rev--; ocp_r3_sweep(slot); return rc == 2 ? 4 : 2; }
            d = &ocj_dir;
        }
    } else {                                   /* a project from elsewhere */
        static ocj4_meta_t fm __attribute__((section(".pool")));
        if (slot == live) {                    /* the music's phrases to the scratch first */
            ocp_r3_sweep(OCP_SCRATCH);
            rc = ocp_r3_copy_slot(OCP_SCRATCH, &ocj_dir, ocp_parts_of(OCJ4_MAX));
            if (rc) return rc == 2 ? 4 : 2;
            ocp_r3_adopt(&ocj_dir);
            live = OCP_SCRATCH;
        }
        ocj4_meta_fresh(&fm, raw);
        fm.rev = 1;
        m = &fm;
        ocl_lists_init(&ocj_l);
        rc = ocj4_foreign_chunks(slot, q, raw, ocp_parts_of(OCJ4_MAX));
        if (rc) { ocp_r3_sweep(slot); return rc == 2 ? 4 : 2; }
        d = &ocj_dir;
    }
    /* the record, built in the staging's project part (its bytes are q's now), the commit */
    rlen = ocj4_build(raw, OCJ4_MAX, q, raw, d, ocj_h.pid, &ocj_l, m, xs ? ocp.len : 0, xs ? ocp.hash : 0);
    if (!rlen || ocs_free_count() < ocp_parts_of(rlen)) { rc = 4; goto failed; }
    /* a record this firmware cannot read back is never committed (the slot would be held as OLD FORMAT, unusable). Parsed into the project
     * scratch (q): the record is built, and the staging is re-read from the slot after the commit anyway; a project_t of its own would be 17 KB */
    if (ocj4_parse(raw, rlen, q, 0)) { rc = 2; goto failed; }
    rc = ocs_write(OCS_PROJECT, OCJ4_RID(slot), raw, rlen);
    if (rc) { rc = rc == -1 ? 4 : 2; goto failed; }
    had_old = ocs_exists(OCS_PROJECT, slot);
    (void)ocs_delete(OCS_PROJECT, slot);       /* (an OLD FORMAT record there, if any, is replaced) */
    if (xs) {
        ocp_r3_switch(slot);                   /* the live pool, as committed */
        ocp_r3_sweep(slot);
        if (slot != live) ocp_r3_sweep(live);  /* (the source's orphans: its uncommitted chunks, or the whole scratch) */
    } else {
        ocp_r3_sweep(slot);                    /* (the live pool is untouched: the scratch's, if it was this slot's) */
    }
    rc = ocj_read(slot, raw) == (int)BANK_STORE_SIZE ? 0 : 2;
    if (had_old) ocj4_old_chunks_cleanup();    /* (the record of the format before is replaced: the old chunks nobody names go) */
    return rc;
failed:
    if (xs) m->rev--;
    if (slot == ocp.dir.slot) {                /* (the record may or may not have committed: the committed bits are read again,
                                                * so that no later keep writes under a generation the new record names) */
        if (!ocj4_phrd_read(slot, &ocp_sdir, ocp_snamed, 0, 0)) { memcpy(ocp.cgen, ocp_sdir.gen, sizeof ocp.cgen); memcpy(ocp.cnamed, ocp_snamed, sizeof ocp.cnamed); }
        else memset(ocp.cnamed, 0, sizeof ocp.cnamed);
    } else {
        ocp_r3_sweep(slot);                    /* (chunks written for it that no record names: erased now) */
    }
    return rc;
}

/* project slot renamed: its record written again with HEAD's name replaced and every other byte as it was (the chunks
 * are untouched, the uid, the revision and every foreign byte stay). 0 done; 1 no record; 2 not written */
static int ocj4_rename(uint32_t slot, const char *name)
{
    uint8_t *buf = proj_wire_borrow();
    int n = ocj4_read_whole(slot & 3u, buf, OCJ4_MAX), rc;
    uint32_t pos, l = str_len(name) < PROJ_NAME_LEN ? str_len(name) : PROJ_NAME_LEN;
    if (n < 0) return 1;
    for (pos = OCJ4_HLEN; pos + 8u <= (uint32_t)n; pos += 8u + rd32(buf + pos + 4)) {
        if (rd32(buf + pos) != T_HEAD) continue;
        if (rd32(buf + pos + 4) < OCJ4_HEAD_KNOWN) return 2;
        memset(buf + pos + 8u, 0, PROJ_NAME_LEN);
        memcpy(buf + pos + 8u, name, l);
        rc = ocs_write(OCS_PROJECT, OCJ4_RID(slot & 3u), buf, (uint32_t)n);
        if (rc) return 2;
        if ((slot & 3u) == ocp.dir.slot) { /* (the live project: the committed bits are the record's, unchanged) */ }
        return 0;
    }
    return 2;
}

/* slot as an FBKK into raw (proj_x with it). BANK_STORE_SIZE, or -1 (no record, or one that does not read). The slot's
 * phrases are read through its own directory, the music's untouched */
static int ocj_read(uint32_t slot, uint8_t *raw)
{
    project_t *q = &proj_scratch;
    int n, rc;
    slot &= 3u;
    if ((n = ocj4_read_whole(slot, raw, OCJ4_MAX)) < 0 || ocj4_parse(raw, (uint32_t)n, q, 0)) return -1;
    if (ocj4_phrd_read(slot, &ocj_dir, ocp_snamed, 0, 0)) return -1;
    memset(raw + BANK_ACTIVE_OFF, 0, BANK_STORE_SIZE - BANK_ACTIVE_OFF);
    memset(proj_x, 0, sizeof proj_x);
    ocp_rd_dir = &ocj_dir;
    rc = ocj_read_banks(raw);
    ocp_rd_dir = &ocp.dir;
    return rc;
}

/* boot: nothing to scan (each slot's phrases are its own; the sweep is the pool's: ocp_scan) */
static void ocj_scan(void) {}
/* how many other slots name a phrase of the live pool: none (project-owned phrases) */
static uint32_t ocj_refs(uint32_t id, uint32_t but) { (void)id; (void)but; return 0; }

/* the music loaded from slot `slot` (project_load, after bank_restore): the live pool is the slot's, the metadata and
 * the frozen-phrase table are the record's, the sample references resolved */
static void ocj4_activate(uint32_t slot)
{
    int n;
    uint8_t *buf = proj_wire_borrow();
    slot &= 3u;
    ocp_r3_switch(slot);
    if ((n = ocj4_read_whole(slot, buf, OCJ4_MAX)) >= 0 && !ocj4_parse(buf, (uint32_t)n, &proj_scratch, &ocj4_meta)) {
#if OCJ4_FRZ
        ocj4_frzr_in(buf, (uint32_t)n);
#endif
        ocj4_smpr_resolve(&ocj4_meta);
        if (ocj4_meta.scns_foreign) ui_message("SCENES NEED NEWER FW");   /* (kept byte for byte; none plays) */
    } else {
        memset(&ocj4_meta, 0, sizeof ocj4_meta);
    }
}
/* the music is new (a new project, the template, a restored runtime): the scratch is its pool, no metadata, no
 * frozen phrases */
static void ocj4_new(void)
{
    ocp_r3_sweep(OCP_SCRATCH);
    ocp_r3_switch(OCP_SCRATCH);
    memset(&ocj4_meta, 0, sizeof ocj4_meta);
#if OCJ4_FRZ
    memset(frzp, 0, sizeof frzp);
    frzp_dirty = 1;
#endif
}
/* a slot's record names the take folder `name` in its FRZR (the card code's frzd_get asks before it frees a folder) */
static int ocj4_folder_named(const char *name)
{
    for (uint32_t slot = 0; slot < 4u; slot++) {
        ocs_rec_t r;
        uint8_t h[OCJ4_HLEN], s[8], nf;
        char f[OCJ4_FRZ_FOLD];
        uint32_t pos;
        if (ocs_open(OCS_PROJECT, OCJ4_RID(slot), &r) || r.total < OCJ4_HLEN || ocs_at(&r, 0, h, OCJ4_HLEN) || rd32(h) != OCJ4_MAGIC) continue;
        for (pos = OCJ4_HLEN; pos + 8u <= r.total; pos += 8u + rd32(s + 4)) {
            if (ocs_at(&r, pos, s, 8) || rd32(s + 4) > r.total - pos - 8u) break;
            if (rd32(s) != T_FRZR || rd32(s + 4) < 1u || ocs_at(&r, pos + 8u, &nf, 1)) continue;
            for (uint32_t j = 0; j < nf && 1u + (j + 1u) * OCJ4_FRZ_FOLD <= rd32(s + 4); j++) {
                if (ocs_at(&r, pos + 9u + j * OCJ4_FRZ_FOLD, f, OCJ4_FRZ_FOLD)) break;
                f[OCJ4_FRZ_FOLD - 1u] = 0;
                if (ocj4_name_eq(f, name)) return 1;
            }
            break;
        }
    }
    return 0;
}
/* slot holds a record this firmware cannot read: an OLD FORMAT slot (the format before's record at id slot, or a
 * new-format one that does not read) */
static int ocj4_old_slot(uint32_t slot)
{
    uint8_t raw[OCJ4_HLEN];
    ocs_rec_t r;
    slot &= 3u;
    (void)raw;
    if (ocs_exists(OCS_PROJECT, slot)) return 1;
    return ocs_open(OCS_PROJECT, OCJ4_RID(slot), &r) == 0;   /* (called only for a slot that did not read: a record there that
                                                            * does not load, whatever its head says, is held, never free) */
}
#define FRZ_SLOT_NAMES(s, i) ((s) == ocp.dir.slot && (i) < OCP_N && (ocp.len[i] || ocp.hash[i]))   /* (usb_files.c: a phrase is its project's) */
#include "oc_convert.c"                        /* (Release 3, Part 3) the conversion of the projects of the format before */
