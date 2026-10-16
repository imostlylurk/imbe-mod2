/* imbe2test: in-process full-chain test + bit-exactness check.
 *
 * 1. Synthesises a speech-like signal, encodes + decodes, checks
 *    voiced/silence energy and output sanity.
 * 2. If testdata/hts1a.raw exists, encodes it and compares the frames
 *    byte-for-byte against the golden oracle from imbe-mod.
 */

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "imbe2.h"

#define N_FRAMES 60

static double rms(const int16_t *s, int n)
{
    double acc = 0;
    for (int i = 0; i < n; i++)
        acc += (double)s[i] * s[i];
    return sqrt(acc / n);
}

int main(void)
{
    static unsigned char enc_state[8192], dec_state[8192];
    if (imbe2_state_size() > sizeof(enc_state))
    {
        printf("state size %zu too large for test buffer\n",
               imbe2_state_size());
        return 1;
    }
    printf("state sizes: combined %zu, encoder-only %zu, decoder-only %zu\n",
           imbe2_state_size(), imbe2_encoder_state_size(),
           imbe2_decoder_state_size());

    /* exercise the split API: separate encoder and decoder objects */
    imbe2_encoder_init(enc_state);
    imbe2_decoder_init(dec_state);

    /* ---- 1. synthetic round-trip ---- */
    static int16_t out[N_FRAMES * 160];
    uint8_t bits[IMBE2_BYTES_PER_FRAME];
    int n = 0;
    for (int f = 0; f < N_FRAMES; f++)
    {
        int seg = f % 4; /* voiced, voiced, noise, silence per 20ms */
        int16_t speech[160];
        for (int i = 0; i < 160; i++, n++)
        {
            double t = (double)n / 8000.0;
            float v = 0;
            if (seg == 0)
                v = 5000.f * sinf(2.f * (float)M_PI * 180.f * t) *
                    (0.6f + 0.4f * sinf(2.f * (float)M_PI * 3.f * t));
            else if (seg == 1)
                v = 4000.f * sinf(2.f * (float)M_PI * 120.f * t) +
                    1200.f * sinf(2.f * (float)M_PI * 1500.f * t);
            else if (seg == 2)
                v = 1500.f *
                    ((int)(((unsigned)n * 1103515245u + 12345u) >> 16) -
                     32768) / 32768.f;
            speech[i] = (int16_t)v;
        }
        imbe2_encode(enc_state, bits, speech);
        imbe2_decode(dec_state, &out[f * 160], bits);
    }

    int fail = 0;
    double overall = rms(out, N_FRAMES * 160);
    int clipped = 0;
    for (int i = 0; i < N_FRAMES * 160; i++)
        if (out[i] == 32767 || out[i] == -32768)
            clipped++;
    if (overall < 100.0 || overall > 20000.0)
    {
        printf("  synthetic: FAIL overall rms %.1f\n", overall);
        fail = 1;
    }
    if (clipped > (N_FRAMES * 160) / 100)
    {
        printf("  synthetic: FAIL %d clipped samples\n", clipped);
        fail = 1;
    }
    printf("  synthetic: rms %.1f, %d clipped %s\n", overall, clipped,
           fail ? "FAIL" : "PASS");

    /* ---- 2. golden bit-exactness ---- */
    imbe2_encoder_init(enc_state); /* fresh state, as the goldens were */
    FILE *f = fopen("testdata/hts1a.raw", "rb");
    if (!f)
    {
        printf("  golden: SKIPPED (no testdata)\n");
        printf("%s\n", fail ? "TESTS FAILED" : "ALL PASS (golden skipped)");
        return fail;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    int16_t *pcm = malloc(sz);
    fread(pcm, 1, sz, f);
    fclose(f);
    long frames = sz / 2 / 160;

    FILE *g = fopen("testdata/golden_hts1a.frames", "rb");
    if (!g)
    {
        printf("  golden: SKIPPED (no golden file)\n");
        return fail;
    }
    long matched = 0, total = 0;
    for (long i = 0; i < frames; i++)
    {
        imbe2_encode(enc_state, bits, &pcm[i * 160]);
        uint8_t golden[IMBE2_BYTES_PER_FRAME];
        if (fread(golden, 1, sizeof(golden), g) == sizeof(golden))
        {
            total++;
            if (!memcmp(bits, golden, sizeof(bits)))
                matched++;
        }
    }
    fclose(g);
    free(pcm);
    if (matched != total)
    {
        printf("  golden: FAIL %ld/%ld frames match\n", matched, total);
        fail = 1;
    }
    else
        printf("  golden: %ld/%ld frames bit-exact vs imbe-mod PASS\n",
               matched, total);

    printf("%s\n", fail ? "TESTS FAILED" : "ALL PASS");
    return fail;
}
