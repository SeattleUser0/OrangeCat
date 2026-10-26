/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* (Orange Cat 0.13.8, branch linux-hw; OC_HW_TEST builds only, off in every published build) the hardware test surface: editor command 108.
 *
 * What a test on the unit needs and no editor command does: press, hold and release a panel key or button, turn a knob, select a page, and
 * "say state": a short tone sequence that tells the current page, the selected track's engine and the last notice shown, so a recording of
 * the output reads what the screen says when USB is not there (the card tests: the command then comes in on the TRS MIDI input, below).
 *
 * The keys go through the real input path: a pressed id counts as a closed contact in hal/fm1_input.h fm1__keys (FM1_INPUT_FORCED), so the
 * debounce, the edges, the hold times and everything above them (ui_input.c) see exactly what a finger makes. A knob's steps are added where
 * the encoder decoder leaves its detents (fm1_in.enc_steps), what fm1_enc_take returns. tests/input_test.c drives the same path on the host.
 *
 * Request (after the command byte): op [, args]. Reply: op, rc (0 ok, 1 bad request), then the state (op 0 and every other op):
 * forced key mask (6 x 7 bits, ids 0..40), page (127: HOME), engine of the selected track, selected track, notice code (2 x 7 bits, the
 * 12-bit code of the last notice, below), notice text (0 ended), then the panel's map: 14 button ids by printed label (panel.c B_FX .. B_OCTUP)
 * and for each of the 7 knob roles (EN_SELECT .. EN_K4) its encoder and 1 if clockwise is + (a test presses REC as panel.btn[B_REC]), then the
 * page's title (0 ended), the step cursor (SEQ), the tracker's column (N1 .. FX: 0 .. 5) and its picked effect (RATCHET .. ACCENT: 0 .. 5), the picked
 * effect's value on the step a tracker edit at the cursor writes (0 ended: "x4", "FLAM", "+16", "50%", "ON", "OFF") and that step's row.
 *   0                query
 *   1 id             press key id (0..13 buttons, 14..40 note keys; hal/fm1_input.h FM1_KEYMAP) and keep it pressed
 *   2 id             release key id (127: every key)
 *   3 knob s_lo s_hi turn knob 0..6 by s detents (14 bits, offset 8192 as everywhere; + = clockwise)
 *   4 page           select page (an index of PAGES that page_visible allows; 127: HOME)
 *   5                say state (the tones below; about 0.6 s; the output is the tones alone meanwhile)
 *   6 track step     the step's recorded notes, in NOTES' order (seq_notes.c notes_nth), after the state: count, then for each: note, velocity,
 *                    onset (3 x 7 bits, of RECORD_UNIT 65536 a step), duration (3 x 7 bits) and its exponent (the length in steps is
 *                    duration << exponent / 65536): what the tracker's columns and NOTES show, read without a key
 *
 * The tones: a marker of 1,760 Hz for 120 ms, then five digits, each 60 ms of tone and 20 ms of silence; digit d (0..63) is 400 + 50 d Hz
 * (400 .. 3,550). The digits: page >> 6, page & 63, engine & 63, code >> 6, code & 63. The notice code is FNV-1a of the notice's text, folded to
 * 12 bits (tools/hw/capture.py `say` decodes a recording and names the notice by hashing the firmware's notice strings the same way). */

#define HWT_CMD 108u                                      /* editor.c ED_HW_TEST */
#define HWT_DIGITS 5u
/* oc_hwt_forced[2] (melodee.c, before hal/fm1_input.h): bit id set = key id held by the test */

static uint32_t hwt_code(const char *s)                    /* the notice code: FNV-1a, folded to 12 bits */
{
    uint32_t h = 2166136261u;
    for (; *s; s++)
        h = (h ^ (uint8_t)*s) * 16777619u;
    return (h ^ (h >> 12) ^ (h >> 24)) & 0xFFFu;
}

/* the say-state plan: tone i lasts len[i] samples at freq[i] Hz (0: silence); returns the count */
#define HWT_PLAN_MAX (1u + 2u * HWT_DIGITS)
static uint32_t hwt_plan(uint32_t page, uint32_t engine, uint32_t code, uint16_t *freq, uint16_t *len)
{
    const uint32_t d[HWT_DIGITS] = {page >> 6 & 63u, page & 63u, engine & 63u, code >> 6 & 63u, code & 63u};
    uint32_t n = 0, i;
    freq[n] = 1760u; len[n++] = (uint16_t)(FS * 120u / 1000u);
    for (i = 0; i < HWT_DIGITS; i++) {
        freq[n] = 0; len[n++] = (uint16_t)(FS * 20u / 1000u);
        freq[n] = (uint16_t)(400u + 50u * d[i]); len[n++] = (uint16_t)(FS * 60u / 1000u);
    }
    return n;
}

static struct {
    uint16_t freq[HWT_PLAN_MAX], len[HWT_PLAN_MAX];
    volatile uint32_t n, i, left, phase;                  /* the ISR walks the plan: n tones, the one playing, its samples left */
} hwt_say;

/* the audio ISR, after the mix and before the USB copy (audio.c audio_block): while a plan plays, the output is its tones */
static void hwt_tone(int32_t *out, uint32_t n)
{
    uint32_t k;
    if (hwt_say.i >= hwt_say.n)
        return;
    for (k = 0; k < n && hwt_say.i < hwt_say.n; k++) {  /* (the rest of the block after the plan's end is the music again) */
        int32_t v = 0;
        uint32_t f = hwt_say.freq[hwt_say.i];
        if (f) {                                           /* a triangle at a quarter of full scale */
            uint32_t ph = hwt_say.phase >> 16;
            v = (int32_t)(ph < 32768u ? ph : 65535u - ph) - 16384;
            v /= 2;
            hwt_say.phase += (uint32_t)(((uint64_t)f << 32) / FS);
        }
        if (!--hwt_say.left && ++hwt_say.i < hwt_say.n)
            hwt_say.left = hwt_say.len[hwt_say.i];
        out[2u * k] = out[2u * k + 1u] = v;
    }
}

#ifndef HWT_TONES_ONLY                                    /* (tests/hw_say_test.c builds the codes, the plan and the tones alone) */
static uint32_t hwt_page(void) { return ui.home ? 127u : ui.page; }

static void hwt_state(uint32_t op, uint32_t rc)
{
    uint64_t m = oc_hwt_forced[0] | (uint64_t)oc_hwt_forced[1] << 32;
    ed_b(op); ed_b(rc);
    for (uint32_t j = 0; j < 6u; j++) ed_b((uint32_t)(m >> (7u * j)) & 127u);
    ed_b(hwt_page()); ed_b(TSEL->engine & 127u); ed_b(song.sel);
    {
        uint32_t c = hwt_code(ui.msg);
        ed_b(c & 127u); ed_b(c >> 7);
    }
    ed_str(ui.msg, sizeof ui.msg);
    for (uint32_t i = 0; i < NB; i++) ed_b(panel.btn[i]);              /* the panel's map: the button id of each printed label (panel.c B_*) */
    for (uint32_t i = 0; i < NE; i++) { ed_b(panel.enc[i]); ed_b(panel.dir[i] > 0); }   /* the knob of each role (EN_*), clockwise + */
    ed_str(ui.home ? "HOME" : cur_page()->title, 12);                 /* the page's title, */
    ed_b(ui.cursor & 127u); ed_b(ui.tk_col & 127u); ed_b(ui.tk_fx & 127u);   /* the step cursor, the tracker's column and its picked effect */
    {   /* the picked effect's value on the step a tracker edit at the cursor writes (ui_input.c tracker_event_edit: a recorded step's notes ->
         * the row they are shown on; else the cursor's own step), as the tracker writes it ("x4", "FLAM", "+16", "50%", "ON"; "OFF") */
        const track_t *t = TSEL;
        uint32_t c = ui.cursor % NSTEP, e = notes_nth(t, c, 0);
        const step_t *st = &t->step[e < RECORD_MAX ? recording_view(t, &recording[e]) % NSTEP : c];
        char b[16];
        tk_fx_text(st, ui.tk_fx % TFX_N, b, 1);
        ed_str(b, sizeof b);
        ed_b(e < RECORD_MAX ? recording_view(t, &recording[e]) % NSTEP : c);
    }
}

static int ed_hwt_handle(uint32_t cmd, const uint8_t *a, uint32_t na)
{
    uint32_t op = na ? a[0] : 127u, rc = 0;
    if (cmd != HWT_CMD)
        return 0;
    if (op == 1u && na == 2u && a[1] < FM1_NKEY) {
        fm1_irq_off(); oc_hwt_forced[a[1] >> 5] |= 1u << (a[1] & 31u); fm1_irq_on();
    } else if (op == 2u && na == 2u && (a[1] < FM1_NKEY || a[1] == 127u)) {
        fm1_irq_off();
        if (a[1] == 127u) oc_hwt_forced[0] = oc_hwt_forced[1] = 0;
        else oc_hwt_forced[a[1] >> 5] &= ~(1u << (a[1] & 31u));
        fm1_irq_on();
    } else if (op == 3u && na == 4u && a[1] < FM1_NENC) {
        int32_t s = (int32_t)(a[2] | (uint32_t)a[3] << 7) - 8192;
        fm1_irq_off(); fm1_in.enc_steps[a[1]] = (int16_t)(fm1_in.enc_steps[a[1]] + s); fm1_irq_on();
    } else if (op == 4u && na == 2u) {
        if (a[1] == 127u) ui.home = 1, ui.force = 1;
        else if (a[1] < NPAGES && page_visible(a[1])) { ui.home = 0; ui.page = a[1]; page_entered(); ui.force = 1; }
        else rc = 1;
    } else if (op == 5u && na == 1u) {
        uint16_t f[HWT_PLAN_MAX], l[HWT_PLAN_MAX];
        uint32_t n = hwt_plan(hwt_page(), TSEL->engine, hwt_code(ui.msg), f, l);
        fm1_irq_off();
        memcpy(hwt_say.freq, f, sizeof f); memcpy(hwt_say.len, l, sizeof l);
        hwt_say.n = n; hwt_say.i = 0; hwt_say.left = l[0]; hwt_say.phase = 0;
        fm1_irq_on();
    } else if (op == 6u && na == 3u && a[1] < NTRK && a[2] < NSTEP) {
        hwt_state(op, 0);
        {
            uint32_t idx[16], n = 0, k;
            while (n < 16u && (idx[n] = notes_nth(&trk[a[1]], a[2], n)) < RECORD_MAX) n++;
            ed_b(n);
            for (k = 0; k < n; k++) {
                recorded_note_t r = recording_snapshot(idx[k]);
                ed_b(r.note & 127u); ed_b(r.vel & 127u);
                ed_b(r.on & 127u); ed_b((r.on >> 7) & 127u); ed_b(r.on >> 14);
                ed_b(r.duration & 127u); ed_b((r.duration >> 7) & 127u); ed_b(r.duration >> 14);
                ed_b(r.owner >> 5);
            }
        }
        return 1;
    } else if (op != 0u || na != 1u) {
        rc = 1;
    }
    hwt_state(op, rc);
    return 1;
}

/* the TRS MIDI input (midi_uart.c, in the TIMER5 ISR) keeps one frame of this command; the main loop runs it (hwt_uart_poll, from ed_service).
 * Only command 108 is taken from the TRS input: every other SysEx there is ignored, as in a published build */
static uint8_t hwt_rx[24];
static volatile uint8_t hwt_rx_n, hwt_rx_ready, hwt_rx_long;
static void hwt_uart_byte(uint32_t b)                    /* every byte of a SysEx on the TRS input, F0 .. F7 */
{
    if (hwt_rx_ready) return;                             /* one frame at a time */
    if (b == 0xF0u) { hwt_rx_n = 0; hwt_rx_long = 0; return; }
    if (b == 0xF7u) {
        if (!hwt_rx_long && hwt_rx_n >= 4u && hwt_rx[0] == ED_HDR0 && hwt_rx[1] == ED_HDR1 && hwt_rx[2] == ED_HDR2 && hwt_rx[3] == HWT_CMD) hwt_rx_ready = 1;
        else hwt_rx_n = 0;
        return;
    }
    if (hwt_rx_n < sizeof hwt_rx) hwt_rx[hwt_rx_n++] = (uint8_t)b;
    else hwt_rx_long = 1;                                 /* longer than any request of 108: dropped at F7 */
}
static void ed_handle(const uint8_t *f, uint32_t n);      /* (editor.c, below) */
static void hwt_uart_poll(void)
{
    if (!hwt_rx_ready) return;
    ed_handle(hwt_rx, hwt_rx_n);                          /* the reply goes to USB if it is there; the tones are the answer otherwise */
    hwt_rx_n = 0;
    hwt_rx_ready = 0;
}
#endif
