/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 * Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Persistent storage on the SPI NOR.
 *
 * Every object has an A/B pair (five base plus two extension sectors per bank project; one otherwise). A save goes to the copy that is not
 * the current one: erase the sector, program the payload pages (from offset
 * 256), then the 32-byte header at offset 0 LAST. The header is the commit
 * record (magic, seq, length, payload CRC, header CRC); on load the valid
 * copy with the newest seq wins (including wrap), so a write torn at any point leaves the
 * previous copy in charge.
 *
 * Flash access goes through three hooks (also used by the host test):
 *   st_read(off, dst, n)   st_erase(off)   st_prog(off, src, n)
 *
 * Revisions:
 *   0.9.1 (Orange Cat): the user preset library moved out of the PACK and MODULE areas into a group of 23 sectors with one shared
 *          spare (lib_group.c, included at the end of this file). Handles OBJ_LIB0 + bank (22) are the group's banks; the old
 *          object numbers 5..30 stay, as the old pairs the move at boot reads, and as the ids the lab inventory and the backup
 *          identity use. The MODULE area is 12 sectors now (was 20). OBJ_LEGACY_COUNT is the old OBJ_COUNT.
 *   0.7.2 (Orange Cat): the flash map below in one place, with its two engine areas named (ST_PACK_LO, ST_MODULE_LO, as
 *         FL_PACK_LO and FL_MODULE_LO in hal/fm1_flash.h); the Prophet, CZ-1 and native FM6 bank objects can be retired
 *         at run time (st_retire_engines): refused for writing, reads give "no record", the object numbers kept. Nothing
 *         is retired unless a caller asks (see "Retired engine areas").
 *
 * THE FLASH MAP (one place; hal/fm1_flash.h has the same ranges as FL_*, FL_STORE_OK is what the drivers allow)
 *   0x004000..0x092FFF  app image (the update loader's; never written here)
 *   0x093000..0x09CFFF  PACK area, 10 sectors: the Prophet banks' five A/B pairs today (OBJ_P5BANK0..+4); the asset pack
 *                       (Orange Cat 0.8.0, tools/gen_pack.py) when one is installed, tables read in place through XIP
 *   0x09D000..0x09EFFF  shared preset extension pair (OBJ_SHARED_EXT)
 *   0x09F000, 0x0FE000  FM6 patch bank, copy A and copy B (OBJ_FM6BANK)
 *   0x0A0000..0x0C7FFF  bank projects: two five-sector copies per slot (OBJ_BANK0..+3)
 *   0x0C8000..0x0D3FFF  MODULE area, 12 sectors (0.9.1; was 20): the engine modules (oc_module.c), two starts of six sectors; the retired CZ-1 and
 *                       native FM6 bank objects (OBJ_CZBANK0..+7, OBJ_NATIVEFM0..+1) read as empty. 0x0D4000..0x0DBFFF is free ground the
 *                       library group (lib_group.c) uses. The legacy placement below is the pre-0.9.1 map.
 *   0x0DC000..0x0DFFFF  user preset banks 0 and 1 (OBJ_UPRESET0..+1, upreset.c)
 *   0x0E0000..0x0E4FFF  OTA staging (the update loader's)
 *   0x0E5000..0x0FAFFF  extensions: preset banks 2 and 3 (E5000..E8FFF), FM6 user bank copy A (E9000) and B (FA000),
 *                       the expanded project copies (EA000..F9FFF)
 *   0x0FC000..0x0FDFFF  settings, copies A and B (OBJ_SETTINGS); 0xFE000 is the FM6 bank's copy B
 * The four legacy single-pattern project objects (OBJ_PROJECT0..+3, 0x97000..0x9EFFF by st_sector) are retired: their place is
 * the PACK area's upper sectors and the shared extension pair above. FL_STORE_OK allows 0x93000..0xDFFFF, 0xE5000..0xFAFFF and
 * 0xFC000..0xFEFFF
 * The shared user preset library (upreset.c up_obj) keeps its 384 slots in the PACK and MODULE areas' objects: banks 4 and 5
 * in the native FM6 objects, banks 8..15 in the CZ-1 objects, banks 16..20 in the Prophet objects. That is why nothing is
 * retired by the build alone (below).
 *
 * Retired engine areas (Orange Cat 0.7.2). An engine that is off has no bank objects of its own to keep: a Prophet, CZ-1 or
 * native FM6 object is refused for writing (st_save gives -1, the flash untouched) and reads as "no record" (st_head -1),
 * its object number kept so the backup identity and the layout stay. st_retire_engines(mask) sets which engines are off
 * (ST_ENG_PROPHET, ST_ENG_CZ, ST_ENG_FM6). The default is none: the user preset library lives in those objects, and
 * retiring them would take its banks 4, 5 and 8..20 away. [Until 0.9.0. Since 0.9.1 the library is in the group of lib_group.c
 * and these objects are the OLD pairs: only the move at boot reads them.] Retirement is on by build (ST_RETIRE_BY_BUILD, set below
 * when the engine switches are known: the engines the build turns off, MELODEE_P5/CZ/FM6 = 0); st_retire_engines(mask) still
 * sets it by hand (the tests do) */
#define ST_MAGIC 0x554C4546u                   /* "FELU" */
#define ST_COMPACT_TAG 0x32504D43u             /* CMP2: preset payload begins at byte 32 */
#define ST_SECTOR 4096u
#define ST_PAYLOAD_OFF 256u
#define ST_PAYLOAD_MAX (ST_SECTOR - ST_PAYLOAD_OFF)
#define ST_BANK_BASE_MAX (5u * ST_SECTOR - ST_PAYLOAD_OFF)
#define ST_BANK_EXT_LO 0xEA000u
#define ST_BANK_EXT_MAX (2u * ST_PAYLOAD_MAX)
/* (Orange Cat 0.7.2) the two areas the engines can leave to the asset pack and the module (flash map above) */
#define ST_PACK_LO 0x93000u                    /* 10 sectors: 0x93000..0x9CFFF */
#define ST_PACK_SECTORS 10u
#define ST_MODULE_LO 0xC8000u                  /* 12 sectors: 0xC8000..0xD3FFF */
#define ST_MODULE_SECTORS 12u                  /* (0.9.1: was 20; 0xD4000..0xDBFFF went to the library group, lib_group.c) */
#ifdef FL_PACK_LO                              /* the firmware build includes hal/fm1_flash.h first: the two must agree */
_Static_assert(ST_PACK_LO == FL_PACK_LO && ST_PACK_LO + ST_PACK_SECTORS * 0x1000u == FL_PACK_HI &&
               ST_MODULE_LO == FL_MODULE_LO && ST_MODULE_LO + ST_MODULE_SECTORS * 0x1000u == FL_MODULE_HI, "flash map");
#endif

/* The Prophet user banks: 0x93000..0x9CFFF. The old banks at 0x89000 are retired and never migrated.
 * The current banks occupy the legacy single-pattern projects' place: those objects are retired (2026-10-10, Kerem: not
 * wanted), their numbers kept, nothing loads or saves them; 0x9D000..0x9EFFF is the final shared preset pair.
 * Bank projects occupy 0xA0000..0xC7FFF, two five-sector copies per slot.
 * Expanded project copies each use two more sectors at 0xEA000..0xF9FFF (unused USR area).
 * User samples are disabled: this area must never accept sample uploads.
 * Flash map: app area 0x4000..0x92FFF; Prophet banks 0x93000..0x9CFFF; shared extension 0x9D000..0x9EFFF; FM6 patch bank
 * copy A 0x9F000 (B 0xFE000); bank projects 0xA0000..0xC7FFF; CZ banks 0xC8000..0xD7FFF; native FM6 0xD8000..;
 * user preset banks 0xDC000..0xDFFFF (upreset.c); OTA staging 0xE0000..0xE4FFF; extensions 0xE5000..0xFAFFF;
 * settings 0xFC000 (FL_DATA 0x97000..0xDFFFF, FL_GLOB 0xFC000..) */
/* Object numbers 0..19 stay fixed. U33..U64 use E5000..E8FFF;
 * owned FM6 U01..U32 reuse the old bank pair, U33..U64 use E9000/FA000.
 * E0000..E4FFF remains reserved for OTA loader staging. */
enum { OBJ_SETTINGS, OBJ_PROJECT0, OBJ_UPRESET0 = OBJ_PROJECT0 + 4, OBJ_FM6BANK = OBJ_UPRESET0 + 2, OBJ_BANK0, OBJ_CZBANK0 = OBJ_BANK0 + 4, OBJ_UPRESET_EXT0 = OBJ_CZBANK0 + 8, OBJ_UPFM6_EXT = OBJ_UPRESET_EXT0 + 2, OBJ_NATIVEFM0, OBJ_P5BANK0 = OBJ_NATIVEFM0 + 2, OBJ_SHARED_EXT = OBJ_P5BANK0 + 5,
       OBJ_LEGACY_COUNT,                                  /* 31: the objects with a number of their own (the old OBJ_COUNT; the lab inventory lists these) */
       OBJ_LIB0 = OBJ_LEGACY_COUNT, OBJ_COUNT = OBJ_LIB0 + 22 };   /* (0.9.1) OBJ_LIB0 + bank: a library bank in the group (lib_group.c) */

typedef struct {
    uint32_t magic;
    uint16_t type, slot;
    uint32_t seq, len, crc, rsv[2];
    uint32_t hcrc;
} st_hdr_t;
_Static_assert(sizeof(st_hdr_t) == 32u, "storage commit record layout");

static int st_read(uint32_t off, void *dst, uint32_t n);
static int st_erase(uint32_t off);
static int st_prog(uint32_t off, const void *src, uint32_t n);

static uint32_t st_crc32(const void *p, uint32_t n)   /* zlib CRC-32, 4 bits per step */
{
    static const uint32_t T[16] = {
        0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu, 0x76DC4190u, 0x6B6B51F4u, 0x4DB26158u, 0x5005713Cu,
        0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu, 0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u, 0xBDBDF21Cu};
    const uint8_t *b = p;
    uint32_t c = 0xFFFFFFFFu;
    while (n--) {
        c ^= *b++;
        c = (c >> 4) ^ T[c & 15u];
        c = (c >> 4) ^ T[c & 15u];
    }
    return ~c;
}

static uint32_t lg_sector(uint32_t bank);
static int lg_current(uint32_t bank, st_hdr_t *h), lg_head(uint32_t bank, st_hdr_t *h), lg_save(uint32_t bank, const void *src, uint32_t len);
static int lg_read_range(uint32_t bank, uint32_t off, void *dst, uint32_t n);
static uint8_t st_legacy_io;                   /* 1 while the migration reads the old pairs: build-time retirement does not hide them */

static uint32_t st_sector(uint32_t obj, uint32_t copy)  /* flash offset of copy A (0) / B (1); a group bank: its sector now */
{
    if (obj >= OBJ_LIB0) return lg_sector(obj - OBJ_LIB0);
    if (obj == OBJ_SHARED_EXT) return 0x9D000u+copy*ST_SECTOR;
    if (obj >= OBJ_P5BANK0) return ST_PACK_LO+(obj-OBJ_P5BANK0)*2u*ST_SECTOR+copy*ST_SECTOR;
    if (obj >= OBJ_NATIVEFM0) return ST_MODULE_LO + 16u * ST_SECTOR + (obj - OBJ_NATIVEFM0) * 2u * ST_SECTOR + copy * ST_SECTOR;   /* 0xD8000 */
    if (obj == OBJ_UPFM6_EXT) return copy ? 0xFA000u : 0xE9000u;
    if (obj >= OBJ_UPRESET_EXT0) return 0xE5000u + (obj - OBJ_UPRESET_EXT0) * 2u * ST_SECTOR + copy * ST_SECTOR;
    if (obj >= OBJ_CZBANK0)
        return ST_MODULE_LO + (obj - OBJ_CZBANK0) * 2u * ST_SECTOR + copy * ST_SECTOR;
    if (obj >= OBJ_BANK0)
        return 0xA0000u + (obj - OBJ_BANK0) * 10u * ST_SECTOR + copy * 5u * ST_SECTOR;
    if (obj == OBJ_SETTINGS)
        return 0xFC000u + copy * ST_SECTOR;
    if (obj == OBJ_FM6BANK)
        return copy ? 0xFE000u : 0x9F000u;
    if (obj >= OBJ_UPRESET0)
        return 0xDC000u + (obj - OBJ_UPRESET0) * 2u * ST_SECTOR + copy * ST_SECTOR;
    return 0x97000u + (obj - OBJ_PROJECT0) * 2u * ST_SECTOR + copy * ST_SECTOR;
}

static int st_banked(uint32_t obj) { return obj >= OBJ_BANK0 && obj < OBJ_CZBANK0; }
/* New preset objects start directly after the commit header, leaving the last
 * 256 bytes erased so the SPL/update loader cannot interpret musical data as OTA. */
static uint32_t st_payload_off(uint32_t obj) { return obj >= OBJ_UPRESET_EXT0 ? sizeof(st_hdr_t) : ST_PAYLOAD_OFF; }
static uint32_t st_capacity(uint32_t obj) { return obj >= OBJ_LIB0 ? ST_PAYLOAD_MAX : st_banked(obj) ? ST_BANK_BASE_MAX + ST_BANK_EXT_MAX : obj >= OBJ_UPRESET_EXT0 ? ST_SECTOR - 256u - sizeof(st_hdr_t) : ST_PAYLOAD_MAX; }
static int st_preset_object(uint32_t obj)
{
    return (obj>=OBJ_UPRESET0 && obj<OBJ_BANK0) || (obj>=OBJ_CZBANK0 && obj<OBJ_COUNT);
}
static uint32_t st_payload_off_header(uint32_t obj,const st_hdr_t *h)
{
    if(obj>=OBJ_LIB0)return h->rsv[0]==ST_COMPACT_TAG ? sizeof(st_hdr_t) : ST_PAYLOAD_OFF;
    return st_preset_object(obj) && h->rsv[0]==ST_COMPACT_TAG ? sizeof(st_hdr_t) : st_payload_off(obj);
}
static uint32_t st_extension(uint32_t obj, uint32_t copy)
{
    return ST_BANK_EXT_LO + ((obj - OBJ_BANK0) * 2u + copy) * 2u * ST_SECTOR;
}
/* Keep existing five-sector copies in place. Each copy has its own two-sector
 * extension in unused flash, so migration never overwrites the current copy.
 * The last 256 bytes of extension sectors stay erased: the SPL scans those
 * tails for update records, and musical data must never look like one. */
static uint32_t st_address_at(uint32_t obj, uint32_t copy, uint32_t off, uint32_t *span,uint32_t payload)
{
    if (!st_banked(obj) || off < ST_BANK_BASE_MAX) {
        *span = (st_banked(obj) ? ST_BANK_BASE_MAX : st_capacity(obj)) - off;
        return st_sector(obj, copy) + payload + off;
    }
    off -= ST_BANK_BASE_MAX;
    *span = ST_PAYLOAD_MAX - off % ST_PAYLOAD_MAX;
    return st_extension(obj, copy) + off / ST_PAYLOAD_MAX * ST_SECTOR + off % ST_PAYLOAD_MAX;
}
/* The commit header carries the offset, so old and new copies can coexist
 * during conversion. Never infer layout from a payload length or uncommitted data. */
static uint32_t st_address(uint32_t obj,uint32_t copy,uint32_t off,uint32_t *span)
{
    st_hdr_t h;uint32_t payload=st_payload_off(obj);
    if(st_preset_object(obj) && !st_read(st_sector(obj,copy),&h,sizeof h))payload=st_payload_off_header(obj,&h);
    return st_address_at(obj,copy,off,span,payload);
}
static int st_read_range(uint32_t obj, uint32_t copy, uint32_t off, void *dst, uint32_t n)
{
    if (obj >= OBJ_COUNT || copy > 1u || off > st_capacity(obj) || n > st_capacity(obj) - off) return -1;
    if (obj >= OBJ_LIB0) return lg_read_range(obj - OBJ_LIB0, off, dst, n);
    uint8_t *d = dst;
    while (n) {
        uint32_t span, addr = st_address(obj, copy, off, &span), k = n < span ? n : span;
        if (st_read(addr, d, k)) return -1;
        d += k; off += k; n -= k;
    }
    return 0;
}

static uint8_t st_buf[256] __attribute__((aligned(4)));

/* the legacy single-pattern projects (OBJ_PROJECT0..+3): retired, their numbers reserved */
/* (Orange Cat 0.7.2) the engine that owns a bank object, as a mask bit (0: none) */
enum { ST_ENG_PROPHET = 1, ST_ENG_CZ = 2, ST_ENG_FM6 = 4 };
/* (0.9.1) On wherever the engine switches are known (the device build, and every host test that includes core.h or sets them): an
 * engine the build leaves out has its bank objects retired. The library no longer lives in them (lib_group.c), so nothing is lost;
 * the move of the old pairs reads them past the retirement (st_legacy_io) once, and erases them. */
#if defined(MELODEE_P5) && defined(MELODEE_CZ) && defined(MELODEE_FM6) && !defined(ST_RETIRE_BY_BUILD)
#define ST_RETIRE_BY_BUILD 1
#endif
#ifdef ST_RETIRE_BY_BUILD
static uint8_t st_engines_off = (MELODEE_P5 ? 0 : ST_ENG_PROPHET) | (MELODEE_CZ ? 0 : ST_ENG_CZ) | (MELODEE_FM6 ? 0 : ST_ENG_FM6);
#else
static uint8_t st_engines_off;
#endif
static void st_retire_engines(uint32_t mask) { st_engines_off = (uint8_t)(mask & 7u); }
static uint32_t st_engine_of(uint32_t obj)
{
    return obj >= OBJ_P5BANK0 && obj < OBJ_SHARED_EXT ? ST_ENG_PROPHET :
           obj >= OBJ_CZBANK0 && obj < OBJ_UPRESET_EXT0 ? ST_ENG_CZ :
           obj >= OBJ_NATIVEFM0 && obj < OBJ_P5BANK0 ? ST_ENG_FM6 : 0u;
}
static int st_retired(uint32_t obj)
{
    return (obj >= OBJ_PROJECT0 && obj < OBJ_PROJECT0 + 4u) || (st_engine_of(obj) & st_engines_off);
}

static int st_head(uint32_t obj, uint32_t copy, st_hdr_t *h)   /* commit record valid: 0 */
{
    if (obj >= OBJ_LIB0 && obj < OBJ_COUNT) return copy ? -1 : lg_head(obj - OBJ_LIB0, h);
    if (obj >= OBJ_COUNT || copy > 1u || (st_retired(obj) && !st_legacy_io))
        return -1;
    if (st_read(st_sector(obj, copy), h, sizeof *h))
        return -1;
    if (h->magic != ST_MAGIC || h->type != obj || h->slot != copy || h->len > st_capacity(obj) ||
        h->hcrc != st_crc32(h, sizeof *h - 4u) ||
        (h->rsv[0]==ST_COMPACT_TAG && (!st_preset_object(obj) || h->len>ST_SECTOR-256u-sizeof(st_hdr_t))))
        return -1;
    return 0;
}

static int st_body(uint32_t obj, uint32_t copy, const st_hdr_t *h)   /* streaming CRC, bounded 256-byte scratch, valid: 0 */
{
    uint32_t crc = 0xFFFFFFFFu;
    for (uint32_t off = 0; off < h->len; off += sizeof st_buf) {
        uint32_t n = h->len - off > sizeof st_buf ? sizeof st_buf : h->len - off;
        if (st_read_range(obj, copy, off, st_buf, n)) return -1;
        for (uint32_t i = 0; i < n; i++) {
            crc ^= st_buf[i];
            for (uint32_t j = 0; j < 8u; j++) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc == h->crc ? 0 : -1;

}

/* the current copy: the newest valid sequence (A on a tie), -1 when
 * neither is valid. Headers first, so only the winner's payload is read (it
 * is verified); *h gets its header. */
static int st_current(uint32_t obj, st_hdr_t *h)
{
    st_hdr_t a, b;
    if (obj >= OBJ_LIB0 && obj < OBJ_COUNT) return lg_current(obj - OBJ_LIB0, h);
    int va = st_head(obj, 0, &a) == 0, vb = st_head(obj, 1, &b) == 0;
    if (vb && (!va || (b.seq != a.seq && b.seq - a.seq < 0x80000000u))) {
        if (st_body(obj, 1, &b) == 0) {
            *h = b;
            return 1;
        }
        vb = 0;
    }
    if (va && st_body(obj, 0, &a) == 0) {
        *h = a;
        return 0;
    }
    if (vb && st_body(obj, 1, &b) == 0) {
        *h = b;
        return 1;
    }
    return -1;
}

/* load the whole object into dst; returns its length, or -1 if it does not fit */
static int st_load(uint32_t obj, void *dst, uint32_t max)
{
    st_hdr_t h;
    int cur;
    if (obj >= OBJ_COUNT || (cur = st_current(obj, &h)) < 0 || h.len > max) return -1;
    if (st_read_range(obj, (uint32_t)cur, 0, dst, h.len)) return -1;
    return (int)h.len;
}

/* a committed record is there, whatever it holds (st_load: -1 for one too big for its buffer too) */
static int st_exists(uint32_t obj)
{
    st_hdr_t h;
    return obj < OBJ_COUNT && !st_retired(obj) && st_current(obj, &h) >= 0 && h.len != 0;
}

static int st_save(uint32_t obj, const void *src, uint32_t len)
{
#if defined(MELODEE_AUDIO_PROFILE) && MELODEE_AUDIO_PROFILE
    if(AUDIO_LAB_GUARDED())return -9;
#endif
    uint32_t seq, base, off;
    int cur, rc;
    st_hdr_t h;
    if (obj >= OBJ_COUNT || st_retired(obj) || len > st_capacity(obj))
        return -1;
    if (obj >= OBJ_LIB0) return lg_save(obj - OBJ_LIB0, src, len);
    cur = st_current(obj, &h);
    seq = cur < 0 ? 0u : h.seq;
    base = st_sector(obj, cur == 0 ? 1u : 0u);       /* write the other copy */
    uint32_t payload=st_payload_off(obj),compact=0;
    if(st_preset_object(obj) && len==3716u){uint32_t magic;memcpy(&magic,src,sizeof magic);compact=magic==0x32425055u;}
    if(compact)payload=sizeof(st_hdr_t);
    uint32_t crc = st_crc32(src, len);
    uint32_t extent = obj >= OBJ_BANK0 && obj < OBJ_CZBANK0 ? 5u * ST_SECTOR : ST_SECTOR;
    for (off = 0; off < extent; off += ST_SECTOR)
        if ((rc = st_erase(base + off)) != 0) return rc;
    if (st_banked(obj)) for (off = 0; off < 2u * ST_SECTOR; off += ST_SECTOR)
        if ((rc = st_erase(st_extension(obj, cur == 0 ? 1u : 0u) + off)) != 0) return rc;
    for (off = 0; off < len; off += sizeof st_buf) {
        uint32_t n = len - off > sizeof st_buf ? sizeof st_buf : len - off;
        memcpy(st_buf, (const uint8_t *)src + off, n);
        uint32_t span, addr = st_address_at(obj, cur == 0 ? 1u : 0u, off, &span,payload);
        if ((rc = st_prog(addr, st_buf, n)) != 0) return rc;
    }
    h.magic = ST_MAGIC;
    h.type = (uint16_t)obj;
    h.slot = (uint16_t)(cur == 0 ? 1 : 0);
    h.seq = seq + 1u;
    h.len = len;
    h.crc = crc;
    h.rsv[0]=compact?ST_COMPACT_TAG:0xFFFFFFFFu;h.rsv[1]=0xFFFFFFFFu;
    h.hcrc = st_crc32(&h, sizeof h - 4u);
    if ((rc = st_prog(base, &h, sizeof h)) != 0)       /* the commit record, last */
        return rc;
    {   /* read back: a write-protected or failing part must not report SAVED */
        st_hdr_t chk;
        uint32_t c = cur == 0 ? 1u : 0u;
        if (st_head(obj, c, &chk) || memcmp(&chk, &h, sizeof h) || st_body(obj, c, &chk))
            return -7;
    }
    return 0;
}

#include "lib_group.c"
