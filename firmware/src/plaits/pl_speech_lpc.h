/* SPDX-License-Identifier: MIT
 * Copyright 2016-2021 Emilie Gillet (Mutable Instruments Plaits). MIT License: LICENSES/MIT-Mutable.txt
 * C port for Orange Cat: Copyright 2026 Jeremiah Conner. Same license.
 *
 * Orange Cat MACRO2 candidate: the LPC10 half of Plaits' SPEECH model (pl_speech.h), in C.
 *   plaits/dsp/speech/lpc_speech_synth.{h,cc}             pl_speech_lpc_t: the lattice filter and its excitation
 *   plaits/dsp/speech/lpc_speech_synth_controller.{h,cc}  pl_speech_bitstream_t (BitStream), pl_speech_word_bank_t
 *                                                         (LPCSpeechSynthWordBank, its decode luts) and
 *                                                         pl_speech_lpc_ctl_t (LPCSpeechSynthController)
 *   plaits/dsp/speech/lpc_speech_synth_phonemes.cc        pl_speech_phonemes (the 5 vowels and 10 consonants)
 *   plaits/dsp/speech/lpc_speech_synth_words.{h,cc}       the five word banks' bytes are pl_lpc_word_banks (pl_tables2.h)
 * Every float operation in the order of the C++.
 *
 * Deliberate differences:
 *   THE WORD BANK DECODES ON THE FLY. The C++ decodes a whole bank into kLPCSpeechSynthMaxFrames (1024 frames of
 *   14 bytes) plus kLPCSpeechSynthMaxWords (32) boundaries from the allocator, 14.5 KB. Decoded with the C++
 *   (the parity build's own LPCSpeechSynthWordBank), the banks hold
 *     bank 0: 1233 bytes, 229 frames,  7 words      bank 3: 2524 bytes, 604 frames, 26 words
 *     bank 1:  900 bytes, 229 frames, 11 words      bank 4: 4802 bytes, 926 frames, 22 words (longest word 58 frames)
 *     bank 2: 1552 bytes, 524 frames, 26 words
 *   so the exact-array form costs 13,116 bytes per engine (928 frames + 28 boundaries), too much for a track. Here
 *   Load walks the bank once with the C++'s decode, counting, and keeps per word only {first byte, first frame}
 *   (pl_speech_word_bank_t: 160 bytes). The C++ starts every word's decode from a zero frame and byte-aligns the
 *   words (Flush at the 0xf code), so a word's frames are reproduced exactly from its first byte: PlayFrame(i)
 *   decodes from the start of frame i's word up to frame i (f1), and one frame further for f2 (the next word's
 *   first frame when i ends a word; a zero frame past the last word: the C++ reads frames_[num_frames_] there,
 *   blended by 0, which gives the same values). The decoder where the last read stopped stays in the struct, so
 *   the next read of frame i or i + 1 (the same address next block, or the next frame of a word) is 0 or 1 frame
 *   of decoding; a jump backwards restarts from the word's start (58 frames at most). The decode itself
 *   (pl_speech_word_decoder_next) is LoadNextWord's loop body unchanged, so the frame bytes are the C++'s.
 *   The bank table is the fixed pl_lpc_word_banks (no word_banks_ pointer or num_banks_, no user data).
 *   The controller's pointer to the word bank is an argument of pl_speech_lpc_ctl_render instead of a field.
 */
#pragma once
#include "pl_dsp2.h"
#include "pl_tables2.h"

#define PL_SPEECH_LPC_ORDER 10                  /* kLPCOrder */
#define PL_SPEECH_LPC_DEFAULT_F0 100.0f         /* kLPCSpeechSynthDefaultF0 */
#define PL_SPEECH_LPC_MAX_WORDS 28              /* words + 1 per bank (kLPCSpeechSynthMaxWords is 32; 26 words at most) */
#define PL_SPEECH_LPC_NUM_VOWELS 5
#define PL_SPEECH_LPC_NUM_CONSONANTS 10
#define PL_SPEECH_LPC_NUM_PHONEMES (PL_SPEECH_LPC_NUM_VOWELS + PL_SPEECH_LPC_NUM_CONSONANTS)
#define PL_SPEECH_LPC_FPS 40.0f                 /* kLPCSpeechSynthFPS */

/* LPCSpeechSynth::Frame: 14 bytes */
typedef struct {
    uint8_t energy, period;
    int16_t k0, k1;
    int8_t k2, k3, k4, k5, k6, k7, k8, k9;
} pl_speech_frame_t;
/* (field by field: a struct assignment or a {...} initialiser becomes a memcpy call on the FM-1, which a module has
 * not got) */
static inline void pl_speech_frame_clear(pl_speech_frame_t *f)
{
    f->energy = 0; f->period = 0; f->k0 = 0; f->k1 = 0;
    f->k2 = 0; f->k3 = 0; f->k4 = 0; f->k5 = 0; f->k6 = 0; f->k7 = 0; f->k8 = 0; f->k9 = 0;
}
static inline void pl_speech_frame_copy(pl_speech_frame_t *to, const pl_speech_frame_t *from)
{
    to->energy = from->energy; to->period = from->period; to->k0 = from->k0; to->k1 = from->k1;
    to->k2 = from->k2; to->k3 = from->k3; to->k4 = from->k4; to->k5 = from->k5;
    to->k6 = from->k6; to->k7 = from->k7; to->k8 = from->k8; to->k9 = from->k9;
}

/* ------------------------------------------------------ lpc_speech_synth.cc --- */
typedef struct {
    float phase, frequency, noise_energy, pulse_energy;
    float next_sample;
    int excitation_pulse_sample_index;
    float k[PL_SPEECH_LPC_ORDER];
    float s[PL_SPEECH_LPC_ORDER + 1];
} pl_speech_lpc_t;

static inline void pl_speech_lpc_init(pl_speech_lpc_t *l)
{
    int i;
    l->phase = 0.0f;
    l->frequency = 0.0125f;
    l->noise_energy = 0.0f;
    l->pulse_energy = 0.0f;
    l->next_sample = 0.0f;
    l->excitation_pulse_sample_index = 0;
    for (i = 0; i < PL_SPEECH_LPC_ORDER; ++i) l->k[i] = 0.0f;
    for (i = 0; i < PL_SPEECH_LPC_ORDER + 1; ++i) l->s[i] = 0.0f;
}

static void pl_speech_lpc_render(pl_speech_lpc_t *l, float prosody_amount, float pitch_shift, float *excitation,
                                 float *output, size_t size)
{
    const float base_f0 = PL_SPEECH_LPC_DEFAULT_F0 / 8000.0f;
    float d = l->frequency - base_f0;
    float f = (base_f0 + d * prosody_amount) * pitch_shift;
    PL_CONSTRAIN(f, 0.0f, 0.5f);

    float next_sample = l->next_sample;
    while (size--) {
        l->phase += f;

        float this_sample = next_sample;
        next_sample = 0.0f;

        if (l->phase >= 1.0f) {
            l->phase -= 1.0f;
            float reset_time = l->phase / f;
            int reset_sample = (int)(32.0f * reset_time);

            float discontinuity = 0.0f;
            if (l->excitation_pulse_sample_index < (int)PL_LUT_LPC_EXCITATION_PULSE_SIZE) {
                l->excitation_pulse_sample_index -= reset_sample;
                int8_t s = pl_lut_lpc_excitation_pulse[l->excitation_pulse_sample_index];
                discontinuity = (float)s / 128.0f * l->pulse_energy;
            }

            this_sample += -discontinuity * pl_this_blep(reset_time);
            next_sample += -discontinuity * pl_next_blep(reset_time);

            l->excitation_pulse_sample_index = reset_sample;
        }

        float e[11];
        e[10] = pl_rng_sample() > 0 ? l->noise_energy : -l->noise_energy;
        if (l->excitation_pulse_sample_index < (int)PL_LUT_LPC_EXCITATION_PULSE_SIZE) {
            int8_t s = pl_lut_lpc_excitation_pulse[l->excitation_pulse_sample_index];
            next_sample += (float)s / 128.0f * l->pulse_energy;
            l->excitation_pulse_sample_index += 32;
        }
        e[10] += this_sample;
        e[10] *= 1.5f;

        e[9] = e[10] - l->k[9] * l->s[9];
        e[8] = e[9] - l->k[8] * l->s[8];
        e[7] = e[8] - l->k[7] * l->s[7];
        e[6] = e[7] - l->k[6] * l->s[6];
        e[5] = e[6] - l->k[5] * l->s[5];
        e[4] = e[5] - l->k[4] * l->s[4];
        e[3] = e[4] - l->k[3] * l->s[3];
        e[2] = e[3] - l->k[2] * l->s[2];
        e[1] = e[2] - l->k[1] * l->s[1];
        e[0] = e[1] - l->k[0] * l->s[0];

        PL_CONSTRAIN(e[0], -2.0f, 2.0f);

        l->s[9] = l->s[8] + l->k[8] * e[8];
        l->s[8] = l->s[7] + l->k[7] * e[7];
        l->s[7] = l->s[6] + l->k[6] * e[6];
        l->s[6] = l->s[5] + l->k[5] * e[5];
        l->s[5] = l->s[4] + l->k[4] * e[4];
        l->s[4] = l->s[3] + l->k[3] * e[3];
        l->s[3] = l->s[2] + l->k[2] * e[2];
        l->s[2] = l->s[1] + l->k[1] * e[1];
        l->s[1] = l->s[0] + l->k[0] * e[0];
        l->s[0] = e[0];

        *excitation++ = e[10];
        *output++ = e[0];
    }
    l->next_sample = next_sample;
}

/* BlendCoefficient<32768, int16_t> and BlendCoefficient<128, int8_t> */
static inline float pl_speech_lpc_blend16(int16_t a, int16_t b, float blend)
{
    float a_f = (float)a / 32768.0f;
    float b_f = (float)b / 32768.0f;
    return a_f + (b_f - a_f) * blend;
}
static inline float pl_speech_lpc_blend8(int8_t a, int8_t b, float blend)
{
    float a_f = (float)a / 128.0f;
    float b_f = (float)b / 128.0f;
    return a_f + (b_f - a_f) * blend;
}

/* PlayFrame(f1, f2, blend) */
static void pl_speech_lpc_play_frame2(pl_speech_lpc_t *l, const pl_speech_frame_t *f1, const pl_speech_frame_t *f2,
                                      float blend)
{
    float frequency_1 = f1->period == 0 ? l->frequency : 1.0f / (float)f1->period;
    float frequency_2 = f2->period == 0 ? l->frequency : 1.0f / (float)f2->period;
    l->frequency = frequency_1 + (frequency_2 - frequency_1) * blend;

    float energy_1 = (float)f1->energy / 256.0f;
    float energy_2 = (float)f2->energy / 256.0f;
    float noise_energy_1 = f1->period == 0 ? energy_1 : 0.0f;
    float noise_energy_2 = f2->period == 0 ? energy_2 : 0.0f;
    l->noise_energy = noise_energy_1 + (noise_energy_2 - noise_energy_1) * blend;

    float pulse_energy_1 = f1->period != 0 ? energy_1 : 0.0f;
    float pulse_energy_2 = f2->period != 0 ? energy_2 : 0.0f;
    l->pulse_energy = pulse_energy_1 + (pulse_energy_2 - pulse_energy_1) * blend;

    l->k[0] = pl_speech_lpc_blend16(f1->k0, f2->k0, blend);
    l->k[1] = pl_speech_lpc_blend16(f1->k1, f2->k1, blend);
    l->k[2] = pl_speech_lpc_blend8(f1->k2, f2->k2, blend);
    l->k[3] = pl_speech_lpc_blend8(f1->k3, f2->k3, blend);
    l->k[4] = pl_speech_lpc_blend8(f1->k4, f2->k4, blend);
    l->k[5] = pl_speech_lpc_blend8(f1->k5, f2->k5, blend);
    l->k[6] = pl_speech_lpc_blend8(f1->k6, f2->k6, blend);
    l->k[7] = pl_speech_lpc_blend8(f1->k7, f2->k7, blend);
    l->k[8] = pl_speech_lpc_blend8(f1->k8, f2->k8, blend);
    l->k[9] = pl_speech_lpc_blend8(f1->k9, f2->k9, blend);
}

/* ------------------------------------------- lpc_speech_synth_phonemes.cc --- */
static const pl_speech_frame_t pl_speech_phonemes[PL_SPEECH_LPC_NUM_PHONEMES] = {
    { 192,  80, -18368,  11584,  52,  29,  23,  14, -17,  79,  37,   4 },
    { 192,  80, -14528,   1536,  38,  29,  11,  14, -41,  79,  57,   4 },
    { 192,  80,  14528,   9216,  25, -54, -70,  36,  19,  79,  57,  22 },
    { 192,  80, -14528, -13440,  38,  57,  57,  14, -53,   7,  37,  77 },
    { 192,  80, -26368,   4160,  11,  15,  -1,  36, -41,  31,  77,  22 },
    {  15,   0,   5184,   9216, -29, -12,   0,   0,   0,   0,   0,   0 },
    {  10,   0,  27968,  17856,  25,  43, -24, -20, -53,  55,  -4, -51 },
    { 128, 160,  14528,  -3712, -43, -26, -24, -20, -53,  55,  -4, -51 },
    { 128, 160,  10048,  11584, -16,  15,   0,   0,   0,   0,   0,   0 },
    { 224, 100,  18368, -13440, -97, -26, -12, -53, -41,   7,  57,  32 },
    { 192,  80, -10048,   9216, -70,  15,  34, -20, -17,  31, -24,  22 },
    {  96, 160, -18368,  17856, -29, -12, -35,   3,  -5,   7,  37,  22 },
    {  64,  80, -21632,  -6272, -83,  29,  57,   3,  -5,   7,  16,  32 },
    { 192,  80,      0,  -1088,  11, -26, -24,  -9,  -5,  55,  37,  22 },
    {  64,  80,  21632, -17536, -97,  85,  57, -20, -17,  31,  -4,  59 },
};

/* ------------------------------------------ lpc_speech_synth_controller.cc --- */
/* BitStream */
typedef struct { const uint8_t *p; int available; uint16_t bits; } pl_speech_bitstream_t;
static inline void pl_speech_bitstream_init(pl_speech_bitstream_t *b, const uint8_t *p)
{
    b->p = p;
    b->available = 0;
    b->bits = 0;
}
static inline uint8_t pl_speech_bitstream_reverse(uint8_t b)
{
    b = (uint8_t)((b >> 4) | (b << 4));
    b = (uint8_t)(((b & 0xcc) >> 2) | ((b & 0x33) << 2));
    b = (uint8_t)(((b & 0xaa) >> 1) | ((b & 0x55) << 1));
    return b;
}
static inline uint8_t pl_speech_bitstream_get_bits(pl_speech_bitstream_t *b, int num_bits)
{
    int shift = num_bits;
    if (num_bits > b->available) {
        b->bits = (uint16_t)(b->bits << b->available);
        shift -= b->available;
        b->bits |= pl_speech_bitstream_reverse(*b->p++);
        b->available += 8;
    }
    b->bits = (uint16_t)(b->bits << shift);
    uint8_t result = (uint8_t)(b->bits >> 8);
    b->bits &= 0xff;
    b->available -= num_bits;
    return result;
}
static inline void pl_speech_bitstream_flush(pl_speech_bitstream_t *b)
{
    while (b->available) pl_speech_bitstream_get_bits(b, 1);
}

/* LPCSpeechSynthWordBank's decode luts (read while decoding frames only) */
static const uint8_t pl_speech_energy_lut[16] = {
    0x00, 0x02, 0x03, 0x04, 0x05, 0x07, 0x0a, 0x0f, 0x14, 0x20, 0x29, 0x39, 0x51, 0x72, 0xa1, 0xff
};
static const uint8_t pl_speech_period_lut[64] = {
    0, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
    32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 45, 47, 49, 51, 53,
    54, 57, 59, 61, 63, 66, 69, 71, 73, 77, 79, 81, 85, 87, 92, 95, 99,
    102, 106, 110, 115, 119, 123, 128, 133, 138, 143, 149, 154, 160
};
static const int16_t pl_speech_k0_lut[32] = {
    -32064, -31872, -31808, -31680, -31552, -31424, -31232, -30848,
    -30592, -30336, -30016, -29696, -29376, -28928, -28480, -27968,
    -26368, -24256, -21632, -18368, -14528, -10048,  -5184,      0,
      5184,  10048,  14528,  18368,  21632,  24256,  26368,  27968
};
static const int16_t pl_speech_k1_lut[32] = {
    -20992, -19328, -17536, -15552, -13440, -11200,  -8768,  -6272,
     -3712,  -1088,   1536,   4160,   6720,   9216,  11584,  13824,
     15936,  17856,  19648,  21248,  22656,  24000,  25152,  26176,
     27072,  27840,  28544,  29120,  29632,  30080,  30464,  32384
};
static const int8_t pl_speech_k2_lut[16] = { -110, -97, -83, -70, -56, -43, -29, -16, -2, 11, 25, 38, 52, 65, 79, 92 };
static const int8_t pl_speech_k3_lut[16] = { -82, -68, -54, -40, -26, -12, 1, 15, 29, 43, 57, 71, 85, 99, 113, 126 };
static const int8_t pl_speech_k4_lut[16] = { -82, -70, -59, -47, -35, -24, -12, -1, 11, 23, 34, 46, 57, 69, 81, 92 };
static const int8_t pl_speech_k5_lut[16] = { -64, -53, -42, -31, -20, -9, 3, 14, 25, 36, 47, 58, 69, 80, 91, 102 };
static const int8_t pl_speech_k6_lut[16] = { -77, -65, -53, -41, -29, -17, -5, 7, 19, 31, 43, 55, 67, 79, 90, 102 };
static const int8_t pl_speech_k7_lut[8] = { -64, -40, -16, 7, 31, 55, 79, 102 };
static const int8_t pl_speech_k8_lut[8] = { -64, -44, -24, -4, 16, 37, 57, 77 };
static const int8_t pl_speech_k9_lut[8] = { -51, -33, -15, 4, 22, 32, 59, 77 };

/* One word's decoder: LoadNextWord's bitstream and its running frame (a repeat frame, or a silent one, keeps the
 * previous frame's fields: the frame persists through the word) */
typedef struct { pl_speech_bitstream_t bitstream; pl_speech_frame_t frame; } pl_speech_word_decoder_t;
static inline void pl_speech_word_decoder_init(pl_speech_word_decoder_t *d, const uint8_t *data)
{
    pl_speech_bitstream_init(&d->bitstream, data);
    pl_speech_frame_clear(&d->frame);
}
static inline void pl_speech_word_decoder_copy(pl_speech_word_decoder_t *to, const pl_speech_word_decoder_t *from)
{
    to->bitstream.p = from->bitstream.p;
    to->bitstream.available = from->bitstream.available;
    to->bitstream.bits = from->bitstream.bits;
    pl_speech_frame_copy(&to->frame, &from->frame);
}
/* LoadNextWord's loop body: the next frame of the word into d->frame; 0 at the word's end (the 0xf code, Flushed) */
static int pl_speech_word_decoder_next(pl_speech_word_decoder_t *d)
{
    pl_speech_bitstream_t *bitstream = &d->bitstream;
    int energy = pl_speech_bitstream_get_bits(bitstream, 4);
    if (energy == 0) {
        d->frame.energy = 0;
    } else if (energy == 0xf) {
        pl_speech_bitstream_flush(bitstream);
        return 0;
    } else {
        d->frame.energy = pl_speech_energy_lut[energy];
        int repeat = pl_speech_bitstream_get_bits(bitstream, 1);
        d->frame.period = pl_speech_period_lut[pl_speech_bitstream_get_bits(bitstream, 6)];
        if (!repeat) {
            d->frame.k0 = pl_speech_k0_lut[pl_speech_bitstream_get_bits(bitstream, 5)];
            d->frame.k1 = pl_speech_k1_lut[pl_speech_bitstream_get_bits(bitstream, 5)];
            d->frame.k2 = pl_speech_k2_lut[pl_speech_bitstream_get_bits(bitstream, 4)];
            d->frame.k3 = pl_speech_k3_lut[pl_speech_bitstream_get_bits(bitstream, 4)];
            if (d->frame.period) {
                d->frame.k4 = pl_speech_k4_lut[pl_speech_bitstream_get_bits(bitstream, 4)];
                d->frame.k5 = pl_speech_k5_lut[pl_speech_bitstream_get_bits(bitstream, 4)];
                d->frame.k6 = pl_speech_k6_lut[pl_speech_bitstream_get_bits(bitstream, 4)];
                d->frame.k7 = pl_speech_k7_lut[pl_speech_bitstream_get_bits(bitstream, 3)];
                d->frame.k8 = pl_speech_k8_lut[pl_speech_bitstream_get_bits(bitstream, 3)];
                d->frame.k9 = pl_speech_k9_lut[pl_speech_bitstream_get_bits(bitstream, 3)];
            }
        }
    }
    return 1;
}

/* LPCSpeechSynthWordBank: the loaded bank's words (where each starts, in bytes and in frames) and the decoder left
 * where the last read stopped. 160 bytes on the device (the C++'s frames_ and word_boundaries_: 14,464). */
typedef struct {
    int loaded_bank, num_frames, num_words;
    uint16_t word_offset[PL_SPEECH_LPC_MAX_WORDS];   /* [w]: the word's first byte in the bank; [num_words]: its size */
    uint16_t word_frame[PL_SPEECH_LPC_MAX_WORDS];    /* [w]: the word's first frame; [num_words]: num_frames (= word_boundaries_) */
    pl_speech_word_decoder_t decoder;                /* holds frame decoder_frame of word decoder_word (-1: nothing) */
    int decoder_word, decoder_frame;
} pl_speech_word_bank_t;

static void pl_speech_word_bank_reset(pl_speech_word_bank_t *b)
{
    int i;
    b->loaded_bank = -1;
    b->num_frames = 0;
    b->num_words = 0;
    for (i = 0; i < PL_SPEECH_LPC_MAX_WORDS; ++i) { b->word_offset[i] = 0; b->word_frame[i] = 0; }
    b->decoder_word = -1;
    b->decoder_frame = -1;
}
static void pl_speech_word_bank_init(pl_speech_word_bank_t *b)
{
    pl_speech_word_decoder_init(&b->decoder, 0);
    pl_speech_word_bank_reset(b);
}
/* GetWordBoundaries(address, start, end) */
static inline void pl_speech_word_bank_get_word_boundaries(const pl_speech_word_bank_t *b, float address, int *start,
                                                           int *end)
{
    if (b->num_words == 0) {
        *start = *end = -1;
    } else {
        int word = (int)(address * (float)b->num_words);
        if (word >= b->num_words) word = b->num_words - 1;
        *start = (int)b->word_frame[word];
        *end = (int)b->word_frame[word + 1] - 1;
    }
}
/* Load(bank): 1 when a new bank was walked (the player restarts), 0 when it was the loaded one already. The walk is
 * the C++'s Load: LoadNextWord per word (decoding and counting the frames, keeping none), word by word to the end. */
static int pl_speech_word_bank_load(pl_speech_word_bank_t *b, int bank)
{
    if (bank == b->loaded_bank || bank >= (int)PL_LPC_NUM_WORD_BANKS) return 0;

    b->num_frames = 0;
    b->num_words = 0;

    const uint8_t *bank_data = pl_lpc_word_banks[bank].data;
    const uint8_t *data = bank_data;
    size_t size = pl_lpc_word_banks[bank].size;

    while (size) {
        if (b->num_words + 1 >= PL_SPEECH_LPC_MAX_WORDS) break;   /* (never: 26 words at most) */
        b->word_offset[b->num_words] = (uint16_t)(data - bank_data);
        b->word_frame[b->num_words] = (uint16_t)b->num_frames;

        pl_speech_word_decoder_t d;
        pl_speech_word_decoder_init(&d, data);
        while (pl_speech_word_decoder_next(&d)) ++b->num_frames;      /* LoadNextWord */
        size_t consumed = (size_t)(d.bitstream.p - data);

        data += consumed;
        size -= consumed;
        ++b->num_words;
    }
    b->word_offset[b->num_words] = (uint16_t)(data - bank_data);
    b->word_frame[b->num_words] = (uint16_t)b->num_frames;
    b->loaded_bank = bank;
    b->decoder_word = -1;
    b->decoder_frame = -1;
    return 1;
}
/* frames()[i] and frames()[i + 1] of the C++, decoded: f1 from the start of frame i's word (or on from where the
 * last read stopped, when that is at or before i in the same word), f2 one frame further (a copy of the decoder
 * steps, so the stored one stays at i: the same read next block costs one frame). i + 1 == num_frames: a zero frame. */
static void pl_speech_word_bank_get_frames(pl_speech_word_bank_t *b, int i, pl_speech_frame_t *f1,
                                           pl_speech_frame_t *f2)
{
    int w;
    if (b->loaded_bank < 0 || i < 0 || i >= b->num_frames) {
        pl_speech_frame_clear(f1);
        pl_speech_frame_clear(f2);
        return;
    }
    const uint8_t *bank_data = pl_lpc_word_banks[b->loaded_bank].data;
    for (w = 0; w + 1 < b->num_words && (int)b->word_frame[w + 1] <= i; ++w) ;
    if (b->decoder_word != w || b->decoder_frame < 0 || b->decoder_frame > i) {
        pl_speech_word_decoder_init(&b->decoder, bank_data + b->word_offset[w]);
        b->decoder_word = w;
        b->decoder_frame = (int)b->word_frame[w] - 1;
    }
    while (b->decoder_frame < i) {
        pl_speech_word_decoder_next(&b->decoder);
        ++b->decoder_frame;
    }
    pl_speech_frame_copy(f1, &b->decoder.frame);

    if (i + 1 < (int)b->word_frame[w + 1]) {
        pl_speech_word_decoder_t d;
        pl_speech_word_decoder_copy(&d, &b->decoder);
        pl_speech_word_decoder_next(&d);
        pl_speech_frame_copy(f2, &d.frame);
    } else if (i + 1 < b->num_frames) {
        pl_speech_word_decoder_t d;
        pl_speech_word_decoder_init(&d, bank_data + b->word_offset[w + 1]);
        pl_speech_word_decoder_next(&d);
        pl_speech_frame_copy(f2, &d.frame);
    } else {
        pl_speech_frame_clear(f2);
    }
}

/* PlayFrame(frames, frame, interpolate): bank -1 plays the phonemes array, a bank its decoded frames */
static inline void pl_speech_lpc_play_frame(pl_speech_lpc_t *l, pl_speech_word_bank_t *word_bank, int bank,
                                            float frame, int interpolate)
{
    PL_MAKE_INTEGRAL_FRACTIONAL(frame)
    if (!interpolate) frame_fractional = 0.0f;
    if (bank == -1) {
        pl_speech_lpc_play_frame2(l, &pl_speech_phonemes[frame_integral], &pl_speech_phonemes[frame_integral + 1],
                                  frame_fractional);
    } else {
        pl_speech_frame_t f1, f2;
        pl_speech_word_bank_get_frames(word_bank, frame_integral, &f1, &f2);
        pl_speech_lpc_play_frame2(l, &f1, &f2, frame_fractional);
    }
}

/* LPCSpeechSynthController */
typedef struct {
    float clock_phase;
    float sample[2], next_sample[2];
    float gain;
    pl_speech_lpc_t synth;
    int playback_frame, last_playback_frame;
    size_t remaining_frame_samples;
} pl_speech_lpc_ctl_t;

static void pl_speech_lpc_ctl_init(pl_speech_lpc_ctl_t *c)
{
    c->clock_phase = 0.0f;
    c->playback_frame = -1;
    c->last_playback_frame = -1;
    c->remaining_frame_samples = 0;
    c->sample[0] = c->sample[1] = 0.0f;
    c->next_sample[0] = c->next_sample[1] = 0.0f;
    c->gain = 0.0f;
    pl_speech_lpc_init(&c->synth);
}

/* Render(free_running, trigger, bank, frequency, prosody_amount, speed, address, formant_shift, gain, excitation,
 * output, size); word_bank is the engine's (the C++ keeps a pointer to it). bank -1: the phonemes. */
static void pl_speech_lpc_ctl_render(pl_speech_lpc_ctl_t *c, pl_speech_word_bank_t *word_bank, int free_running,
                                     int trigger, int bank, float frequency, float prosody_amount, float speed,
                                     float address, float formant_shift, float gain, float *excitation, float *output,
                                     size_t size)
{
    const float rate_ratio = pl_semitones_to_ratio((formant_shift - 0.5f) * 36.0f);
    const float rate = rate_ratio / 6.0f;

    /* All utterances have been normalized for an average f0 of 100 Hz. */
    const float pitch_shift = frequency / (rate_ratio * PL_SPEECH_LPC_DEFAULT_F0 / PL_CORRECTED_SAMPLE_RATE);
    const float time_stretch = pl_semitones_to_ratio(-speed * 24.0f +
        (formant_shift < 0.4f ? (formant_shift - 0.4f) * -45.0f
            : (formant_shift > 0.6f ? (formant_shift - 0.6f) * -45.0f : 0.0f)));

    if (bank != -1) {
        int reset_everything = pl_speech_word_bank_load(word_bank, bank);
        if (reset_everything) {
            c->playback_frame = -1;
            c->last_playback_frame = -1;
        }
    }

    const int num_frames = bank == -1 ? PL_SPEECH_LPC_NUM_VOWELS : word_bank->num_frames;

    if (trigger) {
        if (bank == -1) {
            /* Pick a pseudo-random consonant, and play it for the duration of a frame. */
            int r = (int)((address + 3.0f * formant_shift + 7.0f * frequency) * 8.0f);
            c->playback_frame = (r % PL_SPEECH_LPC_NUM_CONSONANTS);
            c->playback_frame += PL_SPEECH_LPC_NUM_VOWELS;
            c->last_playback_frame = c->playback_frame + 1;
        } else {
            pl_speech_word_bank_get_word_boundaries(word_bank, address, &c->playback_frame, &c->last_playback_frame);
        }
        c->remaining_frame_samples = 0;
    }

    if (c->playback_frame == -1 && c->remaining_frame_samples == 0) {
        pl_speech_lpc_play_frame(&c->synth, word_bank, bank, address * ((float)num_frames - 1.0001f), 1);
    } else {
        if (c->remaining_frame_samples == 0) {
            pl_speech_lpc_play_frame(&c->synth, word_bank, bank, (float)c->playback_frame, 0);
            c->remaining_frame_samples = (size_t)(PL_SAMPLE_RATE / PL_SPEECH_LPC_FPS * time_stretch);
            ++c->playback_frame;
            if (c->playback_frame >= c->last_playback_frame) {
                int back_to_scan_mode = bank == -1 || free_running;
                c->playback_frame = back_to_scan_mode ? -1 : c->last_playback_frame;
            }
        }
        c->remaining_frame_samples -= size < c->remaining_frame_samples ? size : c->remaining_frame_samples;
    }

    pl_interp_t gain_modulation;
    pl_interp_init(&gain_modulation, &c->gain, gain, size);

    while (size--) {
        float this_sample[2];
        this_sample[0] = c->next_sample[0];
        this_sample[1] = c->next_sample[1];
        c->next_sample[0] = 0.0f;
        c->next_sample[1] = 0.0f;

        c->clock_phase += rate;
        if (c->clock_phase >= 1.0f) {
            c->clock_phase -= 1.0f;
            float reset_time = c->clock_phase / rate;
            float new_sample[2];

            pl_speech_lpc_render(&c->synth, prosody_amount, pitch_shift, &new_sample[0], &new_sample[1], 1);

            float discontinuity[2] = { new_sample[0] - c->sample[0], new_sample[1] - c->sample[1] };
            this_sample[0] += discontinuity[0] * pl_this_blep(reset_time);
            c->next_sample[0] += discontinuity[0] * pl_next_blep(reset_time);
            this_sample[1] += discontinuity[1] * pl_this_blep(reset_time);
            c->next_sample[1] += discontinuity[1] * pl_next_blep(reset_time);
            c->sample[0] = new_sample[0];
            c->sample[1] = new_sample[1];
        }
        c->next_sample[0] += c->sample[0];
        c->next_sample[1] += c->sample[1];
        const float g = pl_interp_next(&gain_modulation);
        *excitation++ = this_sample[0] * g;
        *output++ = this_sample[1] * g;
    }
    pl_interp_done(&gain_modulation);
}

/* (Orange Cat) the decaying states to 0 once tiny (pl_dsp.h pl_flush) */
static inline void pl_speech_lpc_ctl_flush(pl_speech_lpc_ctl_t *c)
{
    pl_flush_n(c->synth.s, PL_SPEECH_LPC_ORDER + 1);
    pl_flush(&c->synth.next_sample);
    pl_flush_n(c->sample, 2);
    pl_flush_n(c->next_sample, 2);
}
