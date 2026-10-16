# imbe-mod2

IMBE (4400 bps) vocoder with a clean C API, split encoder/decoder
state and file tools.

Derived from [imbe-mod](https://github.com/imostlylurk/imbe-mod) (same
vocoding logic, bit-exact): Pavel Yazev's fixed-point IMBE
encoder/decoder (via op25, GPLv3+) with the e_p result caching,
LTO-friendly build and guarded exact-sum autocorrelation.  Encoder
~5.3x / decoder ~1.75x faster than the unmodified op25 code, with
byte-identical output.

This project repackages that engine for standalone use:

- **C API with caller-owned static state** (`inc/imbe2.h`): no heap
  after init, no create/destroy pair, state sizes queryable at runtime
  for static allocation.
- **Split encoder/decoder objects**: all parameter exchange happens
  through the frame bits, so each side is fully self-contained - an
  RX-only build needs 1,932 B of codec state.
- **`imbe_enc` / `imbe_dec`** file tools.
- Self-describing `.imbe` file format (8-byte `IMB1` header) with a
  `--raw` mode for headerless 11-byte frames (word 0 in the low bits,
  big-endian - the same layout as imbe-mod's frame dumps).

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

## Tools

```
imbe_enc input.raw output.imbe          # 8-byte IMB1 header
imbe_enc input.raw output.imbe --raw    # headerless 11-byte frames
imbe_dec output.imbe out.raw            # auto-detects the header
imbe_dec output.imbe out.raw --raw      # force headerless
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

Same engine as imbe-mod (x86-64, 28 s corpus): encode 64 us /
decode 25 us per 20 ms frame, init 2 us.  The only addition here is
the packing/unpacking in the API layer (a few ns per frame).
