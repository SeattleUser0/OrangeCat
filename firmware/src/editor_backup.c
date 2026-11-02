/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 * Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Revisions:
 * (Orange Cat 0.12.2) a backup restore is refused while the transport runs; refusal tests for pack, module, library wipe and restore (editor, card browser)
 * (Orange Cat 0.14.2) the arrangement objects (ids 64..84, LIST 1): the slots' project records and the pool as the record store keeps them, the
 *                     music loaded's lists and phrases; a restore writes them back byte for byte (the backup's arrangement gap, hw/LOG.md)
 */
/* Complete musical archive: no raw addresses are accepted. Objects are staged
 * in main-loop RAM and fully validated before the existing A/B commit path writes.
 * Runtime and saved projects include all 32 pattern banks. Retired user sample
 * slots are absent from the inventory and cannot be restored over project data.
 * (Orange Cat 0.3) A saved project is built from the project store (oc_project.c) into the staging when the LIST
 * counts it and again when it is read; a restored one goes into the store through proj_write_slot.
 */
#ifndef ED_BK_FLASH_PTR
#define ED_BK_FLASH_PTR(off) fm1_xip_ptr(off)
#endif
static const uint8_t ED_BK_IDS[29] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28};
#define ED_BK_N ((uint32_t)sizeof ED_BK_IDS)
/* (Orange Cat 0.14.2) The arrangement objects. A project as ids 0 and 2..5 carry it (FBKK) has no room for Orange Cat's lists (the
 * chains, the song, the scenes, each track's groove: pattern.c ocl_lists_t) nor for which pool phrase each bank is, and the pool itself
 * was no object: a restore brought every project back with a new project's lists (shown on the unit, October 26, 2026). These objects
 * are the record store's own records, read and written as they are (oc_store.c):
 *   64..67  each slot's project record (OCS_PROJECT 0..3: its banks' phrases, its lists, its project part; oc_project.c)
 *   68..83  the pool's sixteen chunks (OCS_POOL 0..15: four phrases each; oc_pool.c)
 *   84      the music loaded: its lists (ocl_lists_t) then each bank's phrase + 1 (bank_pid, NTRK x NPAT)
 * Size 0: no record (a restore of one deletes it). They are listed apart (LIST with the argument 1, after a plain LIST: the same
 * session), so a page that knows only ids 0..28 sees the inventory it always did. A restore writes a record as its pieces arrive,
 * straight into free sectors of the store (ocs_stream_t: a record can be 45 KB, past the staging), its CRC checked whole before
 * the commit headers: an abort, a stale session or a bad CRC leaves the record as it was. A page restores them after ids 2..5 (which
 * write the slots through the pool) and 84 after id 0 (the music loaded, which takes a new project's lists): then the slots, the pool
 * and the music's arrangement are the backup's byte for byte. Restoring them puts any slot the unit had and the backup did not hold
 * away, as ids 2..5 do. */
#define ED_BK_ARR0 64u
#define ED_BK_ARR_POOL (ED_BK_ARR0 + 4u)
#define ED_BK_ARR_LIVE (ED_BK_ARR_POOL + OCP_CHUNKS)
#define ED_BK_ARR_N (ED_BK_ARR_LIVE + 1u - ED_BK_ARR0)
#define ED_BK_LIVE_LEN ((uint32_t)sizeof(ocl_lists_t) + NTRK * NPAT)
#define ED_BK_REC_MAX (OCS_PARTS_MAX * OCS_PAYLOAD_MAX)
_Static_assert(ED_BK_LIVE_LEN <= sizeof proj_wire_u, "the music's arrangement is staged as the other objects are");
static ocs_stream_t ed_bk_stream __attribute__((section(".pool")));
#if OC_R3_FORMAT
static uint8_t ebr_on;                         /* (Release 3) a record by type and id is being put (the end of this file) */
static uint8_t ebr_sess;                       /* (Release 3) a plain LIST began the session; a plain PUT begin does not end it (ed_bk_valid does), so a restore can put records after objects */
#endif
static uint32_t ed_bk_scrc;                    /* the stream's bytes' CRC so far */
static int ed_bk_arr(uint32_t id) { return id >= ED_BK_ARR0 && id <= ED_BK_ARR_LIVE; }
static uint32_t ed_bk_rec_type(uint32_t id) { return id < ED_BK_ARR_POOL ? OCS_PROJECT : OCS_POOL; }
static uint32_t ed_bk_rec_id(uint32_t id) { return id < ED_BK_ARR_POOL ? id - ED_BK_ARR0 : id - ED_BK_ARR_POOL; }
/* n bytes of arrangement object id from off into dst. 0 done */
static int ed_bk_arr_read(uint32_t id, uint32_t off, uint8_t *dst, uint32_t n)
{
    if (id == ED_BK_ARR_LIVE) {
        if (off > ED_BK_LIVE_LEN || n > ED_BK_LIVE_LEN - off) return -1;
        for (uint32_t i = 0; i < n; i++, off++)
            dst[i] = off < sizeof lists ? ((const uint8_t *)&lists)[off] : ((const uint8_t *)bank_pid)[off - sizeof lists];
        return 0;
    }
    ocs_rec_t r;
    if (ocs_open(ed_bk_rec_type(id), ed_bk_rec_id(id), &r)) return n || off ? -1 : 0;
    return ocs_at(&r, off, dst, n);
}
/* object id's size and CRC */
static void ed_bk_arr_info(uint32_t id, uint32_t *len, uint32_t *crc)
{
    ocs_rec_t r;
    uint32_t c = 0xFFFFFFFFu;
    *len = id == ED_BK_ARR_LIVE ? ED_BK_LIVE_LEN : ocs_open(ed_bk_rec_type(id), ed_bk_rec_id(id), &r) ? 0u : r.total;
    for (uint32_t off = 0; off < *len; off += 256u) {
        uint32_t k = *len - off > 256u ? 256u : *len - off;
        if (ed_bk_arr_read(id, off, ed_smp_buf, k)) { *len = 0; c = 0xFFFFFFFFu; break; }
        c = ocs_crc_add(c, ed_smp_buf, k);
    }
    *crc = *len ? ~c : 0u;
}
/* the record a stream wrote, before its commit: it reads as its type must (a project record's head, a chunk's table) */
static int ed_bk_arr_valid(const ocs_stream_t *w)
{
    ocs_rec_t r;
    uint32_t hsz;
    r.parts = (uint8_t)w->parts; r.total = w->len;
    memcpy(r.sect, w->use, sizeof r.sect);
    if (w->type == OCS_PROJECT) return ocj_head_load(&r, &hsz) == 0;
    {
        uint8_t t[OCP_TABLE];
        uint32_t sum = OCP_TABLE;
        if (r.total < OCP_TABLE || ocs_at(&r, 0, t, OCP_TABLE)) return 0;
        for (uint32_t j = 0; j < OCP_PER; j++) sum += ocp_get16(t + 2u * j);
        return sum == r.total;
    }
}
/* after a record restored or deleted: the pool and the slots read again */
static void ed_bk_arr_rescan(void)
{
#if !OC_R3_FORMAT
    ocp_scan();                                /* (Release 3: the pool is the music's; a restore of the format before's records changes
                                                * no slot this firmware reads, and the sweep is the boot's. Part 6 lists the new records) */
#endif
    ocj_scan();
    for (uint32_t i = 0; i < 4u; i++) proj_fetch(i);
    sync_reload = 1; ui.force = 1;
}
/* the music's arrangement staged in raw: in range, then taken (the lists whole, each bank's phrase; their signatures unknown, found
 * when next kept, as a project load leaves them). 0 done, 2 refused */
static uint32_t ed_bk_live_apply(const uint8_t *raw)
{
    ocl_lists_t l;
    const uint8_t *pid = raw + sizeof l;
    memcpy(&l, raw, sizeof l);
    for (uint32_t i = 0; i < NTRK * NPAT; i++) if (pid[i] > OCP_N) return 2;
    ocl_lists_fix(&l);
    lists = l;
    memcpy(bank_pid, pid, sizeof bank_pid);
#if OC_R3_FORMAT
    /* (Release 3) the phrases are the loaded slot's own: a bank that names one this pool does not hold names none (the unit's live pool may not be the
     * one the backup was taken from; the project records restored just before carry their own) */
    for (uint32_t i = 0; i < NTRK * NPAT; i++) if ((&bank_pid[0][0])[i] && !(ocp.len[(&bank_pid[0][0])[i] - 1u] || ocp.hash[(&bank_pid[0][0])[i] - 1u])) (&bank_pid[0][0])[i] = 0;
#endif
    memset(bank_sig, 0, sizeof bank_sig);
    sync_reload = 1; ui.force = 1;
    return 0;
}
#define ED_BK_MAX ((uint32_t)sizeof proj_wire_u)
#define ED_BK_RAW ((uint8_t *)&proj_wire_u)  /* reuse the existing serialized main-loop scratch */
#define ed_bk_set set_rec    /* id 1: the settings record (+ the template), in the settings save's staging (project.c):
                             * a save while a backup runs rewrites it, and the archive's CRC then fails */
static uint32_t ed_bk_set_len;
static uint8_t ed_bk_valid, ed_bk_put, ed_bk_id, ed_bk_gen;
static uint32_t ed_bk_len, ed_bk_crc, ed_bk_pos, ed_bk_ms, ed_bk_usb;
#if MELODEE_FLASH
static uint32_t ed_bk_source_obj, ed_bk_source_copy, ed_bk_source_crc;
#endif
static void ed_bk_u32(uint32_t n) { for (uint32_t i = 0; i < 5u; i++) ed_b((n >> (i * 7u)) & 127u); }
static uint32_t ed_bk_r32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 7 | (uint32_t)p[2] << 14 | (uint32_t)p[3] << 21 | (uint32_t)p[4] << 28;
}
static void ed_bk_pack(const uint8_t *p, uint32_t n)
{
    while (n) {
        uint32_t k = n > 7u ? 7u : n, mask = 0;
        for (uint32_t i = 0; i < k; i++) mask |= (uint32_t)(p[i] >> 7) << i;
        ed_b(mask);
        for (uint32_t i = 0; i < k; i++) ed_b(p[i]);
        p += k; n -= k;
    }
}
static uint32_t ed_bk_preset_bank(uint32_t id)
{
    if(id==6u || id==7u)return id-6u;
    if(id==17u || id==18u)return id-15u;
    if(id>=19u && id<=22u)return id<21u?id-13u:id-17u;
    if(id>=9u && id<=16u)return id-1u;
    if(id>=23u && id<=28u)return id-7u;
    return UP_BANKS;
}
#if MELODEE_FLASH
static uint32_t ed_bk_slot_len[4], ed_bk_slot_crc[4];   /* (Orange Cat 0.3) each slot as the LIST built it */
static uint32_t ed_bk_rt_crc;                           /* .. and the music's (id 0) CRC then */
static uint8_t ed_bk_slot, ed_bk_slot_gen;              /* the slot + 1 the staging holds built (0: none), and
                                                         * proj_wire_gen then (the staging not reused since) */
static void ed_bk_slots(void)
{
    for (uint32_t s = 0; s < 4u; s++) {
        int n = proj_read_slot(s);
        ed_bk_slot_len[s] = n == (int)BANK_STORE_SIZE ? (uint32_t)n : 0u;
        ed_bk_slot_crc[s] = st_crc32(ED_BK_RAW, ed_bk_slot_len[s]);
        fm1_wdt_feed();
    }
    ed_bk_slot = 0;
}
#endif
static const uint8_t *ed_bk_object(uint32_t id,uint32_t *len)
{
    *len=0;
#if MELODEE_FLASH
    ed_bk_source_obj=OBJ_COUNT;
    uint32_t bank=ed_bk_preset_bank(id),obj=bank<UP_BANKS?up_obj(bank):id==1u && template_old()?OBJ_SETTINGS:OBJ_COUNT;
    if(obj<OBJ_COUNT){
        st_hdr_t h;int copy=st_current(obj,&h);
        if(copy>=0){*len=h.len;ed_bk_source_obj=obj;ed_bk_source_copy=(uint32_t)copy;ed_bk_source_crc=h.crc;return ED_BK_FLASH_PTR(st_sector(obj,(uint32_t)copy)+st_payload_off_header(obj,&h));}
        return ED_BK_RAW;
    }
#else
    uint32_t bank=ed_bk_preset_bank(id);
    if(bank<UP_BANKS){*len=sizeof(up_bank_t);return (const uint8_t *)up_cache_bank(bank);}
#endif
#if MELODEE_FLASH
    if (id == 0u && ed_bk_slot && ed_bk_slot_gen == proj_wire_gen) {
        /* (Orange Cat 0.3) the staging holds a slot read since the LIST: the music captured again, the same as the LIST
         * counted it or not at all (a change since: stale, 5) */
        project_capture(&proj_scratch);
        ed_bk_slot = 0;
        if (!bank_pack(ED_BK_RAW, &proj_scratch, 1)) { proj_wire_gen++; return 0; }
        proj_wire_gen++;
        if (st_crc32(ED_BK_RAW, BANK_STORE_SIZE) == ed_bk_rt_crc) ed_bk_gen = proj_wire_gen;
    }
#endif
    if (id == 0u) { *len = BANK_STORE_SIZE; return ED_BK_RAW; }
    if (id == 1u) { *len = ed_bk_set_len; return (const uint8_t *)&ed_bk_set; }
    if (id >= 2u && id <= 5u) {
        uint32_t slot = id - 2u;
#if MELODEE_FLASH
        /* (Orange Cat 0.3) a slot is built from the project store into the staging when it is read (the LIST took
         * its length and CRC so: ed_bk_slots); the runtime copy there is then gone (a GET of it answers 5) */
        *len = ed_bk_slot_len[slot];
        if (*len && (ed_bk_slot != slot + 1u || ed_bk_slot_gen != proj_wire_gen)) {
            if (proj_read_slot(slot) != (int)BANK_STORE_SIZE) { *len = 0; return 0; }
            ed_bk_slot = (uint8_t)(slot + 1u); ed_bk_slot_gen = proj_wire_gen;
        }
        return ED_BK_RAW;                   /* (empty: a valid pointer, zero length) */
#else
        if (bank_valid(proj_bank_slot[slot], BANK_STORE_SIZE) &&
            !memcmp(proj_bank_slot[slot] + 8u, &proj_slot[slot], sizeof proj_slot[slot])) {
            *len = BANK_STORE_SIZE; return proj_bank_slot[slot];
        }
        if (project_used(slot)) *len = sizeof proj_slot[slot];
        return (const uint8_t *)&proj_slot[slot];
#endif
    }
    if(id==8u)return ED_BK_RAW;
    return 0;
}
static uint32_t ed_bk_capture(void)
{
    if (ed_flash_stop()) return 3;
    up_favorite_poll();
#if MELODEE_FLASH
    ed_bk_slots();                                  /* (Orange Cat 0.3: built in the staging, before the runtime's) */
#endif
    project_capture(&proj_scratch);
    if (!bank_pack(ED_BK_RAW, &proj_scratch, 1)) return 2;
    ed_bk_gen = ++proj_wire_gen;
#if MELODEE_FLASH
    ed_bk_rt_crc = st_crc32(ED_BK_RAW, BANK_STORE_SIZE);
    ed_bk_set.p = persist_saved;                    /* fields absent from this build survive */
#else
    memset(&ed_bk_set.p, 0, sizeof ed_bk_set.p);
#endif
    settings_export(&ed_bk_set.p);                  /* the record as it is stored: the template (tmpl) after the settings */
    ed_bk_set_len = sizeof ed_bk_set.p + (template_used() ? sizeof ed_bk_set.t : 0u);
    ed_bk_put = 0; ed_bk_valid = 1; ed_bk_usb = usb.resets;
#if OC_R3_FORMAT
    ebr_sess = 1;
#endif
    return 0;
}
static int ed_bk_panel_valid(const panel_t *p)
{
    uint32_t b = 0, e = 0;
    if (p->magic != PANEL_MAGIC) return 0;
    for (uint32_t i = 0; i < NB; i++) {
        if (p->btn[i] >= NB || (b & (1u << p->btn[i]))) return 0;
        b |= 1u << p->btn[i];
    }
    for (uint32_t i = 0; i < NE; i++) {
        if (p->enc[i] >= NE || (e & (1u << p->enc[i])) || (p->dir[i] != 1 && p->dir[i] != -1)) return 0;
        e |= 1u << p->enc[i];
    }
    return 1;
}
#if MELODEE_FLASH
static int ed_bk_preset_valid(uint32_t bank,const uint8_t *raw,uint32_t len)
{
    const up_bank_t *h=(const up_bank_t *)raw;
    if(len==sizeof(up_disk_bank_t) && h->magic==UP_DISK_MAGIC && h->rsize==sizeof(up_disk_rec_t) && h->nslot==UP_PER_BANK){
        const up_disk_bank_t *d=(const up_disk_bank_t *)raw;
        for(uint32_t j=0;j<UP_PER_BANK;j++){up_rec_t r;memset(&r,0,sizeof r);memcpy(&r,d->r[j].head,160u);memcpy(r.cz_extra,d->r[j].extra,46u);
            if(r.used && (r.ver==UP_VER || r.ver==UP_VER_GRID || r.ver==UP_VER_CZ || r.ver==UP_VER_DRUM || r.ver==UP_VER_DRUM_GRID || r.ver==UP_VER_FM || r.ver==UP_VER_P5 || r.ver==UP_VER_NATIVE_CZ) && !up_valid(&r))return 0;
        }
        return 1;
    }
    if(bank>=2u && bank<4u && len==3808u)return 1;
    if(bank<4u && len==3816u && h->magic==UP_BANK_MAGIC && h->rsize==238u && h->nslot==16u)return 1;
    if(bank>=4u && bank<8u)return len==sizeof(native_fm_t) && native_fm_valid((const native_fm_t *)raw);
    if(bank>=8u && bank<16u)return len==sizeof(cz_bank_t) && cz_bank_valid((const cz_bank_t *)raw);
    if(bank>=16u && bank<21u)return len==sizeof(p5_bank_t) && p5_bank_valid((const p5_bank_t *)raw,bank-16u);
    return 0;
}
#endif
static uint32_t ed_bk_commit_inner(void)
{
    uint8_t *raw = ED_BK_RAW;
    uint32_t obj;
    if (ed_bk_arr(ed_bk_id) && ed_bk_id != ED_BK_ARR_LIVE) {   /* (0.14.2) a record: streamed into the store already */
        int rc;
        if (!ed_bk_len) {
            if (ocs_delete(ed_bk_rec_type(ed_bk_id), ed_bk_rec_id(ed_bk_id))) { ed_bk_arr_rescan(); return 4; }
#if OC_R3_FORMAT
            if (ed_bk_rec_type(ed_bk_id) == OCS_PROJECT) ocj4_slot_erase_new(ed_bk_rec_id(ed_bk_id));   /* (an empty slot in the backup: empty here too) */
#endif
            ed_bk_arr_rescan();
            return 0;
        }
        if (ed_bk_pos != ed_bk_len || ~ed_bk_scrc != ed_bk_crc || !ed_bk_arr_valid(&ed_bk_stream)) { ocs_stream_drop(&ed_bk_stream); return 2; }
        rc = ocs_stream_commit(&ed_bk_stream);
        if (rc < -1) { uint8_t cursor = ocs_cursor; ocs_boot(); ocs_cursor = cursor; }   /* (as ocs_write_src: what is on the flash, read again) */
#if OC_R3_FORMAT
        /* (Release 3) a record of the format before replaces its slot whole, but only once it reads end to end (its pool objects are there too,
         * in whatever order the page sends them): the slot's Release 3 records go then (ocj4_restore_settle). A power cut never leaves the slot
         * empty: until then the slot has the project it had; with both records the next start (ocj4_convert_boot) sees the new one is not the old
         * one's conversion and converts the old one over it when it reads, or drops the old one when it does not. The slot then converts at the
         * next start or by CONVERT. */
        if (!rc) ocj4_restore_settle();            /* (any object of the format before: the page's order does not matter) */
#endif
        ed_bk_arr_rescan();
        return rc ? 4u : 0u;
    }
    if (ed_bk_pos != ed_bk_len || st_crc32(raw, ed_bk_len) != ed_bk_crc) return 2;
    if (ed_bk_id == ED_BK_ARR_LIVE) return ed_bk_live_apply(raw);
    if (ed_bk_id == 0u || (ed_bk_id >= 2u && ed_bk_id <= 5u)) {
        int full = bank_full(ed_bk_len);
        if (ed_bk_len && !(full ? bank_valid(raw, ed_bk_len) : proj_import(&proj_scratch, raw, (int)ed_bk_len))) return 2;
        if (ed_bk_id == 0u) {
            if (!ed_bk_len) return 2;
            up_boot();
#if MELODEE_FLASH
            if(up_migration_error)return 4;
#endif
            if (project_restore_runtime(&proj_scratch)) return 2;
            if (full) bank_restore(raw);
#if OC_ONE_AUTH
            fm1_irq_off(); recording_reconcile(); fm1_irq_on();   /* (round 11) the banks are back: the one reconcile of the restore */
#endif
            return 0;
        }
        if (ed_bk_len && !full) {
            proj_bound(&proj_scratch); chain_defaults(&proj_scratch.chain);
            if (!bank_pack(raw, &proj_scratch, 0)) return 2;
            ed_bk_len = BANK_STORE_SIZE;
        }
        if (proj_write_slot(ed_bk_id - 2u, raw, ed_bk_len)) return 4;
        if (proj_cur == ed_bk_id - 2u) { proj_cur = PROJ_NO_SLOT; proj_skip_armed = 1; }   /* (round 9: the music in RAM is not new music to autosave) */
        sync_reload = 1; ui.force = 1;
        return 0;
    } else if (ed_bk_id == 1u) {
#if MELODEE_AUDIO_PROFILE
        /* Guarded diagnostics restore preferences in RAM only. No template
         * replacement, persistence bookkeeping or flash commit is allowed. */
        if(AUDIO_LAB_GUARDED()){
            if(ed_bk_len!=sizeof(persist_t) || !settings_legacy_valid((const persist_t *)raw) ||
               !ed_bk_panel_valid(&((const persist_t *)raw)->panel))return 2;
            settings_import((persist_t *)raw,sizeof(persist_t));
            panel_init();settings_init();palette_set(settings.palette);
            return 0;
        }
#endif
        const persist_t *p = (const persist_t *)raw;
        const uint8_t *t = raw + sizeof *p;                  /* a template after the settings */
        uint32_t tl = ed_bk_len - sizeof *p, tm = 0, ts = 0;
        if (ed_bk_len >= sizeof *p + 8u) {
            memcpy(&ts, t + tl - 8u, 4);
            memcpy(&tm, t + tl - 4u, 4);
        }
        if (!(ed_bk_len>=sizeof *p && ed_bk_len<=sizeof ed_bk_set && p->magic==PERSIST_MAGIC &&
              p->ext.usb_off<=3u && p->ext.boot<=4u &&
              (ed_bk_len==sizeof *p || tmpl_blob_valid(t,tl))) || !palette_stored_ok(p->palette) ||
            p->lowcut > 2u || (p->zoom & 0xFFu) > 1u || (p->zoom > 0xFFu && (p->zoom & 0xFFFF0000u) != RECORD_PREF_TAG) || !hold_stored_ok(p->bold) || p->favorites.filter > 1u || !ed_bk_panel_valid(&p->panel)) return 2;
        if(tl && ed_bk_len>sizeof *p && !tmpl_blob_valid(t,tl))return 2;
        obj = OBJ_SETTINGS;
    } else if(ed_bk_preset_bank(ed_bk_id)<UP_BANKS){
        uint32_t bank=ed_bk_preset_bank(ed_bk_id);
#if MELODEE_FLASH
        if(ed_bk_len && !ed_bk_preset_valid(bank,raw,ed_bk_len))return 2;
        /* Legacy extension archives had a synthetic UPB1 header. */
        int compact=bank>=2u && bank<4u && ed_bk_len==3816u && ((const up_bank_t *)raw)->magic==UP_BANK_MAGIC;
        if(!flash_ok || st_save(up_obj(bank),compact?raw+8u:raw,compact?ed_bk_len-8u:ed_bk_len))return 4;
        up_cache_reset();native_cache_reset();up_gen++;sync_reload=1;ui.force=1;
#else
        if(ed_bk_len && (ed_bk_len!=sizeof(up_bank_t) || !up_bank_shape(ed_bk_len,((const up_bank_t *)raw)->rsize)))return 2;
        memset(up_cache_bank(bank),0,sizeof(up_bank_t));if(ed_bk_len)memcpy(up_cache_bank(bank),raw,ed_bk_len);
#endif
        return 0;
    } else if(ed_bk_id==8u){return ed_bk_len?2u:0u;
    } else return 1;
#if MELODEE_FLASH
    if (!flash_ok || st_save(obj, ed_bk_id == 17u || ed_bk_id == 18u ? raw + 8u : raw,
                            (ed_bk_id == 17u || ed_bk_id == 18u) && ed_bk_len ? ed_bk_len - 8u : ed_bk_len)) return 4;
#else
    (void)obj;
#endif
    if (ed_bk_id == 1u) {
        uint32_t ns = ed_bk_len > sizeof ed_bk_set.p ? sizeof ed_bk_set.p : ed_bk_len;
        memset(&ed_bk_set, 0, sizeof ed_bk_set);
        memcpy(&ed_bk_set.p, raw, ns);
        settings_import(&ed_bk_set.p, (int)ns);
        tmpl_take(raw + ns, ed_bk_len - ns);            /* (none in the backup: none now) */
        tmpl_dirty = 0;
        settings_old_accept();
        panel_init(); settings_init(); palette_set(settings.palette);
#if MELODEE_FLASH
        persist_saved = ed_bk_set.p; persist_pending = persist_repair = 0;
#endif
    }
    sync_reload = 1; ui.force = 1;
    return 0;
}
static uint32_t ed_bk_commit(void) { return ed_bk_commit_inner(); }
static uint32_t ed_bk_write(const uint8_t *a, uint32_t n)
{
    if (n < 2u || a[0] > 3u || (a[1] > 28u && !ed_bk_arr(a[1]))) return 1;
    if (transport_busy()) return 3;                 /* (0.12.2) a restore is REFUSED while the transport runs (it used to stop it): rc 3, as the other installs */
    if (a[0] == 0u) {
        if (n != 12u || a[6] > 15u || a[11] > 15u) return 1;
        uint32_t len = ed_bk_r32(a + 2);
        if (len > ED_BK_MAX || (a[1] == 0u && !bank_full(len) && len != sizeof(project_store_t)) ||
            (a[1] == 1u && len != sizeof(persist_t) && !tmpl_param_count(len - sizeof(persist_t))) ||
            (a[1] >= 2u && a[1] <= 5u && len && !bank_full(len) && len != sizeof(project_store_t)) ||
            (ed_bk_preset_bank(a[1])<UP_BANKS && len>3840u) ||
            (a[1]==8u && len) ||
            (a[1] == ED_BK_ARR_LIVE && len != ED_BK_LIVE_LEN) || (ed_bk_arr(a[1]) && len > ED_BK_REC_MAX)) return 1;
        ocs_stream_drop(&ed_bk_stream);           /* (0.14.2) a record begun before and never committed */
#if OC_R3_FORMAT
        ebr_on = 0;                                /* (a keyed record begun is dropped with it) */
#endif
        if (ed_bk_arr(a[1]) && a[1] != ED_BK_ARR_LIVE && len &&
            ocs_stream_begin(&ed_bk_stream, ed_bk_rec_type(a[1]), ed_bk_rec_id(a[1]), len)) return 4;   /* (no room in the store) */
        ed_bk_scrc = 0xFFFFFFFFu;
        ed_bk_valid = 0; ed_bk_put = 1; ed_bk_id = a[1]; ed_bk_len = len; ed_bk_gen = ++proj_wire_gen;
        ed_bk_crc = ed_bk_r32(a + 7); ed_bk_pos = 0;
        ed_bk_usb = usb.resets; ed_bk_ms = fm1_ms;
        return 0;
    }
    if (!ed_bk_put || ed_bk_id != a[1] || ed_bk_usb != usb.resets || fm1_ms - ed_bk_ms > 15000u ||
        ed_bk_gen != proj_wire_gen) {                 /* (a project save / load reused the staging RAM) */
        ed_bk_put = 0; ocs_stream_drop(&ed_bk_stream); return 5;
    }
    ed_bk_ms = fm1_ms;
    if (a[0] == 3u) { ed_bk_put = 0; ocs_stream_drop(&ed_bk_stream); return n == 2u ? 0u : 1u; }
    if (a[0] == 2u) {
        if (n != 2u) return 1;
        uint32_t rc = ed_bk_commit(); ed_bk_put = 0; return rc;
    }
    if (n < 9u || a[6] > 15u || ed_bk_r32(a + 2) != ed_bk_pos) return 1;
    uint32_t count = ed_unpack7(a + 7, n - 7u, ed_smp_buf, 256u);
    if (!count || count > ed_bk_len - ed_bk_pos) return 1;
    if (ed_bk_arr(ed_bk_id) && ed_bk_id != ED_BK_ARR_LIVE) {  /* (0.14.2) a record: into its sectors as it comes */
        ed_bk_scrc = ocs_crc_add(ed_bk_scrc, ed_smp_buf, count);
        if (ocs_stream_put(&ed_bk_stream, ed_smp_buf, count)) { ed_bk_put = 0; return 4; }
    } else memcpy(ED_BK_RAW + ed_bk_pos, ed_smp_buf, count);
    ed_bk_pos += count;
    return 0;
}
static int ed_backup_handle(uint32_t cmd, const uint8_t *a, uint32_t n)
{
    if (cmd == ED_BACKUP_LIST && n == 1u && a[0] == 1u) {   /* (0.14.2) the arrangement objects, in the session a plain LIST began */
        uint32_t rc = !ed_bk_valid || ed_bk_usb != usb.resets ? 5u : transport_busy() ? 3u : 0u;
        ed_b(1); ed_b(rc); ed_b(rc ? 0u : ED_BK_ARR_N);
        if (!rc) for (uint32_t id = ED_BK_ARR0; id <= ED_BK_ARR_LIVE; id++) {
            uint32_t len, crc;
            ed_bk_arr_info(id, &len, &crc);
            ed_b(id); ed_bk_u32(len); ed_bk_u32(crc);
            fm1_wdt_feed();
        }
        return 1;
    }
    if (cmd == ED_BACKUP_LIST) {
        uint32_t rc = n ? 1u : ed_bk_capture();
        ed_b(1); ed_b(rc); ed_b(rc ? 0u : ED_BK_N);
        if (!rc) for (uint32_t i = 0; i < ED_BK_N; i++) {
            uint32_t len;
            uint32_t crc;
#if MELODEE_FLASH
            if (ED_BK_IDS[i] >= 2u && ED_BK_IDS[i] <= 5u) {   /* (Orange Cat 0.3: as ed_bk_slots built them) */
                len = ed_bk_slot_len[ED_BK_IDS[i] - 2u]; crc = ed_bk_slot_crc[ED_BK_IDS[i] - 2u];
                ed_b(ED_BK_IDS[i]); ed_bk_u32(len); ed_bk_u32(crc);
                continue;
            }
#endif
            const uint8_t *p = ed_bk_object(ED_BK_IDS[i], &len);
#if MELODEE_FLASH
            if (ed_bk_source_obj < OBJ_COUNT) crc = ed_bk_source_crc;
            else
#endif
                crc = st_crc32(p, len);
            ed_b(ED_BK_IDS[i]); ed_bk_u32(len); ed_bk_u32(crc);
            fm1_wdt_feed();
        }
        return 1;
    }
    if (cmd == ED_BACKUP_GET && n && ed_bk_arr(a[0])) {     /* (0.14.2) an arrangement object, read where it is */
        uint32_t len, off = n >= 6u ? ed_bk_r32(a + 1) : 0u;
        uint32_t count = n == 8u ? (uint32_t)a[6] | (uint32_t)a[7] << 7 : 0u, rc;
        rc = !ed_bk_valid || ed_bk_usb != usb.resets ? 5u : transport_busy() ? 3u : n != 8u || a[5] > 15u || !count || count > 256u ? 1u : 0u;
        if (!rc) {
            len = a[0] == ED_BK_ARR_LIVE ? ED_BK_LIVE_LEN : 0u;
            if (a[0] != ED_BK_ARR_LIVE) { ocs_rec_t r; if (!ocs_open(ed_bk_rec_type(a[0]), ed_bk_rec_id(a[0]), &r)) len = r.total; }
            if (off > len || count > len - off) rc = 1;
            else if (ed_bk_arr_read(a[0], off, ed_smp_buf, count)) rc = 4;
        }
        ed_b(a[0]); ed_b(rc); ed_bk_u32(off); ed_b(rc ? 0u : count & 127u); ed_b(rc ? 0u : count >> 7);
        if (!rc) ed_bk_pack(ed_smp_buf, count);
        return 1;
    }
    if (cmd == ED_BACKUP_GET) {
        uint32_t len = 0, off = n >= 6u ? ed_bk_r32(a + 1) : 0u;
        uint32_t count = n == 8u ? (uint32_t)a[6] | (uint32_t)a[7] << 7 : 0u;
        const uint8_t *p = n ? ed_bk_object(a[0], &len) : 0;
        uint32_t rc = (!ed_bk_valid || ed_bk_usb != usb.resets || (n && !a[0] && ed_bk_gen != proj_wire_gen)) ? 5u : transport_busy() ? 3u :
            n != 8u || a[5] > 15u || !p || !count || count > 256u || off > len || count > len - off ? 1u : 0u;
        uint32_t data_off = off;
#if MELODEE_FLASH
        if (!rc && ed_bk_source_obj < OBJ_COUNT) {
            if (st_read_range(ed_bk_source_obj, ed_bk_source_copy, off, ed_smp_buf, count)) rc = 4;
            p = ed_smp_buf; data_off = 0;
        }
#endif
        ed_b(n ? a[0] : 127u); ed_b(rc); ed_bk_u32(off); ed_b(rc ? 0u : count & 127u); ed_b(rc ? 0u : count >> 7);
        if (!rc) ed_bk_pack(p + data_off, count);
        return 1;
    }
    if (cmd == ED_BACKUP_PUT) {
        uint32_t rc = ed_bk_write(a, n);
        ed_b(n ? a[0] : 127u); ed_b(n >= 2u ? a[1] : 127u); ed_b(rc);
        return 1;
    }
    return 0;
}

#if OC_R3_FORMAT
/* ------------------------------------------------------------------ the records by type and id (Release 3, UI_BUILD.md "Part 6") --- */
/* The extension commands 135 LISTR, 136 GETR, 137 PUTR (extension bytes 7, 8, 9; web/EDITOR_PROTOCOL.md "Records by type and id"). A record is the store's
 * own (OCS_PROJECT or OCS_POOL, any id), read and written as it is: the arrangement objects 64..83 name the format before's records by fixed ids and a
 * Release 3 unit has none (its records have ids with bit 15). Same session rule as the other backup commands (a plain LIST began it: rc 5 otherwise),
 * refused while the transport runs (rc 3).
 *   7 LISTR  start lo, start hi        -> rc, start lo, start hi, n (0..16), more (1 when there are more), then n x [type, id (3 x 7 bits), size (5 x 7), CRC (5 x 7)]
 *   8 GETR   key (4), offset (5), count (2, 1..256) -> key (4), rc, offset (5), count (2), pack7 data
 *   9 PUTR   op, key (4), then: 0 begin: size (5), CRC (5) | 1 data: offset (5), pack7 | 2 commit | 3 abort | 4 finish | 5 clear -> op, rc
 * key = type (1 project, 2 pool), id lo, id mid, id hi (2 bits). Listed: every project and pool record but the scratch slot's chunks. rc of PUTR: 0 done;
 * 1 refused (arguments, a type or id the format does not have, a size past a record); 2 the CRC or the shape of the record (nothing is changed);
 * 3 the transport runs; 4 no room or a flash error; 5 stale (no begin, a USB reset, 15 s, the staging reused); 6 a chunk of a slot whose record names that
 * very chunk under the same generation (writing it would change that project under its own record: clear the slot first, op 5, and the page says so).
 * 4 finish (once, after the last record): the chunks no record names are swept and the live pool is read again from its slot's record. 5 clear: the slot's
 * Release 3 record and chunks go (the key names the slot's record). The chunks of a restore come first, then the record. */
enum { EBR_LIST = 7, EBR_GET = 8, EBR_PUT = 9 };
static uint8_t ebr_type;
static uint32_t ebr_id, ebr_len, ebr_crc, ebr_pos, ebr_scrc, ebr_ms, ebr_usb;
static uint32_t ebr_key_id(const uint8_t *k) { return (uint32_t)k[1] | (uint32_t)k[2] << 7 | (uint32_t)(k[3] & 3u) << 14; }
/* a (type, id) this format has */
static int ebr_key_ok(const uint8_t *k)
{
    uint32_t id = ebr_key_id(k);
    if (k[3] > 3u) return 0;
    if (k[0] == OCS_PROJECT) return id < 4u || (id >= OCJ4_RID(0) && id <= OCJ4_RID(3));
    if (k[0] != OCS_POOL) return 0;
    if (!(id & 0x8000u)) return id < 16u;
    return ((id >> 9) & 7u) < 4u && (id & 255u) >= 1u;
}
/* the n-th (from 0) distinct, complete project or pool record in the store's header order. 1 when there is one */
static int ebr_nth(uint32_t n, uint32_t *type, uint32_t *id)
{
    uint32_t seen = 0;
    for (uint32_t s = 0; s < OCS_NSECT; s++) {
        const ocs_hdr_t *h = &ocs_hdr[s];
        int dup = 0;
        if (h->magic != OCS_MAGIC || h->part != 0 || (h->type != OCS_PROJECT && h->type != OCS_POOL)) continue;
        if (h->type == OCS_POOL && (h->id & 0x8000u) && ((h->id >> 9) & 7u) == OCP_SCRATCH) continue;
        for (uint32_t t = 0; t < s && !dup; t++)
            dup = ocs_hdr[t].magic == OCS_MAGIC && ocs_hdr[t].part == 0 && ocs_hdr[t].type == h->type && ocs_hdr[t].id == h->id;
        if (dup || !ocs_exists(h->type, h->id)) continue;
        if (seen++ == n) { *type = h->type; *id = h->id; return 1; }
    }
    return 0;
}
static void ebr_key_out(uint32_t type, uint32_t id) { ed_b(type); ed_b(id & 127u); ed_b((id >> 7) & 127u); ed_b(id >> 14); }
static void ebr_info(uint32_t type, uint32_t id, uint32_t *len, uint32_t *crc)
{
    ocs_rec_t r;
    uint32_t c = 0xFFFFFFFFu;
    *len = ocs_open(type, id, &r) ? 0u : r.total;
    for (uint32_t off = 0; off < *len; off += 256u) {
        uint32_t k = *len - off > 256u ? 256u : *len - off;
        if (ocs_at(&r, off, ed_smp_buf, k)) { *len = 0; break; }
        c = ocs_crc_add(c, ed_smp_buf, k);
    }
    *crc = *len ? ~c : 0u;
}
/* the record the stream wrote reads as its kind must (before its commit) */
static int ebr_valid(const ocs_stream_t *w, uint32_t type, uint32_t id)
{
    ocs_rec_t r;
    uint8_t h[OCJ4_HLEN], t[3];
    r.parts = (uint8_t)w->parts; r.total = w->len;
    memcpy(r.sect, w->use, sizeof r.sect);
    if (type == OCS_PROJECT && (id & 0x8000u)) {          /* OCJ4: its magic, header, length field, and sections that end where the record does */
        uint32_t pos;
        uint8_t s[8];
        if (r.total < OCJ4_HLEN || ocs_at(&r, 0, h, OCJ4_HLEN) || rd32(h) != OCJ4_MAGIC || h[5] != OCJ4_HLEN || rd32(h + 8) != r.total - OCJ4_HLEN) return 0;
        for (pos = OCJ4_HLEN; pos + 8u <= r.total; pos += 8u + rd32(s + 4)) {
            if (ocs_at(&r, pos, s, 8) || rd32(s + 4) > r.total - pos - 8u) return 0;
        }
        return pos == r.total;
    }
    if (type == OCS_PROJECT) { uint32_t hsz; return ocj_head_load(&r, &hsz) == 0; }
    if (!(id & 0x8000u)) return ed_bk_arr_valid(w);          /* the format before's chunk: its fixed table */
    {   /* a chunk: n, then n x (phrase + 1, length lo, length hi), then the phrases: the sizes add up */
        uint32_t n, sum = 1u;
        if (r.total < 1u || ocs_at(&r, 0, t, 1)) return 0;
        n = t[0];
        for (uint32_t e = 0; e < n; e++) {
            if (ocs_at(&r, 1u + 3u * e, t, 3) || !t[0]) return 0;
            sum += 3u + (uint32_t)t[1] + ((uint32_t)t[2] << 8);
        }
        return sum == r.total && n > 0u;
    }
}
/* 1 when writing chunk (slot, gen, c) would change a chunk its slot's record names under that very generation */
static int ebr_chunk_in_use(uint32_t id)
{
    uint32_t slot = (id >> 9) & 7u, c = id & 255u;
    if (!(id & 0x8000u) || slot >= 4u) return 0;
    if (ocj4_phrd_read(slot, &ocp_sdir, ocp_snamed, 0, 0)) return 0;
    return ocp_bit(ocp_snamed, c) && ocp_bit(ocp_sdir.gen, c) == ((id >> 8) & 1u) && ocs_exists(OCS_POOL, id);
}
static int ebr_session(void) { return ebr_sess && ed_bk_usb == usb.resets; }
static int ed_bkr_handle(uint32_t cmd, const uint8_t *a, uint32_t n)
{
    if (cmd == ED_BKR_LIST) {
        uint32_t start, type, id, k = 0, rc;
        if (n != 2u || a[0] > 127u || a[1] > 127u) return 0;
        start = (uint32_t)a[0] | (uint32_t)a[1] << 7;
        rc = !ebr_session() ? 5u : transport_busy() ? 3u : 0u;
        ed_b(rc); ed_b(a[0]); ed_b(a[1]);
        if (rc) { ed_b(0); ed_b(0); return 1; }
        { uint32_t cnt = 0;
          while (cnt < 16u && ebr_nth(start + cnt, &type, &id)) cnt++;
          ed_b(cnt); ed_b(ebr_nth(start + cnt, &type, &id) ? 1u : 0u);
          for (k = 0; k < cnt; k++) {
              uint32_t len, crc;
              (void)ebr_nth(start + k, &type, &id);
              ebr_info(type, id, &len, &crc);
              ebr_key_out(type, id); ed_bk_u32(len); ed_bk_u32(crc);
              fm1_wdt_feed();
          } }
        return 1;
    }
    if (cmd == ED_BKR_GET) {
        uint32_t off, count, rc, len = 0;
        ocs_rec_t r;
        if (n != 11u || a[0] > 3u || a[1] > 127u || a[2] > 127u || a[3] > 3u) return 0;
        off = ed_bk_r32(a + 4); count = (uint32_t)a[9] | (uint32_t)a[10] << 7;
        rc = !ebr_session() ? 5u : transport_busy() ? 3u : !ebr_key_ok(a) || a[8] > 15u || !count || count > 256u ? 1u : 0u;
        if (!rc) {
            if (!ocs_open(a[0], ebr_key_id(a), &r)) len = r.total;
            if (off > len || count > len - off) rc = 1;
            else if (ocs_at(&r, off, ed_smp_buf, count)) rc = 4;
        }
        ed_b(a[0]); ed_b(a[1]); ed_b(a[2]); ed_b(a[3]); ed_b(rc); ed_bk_u32(off); ed_b(rc ? 0u : count & 127u); ed_b(rc ? 0u : count >> 7);
        if (!rc) ed_bk_pack(ed_smp_buf, count);
        return 1;
    }
    if (cmd == ED_BKR_PUT) {
        uint32_t op, rc = 0;
        if (n < 5u || a[0] > 5u || a[1] > 127u || a[2] > 127u || a[3] > 127u || a[4] > 127u) return 0;
        op = a[0];
        if (op == 0u || op == 1u || op == 2u || op == 3u || op == 5u) { if (!ebr_key_ok(a + 1) && op != 3u) rc = 1u; }
        if (!rc && ((op == 0u && n != 15u) || (op == 1u && n < 11u) || ((op == 2u || op == 3u || op == 4u || op == 5u) && n != 5u))) rc = 1u;
        if (!rc && !ebr_session()) rc = 5u;
        if (!rc && transport_busy()) rc = 3u;
        if (!rc && op == 5u) {                                  /* clear: the slot's Release 3 record and chunks */
            uint32_t id = ebr_key_id(a + 1);
            if (a[1] != OCS_PROJECT || id < OCJ4_RID(0) || id > OCJ4_RID(3)) rc = 1u;
            else { ocj4_slot_erase_new(id - OCJ4_RID(0)); ed_bk_arr_rescan(); }
        } else if (!rc && op == 4u) {                           /* finish */
            for (uint32_t s = 0; s < 4u; s++) ocp_r3_sweep(s);
            if (ocp.dir.slot < 4u) ocp_r3_switch(ocp.dir.slot);
            ocj4_old_c = 0;
            ed_bk_arr_rescan();
        } else if (!rc && op == 0u) {
            uint32_t len = ed_bk_r32(a + 5), id = ebr_key_id(a + 1);
            if (len < 1u || len > ED_BK_REC_MAX || a[9] > 15u || a[14] > 15u) rc = 1u;
            else if (a[1] == OCS_POOL && ebr_chunk_in_use(id)) rc = 6u;
            else {
                ocs_stream_drop(&ed_bk_stream); ed_bk_put = 0;
                if (ocs_stream_begin(&ed_bk_stream, a[1], id, len)) rc = 4u;
                else { ebr_on = 1; ebr_type = a[1]; ebr_id = id; ebr_len = len; ebr_crc = ed_bk_r32(a + 10); ebr_pos = 0; ebr_scrc = 0xFFFFFFFFu; ebr_ms = fm1_ms; ebr_usb = usb.resets; }
            }
        } else if (!rc) {                                       /* 1 data, 2 commit, 3 abort: a record begun, the same key, fresh */
            if (!ebr_on || ebr_usb != usb.resets || fm1_ms - ebr_ms > 15000u || a[1] != ebr_type || ebr_key_id(a + 1) != ebr_id) {
                ebr_on = 0; ocs_stream_drop(&ed_bk_stream); rc = op == 3u ? 0u : 5u;
            } else if (op == 3u) { ebr_on = 0; ocs_stream_drop(&ed_bk_stream); }
            else if (op == 1u) {
                uint32_t off = ed_bk_r32(a + 5), count;
                if (a[9] > 15u || off != ebr_pos) rc = 1u;
                else if (!(count = ed_unpack7(a + 10, n - 10u, ed_smp_buf, 256u)) || count > ebr_len - ebr_pos) rc = 1u;
                else { ebr_ms = fm1_ms; ebr_scrc = ocs_crc_add(ebr_scrc, ed_smp_buf, count);
                       if (ocs_stream_put(&ed_bk_stream, ed_smp_buf, count)) { ebr_on = 0; ocs_stream_drop(&ed_bk_stream); rc = 4u; } else ebr_pos += count; }
            } else {                                            /* commit */
                if (ebr_pos != ebr_len || ~ebr_scrc != ebr_crc || !ebr_valid(&ed_bk_stream, ebr_type, ebr_id)) { ebr_on = 0; ocs_stream_drop(&ed_bk_stream); rc = 2u; }
                else {
                    int r = ocs_stream_commit(&ed_bk_stream);
                    if (r < -1) { uint8_t cursor = ocs_cursor; ocs_boot(); ocs_cursor = cursor; }
                    ebr_on = 0;
                    ocj4_old_c = 0;
                    if (!r && ebr_type == OCS_PROJECT && (ebr_id & 0x8000u)) (void)ocs_delete(OCS_PROJECT, ebr_id & 3u);   /* (a Release 3 record restored is the slot's wish: an old record there goes, settle must not undo it) */
                    ocj4_restore_settle();
                    ed_bk_arr_rescan();
                    rc = r ? 4u : 0u;
                }
            }
        }
        ed_b(op); ed_b(rc);
        return 1;
    }
    return 0;
}
#endif
