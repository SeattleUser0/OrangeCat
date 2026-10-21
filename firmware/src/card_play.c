/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat: a WAV playing from the USB drive (build doc phase 2, the plan's steps 2 and 4). The audio side only:
 * a ring of 16-bit stereo frames that the main loop fills from the card (usb_files.c uf_play_*) and the audio ISR
 * plays (card_render, from audio.c audio_block, after the click: at the DAC, with MASTER, -6 dB so a full scale
 * file leaves headroom for the synths under it).
 *
 * The file's rate is followed by stepping through the ring at rate / 44100 (Q16) with a straight line between two
 * frames: a 48 kHz file plays at its own pitch. The ring is 32 KB in the .pool section (0.19 s at 44.1 kHz): the main
 * loop tops it up 4 KB at a time. (.pool is shared with the audio working arena, which must keep 48 KB: the first
 * 0.4.11 build, with a 64 KB ring and 8 KB reads, failed to link on that.) When the ISR finds fewer frames than it needs it plays silence for the rest of the
 * block and counts a gap (cp_gaps): the page shows them, so a test says whether the card keeps up.
 *
 * One producer (the main loop: cp_w), one consumer (the audio ISR: cp_r). The main loop writes the frames, then
 * cp_w (RING_PUBLISH between); the ISR reads cp_w once a block.
 *
 * (0.4.12) The reading is card_stream.c's (kept apart so the audio's host tests need no USB host): cs_tick, from the
 * 10 kHz TIMER5 interrupt (main.c), drives whole SCSI READ(10)
 * commands one step a tick (the command out, each 64-byte packet in, the status in) and turns each packet into
 * frames in the ring as it lands (cs_feed). Nothing waits on the bus: a tick looks at one register and moves on.
 * The main loop no longer reads during playback; it only works out where the file is on the card first (cs_ext:
 * the runs of sectors the file's clusters make, from FatFs's cluster chain) and then watches. When it needs the
 * bus itself (FatFs listing a folder while the WAV plays) it holds the engine at the end of its command (cs_claim)
 * and lets it go after (cs_release).
 *
 * Revisions:
 *   0.4.11: first version.
 *   0.4.12: the card read from the timer interrupt (card_stream.c), straight into the ring: no read buffer, no main
 *          loop time per read. 16 sectors (8 KB) a command.
 */
#ifndef CP_GAIN
#define CP_GAIN() ((int32_t)song.master_q12)            /* (host tests: a fixed level) */
#endif
#define CP_FRAMES 8192u                                 /* the ring: a power of two (32 KB, 0.19 s at 44.1 kHz) */
static int16_t cp_ring[CP_FRAMES][2] __attribute__((section(".pool"), aligned(4)));
static volatile uint32_t cp_w, cp_r;                    /* frames written (main loop), frames passed (ISR) */
static volatile uint8_t cp_on;                          /* the ISR plays */
static volatile uint32_t cp_gaps;                       /* blocks the ring ran short in */
static uint32_t cp_step = 65536u, cp_phase;             /* file frames per output frame (Q16), the fraction now */

/* add the file to one block of DAC samples (stereo Q15, after the master and the click) */
static __attribute__((noinline)) void card_render(int32_t *out, uint32_t n)
{
    uint32_t i, r, w, ph, st;
    int32_t g;
    if (!cp_on)
        return;
    r = cp_r;
    w = cp_w;
    ph = cp_phase;
    st = cp_step;
    g = CP_GAIN();
    for (i = 0; i < n; i++) {
        const int16_t *a, *b;
        int32_t f, l, rr;
        if (w - r < 2u) {                               /* short: silence for the rest of the block */
            cp_gaps++;
            break;
        }
        a = cp_ring[r & (CP_FRAMES - 1u)];
        b = cp_ring[(r + 1u) & (CP_FRAMES - 1u)];
        f = (int32_t)(ph >> 1);                         /* Q15 */
        l = a[0] + (((b[0] - a[0]) * f) >> 15);
        rr = a[1] + (((b[1] - a[1]) * f) >> 15);
        out[2u * i] += (l * g) >> 13;                   /* MASTER (Q12), -6 dB */
        out[2u * i + 1u] += (rr * g) >> 13;
        ph += st;
        r += ph >> 16;
        ph &= 0xFFFFu;
    }
    cp_phase = ph;
    cp_r = r;
}

