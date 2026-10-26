# DUB-TAPE: multitap dub feedback delay research for the M-VAVE FM-1

Independent research prototype for Orange Cat's sound-generation side. It is isolated from the Orange Cat repository:
no production file was changed, and nothing ran on the hardware.

**Result in one paragraph.** A single circulating mono int16 line with up to four virtual playback heads at
multiples of TIME. Feedback runs from a weighted head mix through HP, a resonant LP (FEEDBACK calibrated at the resonant
peak) and a cubic saturator before it is rewritten. This gives convincing dub behaviour: rhythmic multi-head echoes,
progressive darkening (8 repeats: the 8 kHz band −84 dB vs −12 dB at 1 kHz), filter sweeps on a self-regenerating loop,
and bounded runaways above unity feedback. It costs 32 KB for 0.74 s at a 22.05 kHz line and an estimated 3.5–6 % of one
360 MHz core. Recommendation and integration notes: [`docs/HANDOFF.md`](docs/HANDOFF.md).

## Deliverables

| # | deliverable | where |
|---|---|---|
| 1 | annotated source survey with licences | [`docs/SOURCE_SURVEY.md`](docs/SOURCE_SURVEY.md) |
| 2 | algorithm descriptions (block diagrams, maths) | [`docs/DESIGN.md`](docs/DESIGN.md) |
| 3 | portable C99 prototypes | `src/dubdelay.[ch]`, `src/dd_math.h`; test scenes `src/dd_scenes.h` |
| 4 | reproducible WAV renders | `renders/` (`make all-results`; list in [`docs/RENDERS.csv`](docs/RENDERS.csv)) |
| 5 | measurements (timing, decay, spectra, stability, saturation, aliasing, noise, artifacts, bypass) | [`docs/MEASUREMENTS.md`](docs/MEASUREMENTS.md) (generated), summary in [`docs/EVALUATION.md`](docs/EVALUATION.md) |
| 6 | RAM and CPU for 1 and 4 instances | [`docs/EVALUATION.md` §8](docs/EVALUATION.md), [`docs/BENCH_HOST.md`](docs/BENCH_HOST.md) |
| 7 | comparison of the algorithms | [`docs/EVALUATION.md` §9](docs/EVALUATION.md) |
| 8 | recommended FM-1 implementation | [`docs/HANDOFF.md`](docs/HANDOFF.md) |
| 9 | handoff for the integration session | [`docs/HANDOFF.md`](docs/HANDOFF.md) |
| | controls and presets | [`docs/PARAMETERS.md`](docs/PARAMETERS.md) |

## Reproduce

```sh
make                # host tools: build/render, build/measure, build/bench
make all-results    # renders/*.wav, docs/RENDERS.csv, docs/MEASUREMENTS.md, docs/BENCH_HOST.md (~1 min)
make fm1            # cross-compile the DSP with Orange Cat's pi32v2 flags and print the loop instruction counts
```

Requirements: a C99 compiler and libm for the host tools; Python 3 only for `tools/loops.py` and `tools/envelope.py`.
`make fm1` expects the Jieli toolchain at `~/fm1-toolchain/jieli/jieli-linux-toolchains-20260730.1`
(override with `JIELI_TOOLCHAIN=`).

## Limits of the evidence

No listening test by a person, no comparison with any hardware echo or reference recording (no claim of matching an
RE-201 or any other unit), no FM-1 execution. FM-1 CPU figures are static instruction counts; host timings are a
relative proxy measured on a loaded machine.

Licence: GPL-3.0-only (SPDX headers), matching Orange Cat. No third-party code was copied.
