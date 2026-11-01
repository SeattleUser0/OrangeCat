/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 * Orange Cat, Release 3 (branch r3-format, OC_R3_FORMAT): the conversion of the projects of the format before.
 * Included at the end of oc_ocj4.c. The design is docs/orangecat/PROJECT_FORMAT_R3.md section 9 and docs/orangecat/UI_BUILD.md "Part 3"
 * (the choices, and the review's findings).
 *
 * What it does
 *   The format before kept a project as a record OCS_PROJECT id 0..3 (OCJ3, or OCJ2) and its phrases in a pool shared by the four
 *   slots (OCS_POOL ids 0..15, four phrases a chunk, version 1). The first start of this firmware, and the CONVERT of the PROJECT page, turn a
 *   slot into a Release 3 project, one slot at a time:
 *     1. the old record is read as the FBKK the rest of the firmware speaks (ocj3_read, the reader of the format before, with the pool read
 *        through the old chunk table: ocp_rd_dir 0);
 *     2. the phrases the project uses (its banks' and its chains') are the ones to copy: each is parsed (ocp_check: the same parser a load
 *        uses, so its knob-macro events are dropped and its motion ids mapped by np) and its NEW bytes' length computed;
 *     3. the chunks are planned (first fit, as the live writer does on an empty pool), the room checked before a byte is written, the
 *        phrases streamed old to new (version 1 to 2, np = P_COUNT: byte for byte what a load followed by a keep writes), generation 0;
 *     4. the project record is built from the old record's project part and the old lists (ocj4_build) and written: the commit;
 *     5. the old record is erased (only after the new one reads back), then the old pool chunks no remaining old record names.
 *   A slot that does not fit stays as the old record (OLD FORMAT); the others go on. A power cut leaves the old record, or both (the new one
 *   wins at the next start and the old is erased once the new reads end to end), never neither.
 *
 * Revisions:
 *   Part 3 (November 1, 2026): first version.
 */
#define CV_N 64u                               /* the format before's phrases */

/* ------------------------------------------------------------ the old reader, wrapped --- */
#if MELODEE_FLASH
#define OCJ4_FLASH_OK flash_ok                  /* (the unit's store is on the flash: storage_hw.c) */
#else
#define OCJ4_FLASH_OK 0                         /* (a RAM-only build, the editor's host test: nothing to convert) */
#endif
/* slot's old record as an FBKK into raw: ocj3_read with the pool read through the old chunk table (ocp_rd_dir 0), restored on every return.
 * BANK_STORE_SIZE, or -1 (no record, or one that does not read: exactly what the old firmware could not load either) */
static int ocj4_old_read(uint32_t slot, uint8_t *raw)
{
    int n;
    ocp_rd_dir = 0;
    n = ocj3_read(slot, raw);
    ocp_rd_dir = &ocp.dir;
    return n;
}

/* ------------------------------------------------------------ the repack: version 1 to 2, streaming --- */
/* One phrase of the format before, parsed by ocp_check (the load's own parser) and written as what the live writer would write for it:
 * version 2, np = P_COUNT, 5-byte motion events (the parse mapped the ids and dropped the knob macros'), the notes as they are. */
static struct {
    ocs_rec_t rec;
    ocp_rd_t rd;
    ocp_parsed_t p;
    uint32_t notes_at, id, phase, i, nsteps, nnotes, mask_lo, mask_hi;
    uint64_t mask;
    uint8_t item[OCP_HEAD], ipos, ilen;
} cv_up __attribute__((section(".pool")));

static uint32_t cv_new_len(const ocp_parsed_t *p, uint64_t *mask_out)
{
    uint64_t mask = 0;
    uint32_t i, n = 0;
    for (i = 0; i < NSTEP; i++)
        if (ocp_step_used(&p->pat.step[i])) { mask |= (uint64_t)1 << i; n++; }
    if (mask_out) *mask_out = mask;
    if (!n && !p->mot.count && !p->nnotes) return 0;
    return OCP_HEAD + n * OCP_STEP + 1u + p->mot.count * 5u + 2u + p->nnotes * OCP_NOTE;
}
/* phrase id (0-based) opened in the old pool and parsed: its new length (0 empty), -1 it does not read. *old_hash: the CRC-32 of its old
 * bytes (the freeze table's hash then) */
static int cv_open(uint32_t id, uint32_t *old_hash)
{
    int rc = ocp_open_pattern(id, &cv_up.rec, &cv_up.rd);
    if (rc < 0) return -1;
    if (!rc) { *old_hash = 0; return 0; }
    *old_hash = ocp_hash_of(&cv_up.rec, cv_up.rd.off, cv_up.rd.end - cv_up.rd.off);
    if (ocp_check(id, 0, &cv_up.rec, &cv_up.rd, &cv_up.p, &cv_up.notes_at)) return -1;
    return (int)cv_new_len(&cv_up.p, 0);
}
static int ocj4_up_begin(uint32_t id, uint32_t len)
{
    uint32_t h;
    int n = cv_open(id, &h);
    if (n < 0 || (uint32_t)n != len) return -1;
    cv_new_len(&cv_up.p, &cv_up.mask);
    cv_up.id = id; cv_up.phase = 0; cv_up.i = 0; cv_up.ipos = cv_up.ilen = 0;
    return 0;
}
/* the next item (the head, a step, a count, an event, a note) into cv_up.item */
static void cv_item(void)
{
    uint8_t *b = cv_up.item;
    cv_up.ipos = 0; cv_up.ilen = 0;
    while (!cv_up.ilen && cv_up.phase < 6u) {
        switch (cv_up.phase) {
        case 0:
            b[0] = 2; b[1] = (uint8_t)(cv_up.p.flags & 1u); ocp_put16(b + 2, P_COUNT);
            for (uint32_t j = 0; j < 4u; j++) ocp_put16(b + 4 + 2u * j, (uint16_t)cv_up.p.pat.timing[j]);
            for (uint32_t j = 0; j < 8u; j++) b[12 + j] = (uint8_t)(cv_up.mask >> (8u * j));
            cv_up.ilen = OCP_HEAD; cv_up.phase = 1; cv_up.i = 0;
            break;
        case 1:
            for (; cv_up.i < NSTEP && !((cv_up.mask >> cv_up.i) & 1u); cv_up.i++)
                ;
            if (cv_up.i == NSTEP) { cv_up.phase = 2; break; }
            step_snapshot_put((step_snapshot_t *)b, &cv_up.p.pat.step[cv_up.i++]);
            cv_up.ilen = (uint8_t)OCP_STEP;
            break;
        case 2:
            b[0] = (uint8_t)cv_up.p.mot.count; cv_up.ilen = 1; cv_up.phase = 3; cv_up.i = 0;
            break;
        case 3:
            if (cv_up.i == cv_up.p.mot.count) { cv_up.phase = 4; break; }
            b[0] = cv_up.p.mot.event[cv_up.i].place & 63u;
            ocp_put16(b + 1, cv_up.p.mot.event[cv_up.i].param);
            ocp_put16(b + 3, (uint16_t)cv_up.p.mot.event[cv_up.i].value);
            cv_up.ilen = 5; cv_up.i++;
            break;
        case 4:
            ocp_put16(b, cv_up.p.nnotes); cv_up.ilen = 2; cv_up.phase = 5; cv_up.i = 0;
            ocp_notes_rewind(&cv_up.rd, cv_up.notes_at);
            break;
        case 5:
            if (cv_up.i == cv_up.p.nnotes) { cv_up.phase = 6; break; }
            {
                recorded_note_t r;
                if (ocp_rd(&cv_up.rd, &r, OCP_NOTE)) { cv_up.phase = 6; break; }
                r.owner &= 224u;                     /* (as the live writer: the owner's track and bank bits are the bank's it loads into) */
                memcpy(b, &r, OCP_NOTE);
                cv_up.ilen = (uint8_t)OCP_NOTE; cv_up.i++;
            }
            break;
        }
    }
}
static int ocj4_up_get(uint8_t *dst, uint32_t n)
{
    while (n) {
        uint32_t k;
        if (cv_up.ipos == cv_up.ilen) {
            cv_item();
            if (!cv_up.ilen) return -1;
        }
        k = (uint32_t)(cv_up.ilen - cv_up.ipos);
        if (k > n) k = n;
        memcpy(dst, cv_up.item + cv_up.ipos, k);
        cv_up.ipos = (uint8_t)(cv_up.ipos + k); dst += k; n -= k;
    }
    return 0;
}

/* ------------------------------------------------------------ the freeze table, split --- */
static uint32_t cv_ohash[CV_N];                   /* each used phrase's old CRC-32 (the freeze table's hash then) */
static uint16_t cv_len[CV_N];                     /* each phrase's new length (0: empty, or not copied) */
#if OCJ4_FRZ
_Static_assert(sizeof(frzp_t) == 32u, "the old freeze table's entry");
#define CV_FRZ_REC (CV_N * 32u + FZD_N_MAX * 32u + 8u)    /* the unit's OCS_FREEZE record: frzp[64], then frzd (16 folder names, the renames) */
static ocs_rec_t cv_frz;
static int cv_frz_ok;
static char cv_fname[32];
static const char *cv_frz_name_of(uint32_t idx)
{
    cv_fname[0] = 0;
    if (cv_frz_ok && idx < FZD_N_MAX && !ocs_at(&cv_frz, CV_N * 32u + idx * 32u, cv_fname, 32)) cv_fname[31] = 0;
    return cv_fname;
}
static int cv_frz_get_entry(uint32_t i, frzp_t *f)
{
    ocs_rec_t r;
    ocp_rd_t rd;
    uint32_t nl;
    if (!cv_frz_ok || i >= CV_N || !cv_len[i] || ocs_at(&cv_frz, i * 32u, f, 32)) return 0;
    if (!f->take || f->ver != FZP_VER || f->trk >= 4u || f->take >= 1000u || f->frames < 256u || f->dir >= FZD_N_MAX || f->want >= FZD_N_MAX) return 0;
    if (!cv_frz_name_of(f->dir)[0]) return 0;
    if (!cv_frz_name_of(f->want)[0]) return 0;
    f->name[13] = 0;
    for (nl = 0; nl < 14u && f->name[nl]; nl++)   /* a take's file name: 1 to 13 characters, none a path separator */
        if (f->name[nl] < 32 || f->name[nl] == '/' || f->name[nl] == '\\') return 0;
    if (!nl || nl >= 14u) return 0;
    if (f->hash != cv_ohash[i]) return 0;          /* the phrase changed since it was frozen: the freeze was already inert */
    if (ocp_r3_open(&ocj_dir, i, &r, &rd) != 1) return 0;
    f->hash = ocp_hash_of(&r, rd.off, rd.end - rd.off);   /* (the hash includes the version byte: recomputed from the version 2 bytes) */
    return 1;
}
#endif

/* ------------------------------------------------------------ the old pool, after the conversion --- */
/* some slot still has a record of the format before */
/* (cached in ocj4_old_c, oc_ocj4.c: the card code asks on every pass of the main loop while a freeze change waits) */
static int ocj4_old_exists(void)
{
    if (!ocj4_old_c) {
        ocj4_old_c = 2;
        for (uint32_t s = 0; s < 4u; s++) if (ocs_exists(OCS_PROJECT, s)) ocj4_old_c = 1;
    }
    return ocj4_old_c == 1;
}
/* every old pool chunk (ids 0..15) that no remaining old project record names erased; an old record that does not read keeps them all */
static void ocj4_old_chunks_cleanup(void)
{
    uint32_t keep_all = 0, nm0 = 0, nm1 = 0, c;
    ocj3_scan();                                   /* ocj.names[s]: the phrases slot s's old record names (banks and chains) */
    for (uint32_t s = 0; s < 4u; s++) {
        ocs_rec_t r;
        uint32_t hsz;
        if (!ocs_exists(OCS_PROJECT, s)) continue;
        if (ocs_open(OCS_PROJECT, s, &r) || ocj_head_load(&r, &hsz)) keep_all = 1;
        else { nm0 |= ocj.names[s][0]; nm1 |= ocj.names[s][1]; }
    }
    for (c = 0; c < OCP_CHUNKS; c++) {
        uint32_t bits = (c < 8u ? nm0 >> (4u * c) : nm1 >> (4u * (c - 8u))) & 15u;
        if (keep_all || bits || !ocs_exists(OCS_POOL, c)) continue;
        (void)ocs_delete(OCS_POOL, c);
        OC_FEED();
    }
}

/* ------------------------------------------------------------ one slot --- */
static char ocj4_conv_msg[24];                     /* the boot's summary, shown after the project loads (ocj4_conv_notice) */
static uint32_t ocj4_conv_done, ocj4_conv_left;

/* the old record's mark (never 0): its first part's payload CRC and its length. A conversion writes it into the record it makes (CVRT), so a
 * start that finds both records can tell a conversion a cut stopped at the last step (the mark is the old record's) from a restore of an old
 * record over a slot that has a record of its own (the restore wrote the old one last: it is the newer wish) */
static uint32_t ocj4_old_mark(uint32_t s)
{
    ocs_rec_t r;
    if (ocs_open(OCS_PROJECT, s, &r)) return 0u;
    return (ocs_hdr[r.sect[0]].crc ^ r.total * 0x9E3779B1u) | 1u;
}

/* slot s (a record of the format before at id s) -> a Release 3 project. 0 converted; 1 no room (*kb_short: the KB it needs more); 2 it does not
 * convert (the old record does not read, or a flash error): the old record is left as it was */
static int ocj4_convert_slot_body(uint32_t s, uint32_t *kb_short)
{
    uint8_t *raw = proj_wire_borrow();
    project_t *q = &proj_scratch;
    static ocj4_meta_t meta __attribute__((section(".pool")));
    uint8_t used[CV_N], plan_chunk[CV_N];
    uint32_t id, c, fill, parts = 0, nchunks, need, k, b, e, hsz, rlen;
    int n, rc;
    s &= 3u;
    if (kb_short) *kb_short = 0;
    if (!OCJ4_FLASH_OK || (n = ocj4_old_read(s, raw)) != (int)BANK_STORE_SIZE) return 2;
    /* the old lists as the old firmware would have kept them: a chain cell past its 64 phrases is none */
    ocp_rd_dir = 0;                                /* (from here the pool read is the old one's; the wrapper restores it) */
    for (c = 0; c < OCL_NCHAIN; c++) for (e = 0; e < OCL_NENT; e++) if (ocj_l.chain[c][e][0] > CV_N) ocj_l.chain[c][e][0] = 0;
    memcpy(&hsz, ocj_h.fn, 4);
    if (hsz == BANK_FN_MAGIC) { memcpy(q->fm6_fn, ocj_h.fn + 4, sizeof q->fm6_fn); q->fm6_fn_ok = 1; }   /* FM6's functions: the FBKK's block */
    /* 2. the phrases the project uses */
    memset(used, 0, sizeof used);
    for (k = 0; k < NTRK; k++) for (b = 0; b < NPAT; b++) if (ocj_h.pid[k][b] && ocj_h.pid[k][b] <= CV_N) used[ocj_h.pid[k][b] - 1u] = 1;
    for (c = 0; c < OCL_NCHAIN; c++) for (e = 0; e < OCL_NENT; e++) if (ocj_l.chain[c][e][0]) used[ocj_l.chain[c][e][0] - 1u] = 1;
    memset(cv_len, 0, sizeof cv_len);
    memset(cv_ohash, 0, sizeof cv_ohash);
    memset(&ocj_dir, 0, sizeof ocj_dir);
    ocj_dir.slot = (uint8_t)s;
    for (id = 0; id < CV_N; id++) {
        if (!used[id]) continue;
        n = cv_open(id, &cv_ohash[id]);
        if (n < 0) {                               /* a chain-only phrase that does not read: the step names none (the old firmware refused it as well) */
            for (c = 0; c < OCL_NCHAIN; c++) for (e = 0; e < OCL_NENT; e++) if (ocj_l.chain[c][e][0] == id + 1u) ocj_l.chain[c][e][0] = 0;
            used[id] = 0;
            continue;
        }
        cv_len[id] = (uint16_t)n;
    }
    /* 3. the plan: chunks of one part, first fit in id order; a phrase larger than a part has a chunk of its own */
    fill = 1; c = 1;
    for (id = 0; id < CV_N; id++) {
        if (!cv_len[id]) { plan_chunk[id] = 0; continue; }
        if (fill > 1u && fill + 3u + cv_len[id] > OCP_PART) { parts += ocp_parts_of(fill); c++; fill = 1; }
        plan_chunk[id] = (uint8_t)c;
        fill += 3u + cv_len[id];
        if (fill > OCP_PART) { parts += ocp_parts_of(fill); c++; fill = 1; }
    }
    if (fill > 1u) { parts += ocp_parts_of(fill); nchunks = c; } else nchunks = c - 1u;
    need = parts + ocp_parts_of(OCJ4_MAX);
    if (ocs_free_count() < need) {
        if (kb_short) *kb_short = ((need - ocs_free_count()) * OCS_PAYLOAD_MAX + 1023u) / 1024u;
        return 1;
    }
    /* 4. the chunks, generation 0, unnamed until the record */
    for (c = 1; c <= nchunks; c++) {
        uint32_t any = 0;
        memset(&ocp_cw, 0, sizeof ocp_cw);
        ocp_cw.src.get = ocp_cw_get;
        memset(&ocp_otab, 0, sizeof ocp_otab);
        memset(&ocp_ntab, 0, sizeof ocp_ntab);
        for (id = 0; id < CV_N; id++)
            if (plan_chunk[id] == c) { if (ocp_ntab_add(id, cv_len[id], OCP_SRC_REPACK)) return 2; ocj_dir.chunk[id] = (uint8_t)c; any = 1; }
        if (!any) continue;
        rc = ocs_write_src(OCS_POOL, ocp_rid(s, 0, c), &ocp_cw.src, ocp_ntab_total());
        if (rc) { ocp_r3_sweep(s); if (kb_short) *kb_short = 0; return rc == -1 ? 1 : 2; }
        OC_FEED();
    }
    /* 5. the record: the old project part, the old lists and bank map, a fresh identity; the freeze table split by this project's phrases */
    memset(&ocj4_old, 0, sizeof ocj4_old);
    ocj4_nidx = 0; ocj4_idx_full = 0;
    ocj4_meta_fresh(&meta, raw);
    meta.rev = 1;
    meta.uid = fnv(meta.uid, &s, 4);               /* (two slots converted from the same bytes would otherwise share an identity) */
    meta.seed = fnv(meta.seed, &s, 4);
    for (k = 0; k < NTRK; k++) meta.trk_id[k] = fnv(fnv(meta.seed, "trk", 3), &k, 1);
    ocj4_smpr_refresh(&meta, q);
#if OCJ4_FRZ
    cv_frz_ok = !ocs_open(OCS_FREEZE, 0, &cv_frz) && cv_frz.total == CV_FRZ_REC;
    ocj4_frz_get = cv_frz_get_entry; ocj4_frz_name = cv_frz_name_of;
#endif
    ocj4_cvrt_out = ocj4_old_mark(s);
    rlen = ocj4_build(raw, OCJ4_MAX, q, raw, &ocj_dir, ocj_h.pid, &ocj_l, &meta, 0, 0);
    ocj4_cvrt_out = 0;
#if OCJ4_FRZ
    ocj4_frz_get = ocj4_frz_live_get; ocj4_frz_name = ocj4_frz_live_name;
#endif
    if (!rlen || ocs_free_count() < ocp_parts_of(rlen) || ocj4_parse(raw, rlen, q, 0)) { ocp_r3_sweep(s); return 2; }
    rc = ocs_write(OCS_PROJECT, OCJ4_RID(s), raw, rlen);   /* the commit */
    if (rc) { ocp_r3_sweep(s); return rc == -1 ? 1 : 2; }
    /* 6. the new record reads back end to end: only then the old one goes, and the old chunks nothing names */
    if (ocj_read(s, raw) != (int)BANK_STORE_SIZE) return 2;   /* (both records stay: the next start finishes or reports it) */
    (void)ocs_delete(OCS_PROJECT, s);
    OC_FEED();
    ocj4_old_chunks_cleanup();
    return 0;
}

static int ocj4_convert_slot(uint32_t s, uint32_t *kb_short)
{
    ocj4_old_c = 0;
    int rc = ocj4_convert_slot_body(s, kb_short);
    ocp_rd_dir = &ocp.dir;
    return rc;
}

/* ------------------------------------------------------------ the start --- */
/* at the first start of this firmware (project.c persist_boot, after the sample slot's claim and Part 1's sweep): every slot with a record of the
 * format before. A slot with both: the new record wins when it reads end to end (a cut between the commit and the old record's erase); the old one
 * is then erased. The summary is kept for ocj4_conv_notice */
static void ocj4_slot_erase_new(uint32_t s);
static int ocj4_old_complete(void);
static void ocj4_convert_boot(void)
{
    ocj4_old_c = 0;
    uint32_t s, old = 0, kb = 0, left_kb = 0, left_slot = 0, cannot = 0;
    uint8_t *raw;
    ocj4_conv_msg[0] = 0; ocj4_conv_done = ocj4_conv_left = 0;
    if (!OCJ4_FLASH_OK) return;
    for (s = 0; s < 4u; s++) old += ocs_exists(OCS_PROJECT, s);
    if (!old) { ocj4_old_chunks_cleanup(); return; }     /* (a cut left old chunks behind: nobody names them) */
    BOOT_WORK_BEGIN("CONVERTING PROJECTS", old);
    raw = proj_wire_borrow();
    for (s = 0; s < 4u; s++) {
        int rc = 0;
        if (!ocs_exists(OCS_PROJECT, s)) continue;
        int both = ocs_exists(OCS_PROJECT, OCJ4_RID(s));
        if (both && ocj4_cvrt_read(s) != ocj4_old_mark(s)) {
            /* Both records, and the new one is not the old one's conversion: an old record restored over a slot that has a record of its own
             * (editor_backup.c commits the old one first, then erases the new: a cut between leaves both, and the old one is the newer wish).
             * It is converted over the new one when it reads; one that does not (the pool objects of its restore never came) is dropped and the
             * slot keeps the project it had. */
            if (ocj4_old_read(s, raw) == (int)BANK_STORE_SIZE && ocj4_old_complete()) { ocj4_slot_erase_new(s); both = 0; }
            else { (void)ocs_delete(OCS_PROJECT, s); ocj4_old_c = 0; continue; }
        }
        if (both) {                                         /* a conversion a cut stopped at its last step (the new record carries the old one's mark) */
            if (ocj_read(s, raw) == (int)BANK_STORE_SIZE) { (void)ocs_delete(OCS_PROJECT, s); ocj4_conv_done++; }
            else { ocj4_conv_left++; cannot = s + 1u; }     /* (the new record does not read: the summary says so, not "NEEDS 00K") */
        } else if ((rc = ocj4_convert_slot(s, &kb)) == 0) {
            ocj4_conv_done++;
        } else {
            ocj4_conv_left++;
            if (rc == 1) { left_kb = kb; left_slot = s; } else cannot = s + 1u;
        }
        BOOT_WORK_PROGRESS(ocj4_conv_done + ocj4_conv_left);
        OC_FEED();
    }
    ocj4_old_chunks_cleanup();
    BOOT_WORK_END(0);
    if (ocj4_conv_left) {
        const char *m = cannot ? "CANNOT CONVERT X" : "SLOT X NEEDS 99K";
        uint32_t i = 0;
        for (; m[i] && i < sizeof ocj4_conv_msg - 1u; i++) ocj4_conv_msg[i] = m[i];
        ocj4_conv_msg[i] = 0;
        if (cannot) ocj4_conv_msg[15] = (char)('A' + cannot - 1u);
        else { ocj4_conv_msg[5] = (char)('A' + left_slot); ocj4_conv_msg[13] = (char)('0' + (left_kb > 99u ? 9u : left_kb / 10u)); ocj4_conv_msg[14] = (char)('0' + (left_kb > 99u ? 9u : left_kb % 10u)); }
    } else if (ocj4_conv_done) {
        const char *m = "CONVERTED X PROJECTS";
        uint32_t i = 0;
        for (; m[i] && i < sizeof ocj4_conv_msg - 1u; i++) ocj4_conv_msg[i] = m[i];
        ocj4_conv_msg[i] = 0;
        ocj4_conv_msg[10] = (char)('0' + (ocj4_conv_done > 9u ? 9u : ocj4_conv_done));
    }
}
/* shown once after the boot's project has loaded (as the pack notice), so "LOADED" does not hide it */
static void ocj4_conv_notice(void)
{
    if (ocj4_conv_msg[0]) { ui_message(ocj4_conv_msg); ocj4_conv_msg[0] = 0; }
}

/* the CONVERT action (project_load of a slot that holds a record of the format before): converts that slot alone. 0 converted; the message is
 * shown otherwise. The caller has checked that the transport is stopped */
static int ocj4_convert_now(uint32_t s)
{
    uint32_t kb = 0;
    int rc;
    if (s == ocp.dir.slot) { ui_message("RESTART TO CONVERT"); return 1; }   /* (the live pool is this slot's: a failed conversion would sweep the music's own chunks; the start converts it) */
    ui_message("CONVERTING");
    rc = ocj4_convert_slot(s, &kb);
    if (rc == 1) {
        char m[24] = "OLD FORMAT: NEEDS 00K";
        m[18] = (char)('0' + (kb > 99u ? 9u : kb / 10u)); m[19] = (char)('0' + (kb > 99u ? 9u : kb % 10u));
        ui_message(m);
    } else if (rc) {
        ui_message("CANNOT CONVERT");
    }
    return rc;
}

/* a restore of a project record of the format before into slot s (editor_backup.c): the slot's Release 3 records go first, the old one is
 * committed by the caller, and the slot is converted at the next start or by CONVERT (so it is converted once, never mistaken for a
 * half-finished conversion) */
static void ocj4_slot_erase_new(uint32_t s)
{
    s &= 3u;
    ocj4_old_c = 0;                                /* (the caller commits an old record next) */
    (void)ocs_delete(OCS_PROJECT, OCJ4_RID(s));
    /* (no record now.) The live slot's chunks hold the music's own uncommitted phrases (ocp.dir still names them): they stay, and only
     * the claim a record made is let go, as ocj_write's empty-slot path does; any other slot's chunks all go */
    if (s == ocp.dir.slot) memset(ocp.cnamed, 0, sizeof ocp.cnamed);
    else ocp_r3_sweep(s);
}

/* phrase id (1..64) has bytes in the old pool: chunk (id - 1) / 4 exists and its table gives that phrase a length */
static int ocj4_old_phrase_there(uint32_t id)
{
    ocs_rec_t r;
    uint8_t t[8];
    if (id < 1u || id > 64u || ocs_open(OCS_POOL, (id - 1u) >> 2, &r) || r.total < 8u || ocs_at(&r, 0, t, 8)) return 0;
    return (t[2u * ((id - 1u) & 3u)] | t[2u * ((id - 1u) & 3u) + 1u]) != 0;
}
/* after ocj4_old_read: every phrase the old record names (a bank's, a chain's) has its bytes in the old pool. The old reader takes a phrase with no chunk
 * for an empty one, so a restore of an old record whose pool objects have not all come reads "fine" and would replace the slot's project with an
 * empty one; a phrase a bank or a chain names is never empty (an emptied bank names none) */
static int ocj4_old_complete(void)
{
    for (uint32_t k = 0; k < NTRK; k++)
        for (uint32_t b = 0; b < NPAT; b++) {
            uint32_t id = ocj_h.pid[k][b];
            if (id && !ocj4_old_phrase_there(id)) return 0;
        }
    for (uint32_t c = 0; c < OCL_NCHAIN; c++)
        for (uint32_t e = 0; e < OCL_NENT; e++) {
            uint32_t id = ocj_l.chain[c][e][0];
            if (id && id <= 64u && !ocj4_old_phrase_there(id)) return 0;
        }
    return 1;
}
/* after a record or a chunk of the format before has been restored (editor_backup.c): every slot that has an old record, a Release 3 record that is
 * not that record's conversion, and an old record that now reads end to end (its pool objects are all there) loses its Release 3 records. Called after
 * each restored object, so the page's order does not matter: while the pool is incomplete the slot keeps the project it had (the old record is
 * dropped at the next start if the restore never finishes), and the moment the old project reads it replaces it. Part 4's review, finding 1 */
static void ocj4_restore_settle(void)
{
    uint8_t *raw = (uint8_t *)&proj_wire_u;
    for (uint32_t s = 0; s < 4u; s++) {
        if (!ocs_exists(OCS_PROJECT, s) || !ocs_exists(OCS_PROJECT, OCJ4_RID(s))) continue;
        if (ocj4_cvrt_read(s) == ocj4_old_mark(s)) continue;
        if (ocj4_old_read(s, raw) == (int)BANK_STORE_SIZE && ocj4_old_complete()) ocj4_slot_erase_new(s);
    }
}
