# imbe-mod2

IMBE (4400 bps) vocoder with a clean C API, split encoder/decoder
state and file tools.

Derived from [imbe-mod](https://github.com/imostlylurk/imbe-mod) (same
vocoding logic, bit-exact): Pavel Yazev's fixed-point IMBE
encoder/decoder (via op25, GPLv3+) with the e_p result caching,
LTO-friendly build and guarded exact-sum autocorrelation.  Encoder
~5.3x / decoder ~1.75x faster than the unmodified op25 code, with
byte-identical output.

This project repackages that engine, with an improved encoder, for
standalone use:

- **C API with caller-owned static state** (`inc/imbe2.h`): no heap
  after init, no create/destroy pair, state sizes queryable at runtime
  for static allocation.
- **Split encoder/decoder objects**: all parameter exchange happens
  through the frame bits, so each side is fully self-contained - an
  RX-only build needs 1,932 B of codec state.
- **`imbe_enc` / `imbe_dec`** file tools.
- Self-describing `.imbe` file format (8-byte `IMB1` header, version 2)
  with a `--raw` mode for headerless 11-byte frames carrying the
  standard 88-bit prioritized info stream.
- **Corrected MONO65 gain ladder**: the bottom 8 levels follow the
  reference firmware (Annex-E minus 0.25*(8-k), Q11); this alone drops
  the digital-silence decode floor from RMS 33 to 11.
- **Two encoders**: `imbe_enc` is bit-exact with imbe-mod (the
  performance-optimised original); `imbe_enc2` adds the enhanced
  analysis - leashed-continuity pitch tracking, wider voicing
  hysteresis, stricter octave-switch gating and a min-statistics
  noise gate (see below).  Same 88-bit frames out.

## Frame format

20 ms frames: 160 samples (8 kHz mono s16le) <-> 11 bytes (88 bits,
8 x 11-bit u-vectors, word 0 in the low bits, big-endian byte order).
4400 bps.

## API

```c
#include "imbe2.h"

/* split (encoder-only / decoder-only) */
static unsigned char enc[8192], dec[8192];
imbe2_encoder_init(enc);            /* 6064 B, 8608 B with the noise-gate history */
imbe2_decoder_init(dec);            /* 1932 bytes  */

int16_t speech[IMBE2_SAMPLES_PER_FRAME]; /* 160 */
uint8_t bits[IMBE2_BYTES_PER_FRAME];     /* 11  */
imbe2_encode(enc, bits, speech);
imbe2_decode(dec, speech, bits);

/* or combined full-duplex object (7996 bytes) with accessors */
static unsigned char both[8192];
imbe2_init(both);
imbe2_encode(imbe2_encoder_from(both), bits, speech);
imbe2_decode(imbe2_decoder_from(both), speech, bits);
```

Every object is caller-owned static allocation; no heap after init.

## State sizes

| Object | Size |
|--------|------|
| combined | 7,996 B |
| encoder-only | 6,064 B (8,608 B with enh level 2) |
| decoder-only | 1,932 B |

The split grows the combined footprint slightly (each side carries
its own frame workspace, FFT twiddles and voicing-decision array);
decoder-only saves 76 % vs combined.

## Enhanced encoder (`imbe_enc2`, `imbe2_encoder_set_enh`)

Measured on a 28 s speech corpus (1399 frames):

- **Leashed-continuity pitch tracking** (expired-patent soft-decision
  tracking, US4,885,790/US5,081,681): among candidates whose e_p cost
  is within 0.024 of the raw minimum - on both the look-back and the
  3-window look-ahead paths - prefer the pitch nearest the previous
  frame.  Pitch-track oscillations 462 -> 240 (warble), with the
  spectral fit essentially unchanged.
- **Wider voicing hysteresis** (0.40/0.62 vs 0.45/0.5625) and
  stricter sub-multiple (octave) switch evidence (0.75 vs 0.588):
  octave errors 145 -> 122, v/uv flap 10.5 % -> 9.5 %.
- **Noise gate** (time domain): min-statistics floor (3 s windowed
  minimum of frame power), asymmetric hysteresis (open at E > 25*Fmin,
  close at E < 4*Fmin), closed frames muted to true digital silence
  before analysis.  Settled-pause decode RMS 88 -> 36, speech
  untouched.  Gating must be a real zero - synthesised partial gain
  steps ring the predictive log2-DCT SA quantizer.

## Tools

```
imbe_enc  input.raw output.imbe         # standard encoder (bit-exact imbe-mod)
imbe_enc2 input.raw output.imbe         # enhanced encoder
... --raw                               # headerless 11-byte frames either way
imbe_dec  output.imbe out.raw           # auto-detects the header
imbe_dec  output.imbe out.raw --raw     # force headerless
```

## Build and test

```
cmake -B build && cmake --build build
cd build && ctest
```

`imbe2test` runs an in-process synthetic round-trip test (sane
energy, no clipping) and - when a golden oracle is placed in
`testdata/` - a bit-exactness check of every encoded frame against
it.  The oracle can be regenerated with imbe-mod's `imbebench
--encode` from any 8 kHz mono s16le speech file; with the corpus
used during development (150-frame and 1399-frame male speech) the
match was byte-for-byte for frames and decoded PCM, including with
independent encoder/decoder objects.

## Performance

Host (x86-64, 28 s corpus, 30 passes, in-process `imbebench`):
encode 64 us avg / 250 us worst, decode 32 us avg per 20 ms frame.
The enhancements add ~0.6 % instructions over the standard encoder
(the noise gate makes pauses cheap: muted frames encode faster than
they would with noise present).

### STM32F4 projection (100 MHz, codec2-mod measurement basis)

Same method as codec2-vhf: codec2-mod measured 3200-mode encode at
5.32 ms on an STM32F411 @ 100 MHz (-Os); everything here is scaled by
differential callgrind instruction counts against that anchor
(244,781 Ir/frame for the codec2 encoder on this host, i.e.
2.17 cycles/Ir).  These are projections from measurement ratios, not
hardware runs.

| Workload | Ir/frame | @100 MHz | @120 MHz | % of 20 ms @120 MHz |
|---|---:|---:|---:|---:|
| imbe-mod2 encode (standard) | 951,236 | 20.7 ms | 17.2 ms | 86 % |
| imbe-mod2 encode (enhanced) | 956,682 | 20.8 ms | 17.3 ms | 87 % |
| imbe-mod2 decode | 370,814 | 8.1 ms | 6.7 ms | 34 % |
| blip25-mbe encode | 14,093,192 | 306.3 ms | 255.3 ms | 1276 % |
| blip25-mbe decode | 360,109 | 7.8 ms | 6.5 ms | 33 % |

Notes: the IMBE encoder is a genuine ~4x codec2-3200's cost, so on a
100 MHz M4 encode alone slightly exceeds the 20 ms frame budget;
decode is comfortable.  The blip25-mbe reference encoder (Rust, DVSI
parity analysis chain) is ~15x the Yazev encoder's instruction count
and is not real-time capable on this class of MCU; its decoder,
however, needs marginally FEWER instructions than the Yazev decoder
(360k vs 371k Ir) at reference parity - a promising replacement RX
synthesis core if a C/Rust split build is acceptable.  blip25 figures
measured on the 0.3.0 workspace (encode+decode, info-only rate);
its M4 code footprint was not measured (Rust std cross-build).

### Cortex-M4 static footprint (`tools/size_report.sh`, -Os + newlib-nano)

| Image | Flash (text+rodata) | RAM (data+bss) |
|---|---:|---:|
| encoder (+8,608 B codec state) | 19.0 KiB | 17.1 KiB |
| decoder (+1,932 B codec state) | 13.7 KiB | 16.8 KiB |

(The RAM figures include the oversized 16 KiB state buffers of the
demo images; size the buffers to `imbe2_encoder_state_size()` /
`imbe2_decoder_state_size()` in a real build.)
