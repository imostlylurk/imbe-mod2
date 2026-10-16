/* bench.c: in-process per-frame benchmark, corpus in memory.
 * usage: bench enc|encenh|dec corpus.raw passes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "imbe2.h"

static unsigned char enc_state[16384], dec_state[16384];
static int16_t speech[160], out[160];
static uint8_t bits[11];

static double now_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1e6 + ts.tv_nsec / 1e3;
}

int main(int argc, char **argv)
{
    if (argc != 4) { fprintf(stderr, "usage: bench enc|encenh|dec corpus passes\n"); return 1; }
    int enc_mode = !strcmp(argv[1], "enc");
    int enh_mode = !strcmp(argv[1], "encenh");
    int passes = atoi(argv[3]);
    FILE *f = fopen(argv[2], "rb");
    if (!f) { perror(argv[2]); return 1; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    int frames = n / 320;
    int16_t *corpus = malloc(frames * 320);
    if (fread(corpus, 320, frames, f) != (size_t)frames) { fprintf(stderr, "short read\n"); return 1; }
    fclose(f);
    uint8_t *framebits = malloc((size_t)frames * 11);
    if (enc_mode || enh_mode)
    {
        imbe2_encoder_init(enc_state);
        if (enh_mode) imbe2_encoder_set_enh(enc_state, 1);
        /* pre-encode once for the decode path input */
        for (int fr = 0; fr < frames; fr++)
            imbe2_encode(enc_state, &framebits[fr * 11], &corpus[fr * 160]);
        imbe2_encoder_init(enc_state);
        double best = 1e18, worst = 0, total = 0;
        for (int p = 0; p < passes; p++)
        {
            imbe2_encoder_init(enc_state);
            if (enh_mode) imbe2_encoder_set_enh(enc_state, 1);
            double t0 = now_us(), tmax = 0;
            for (int fr = 0; fr < frames; fr++)
            {
                double f0 = now_us();
                imbe2_encode(enc_state, bits, &corpus[fr * 160]);
                double dt = now_us() - f0;
                if (dt > tmax) tmax = dt;
            }
            double dt = now_us() - t0;
            if (dt < best) best = dt;
            if (tmax > worst) worst = tmax;
            total += dt;
        }
        printf("%s %d frames/pass: avg %.1f us/frame (best pass %.1f), worst frame %.1f us\n",
               argv[1], frames, total / passes / frames, best / frames, worst);
    }
    else
    {
        imbe2_encoder_init(enc_state);
        for (int fr = 0; fr < frames; fr++)
            imbe2_encode(enc_state, &framebits[fr * 11], &corpus[fr * 160]);
        imbe2_decoder_init(dec_state);
        double best = 1e18, worst = 0, total = 0;
        for (int p = 0; p < passes; p++)
        {
            imbe2_decoder_init(dec_state);
            double t0 = now_us(), tmax = 0;
            for (int fr = 0; fr < frames; fr++)
            {
                double f0 = now_us();
                imbe2_decode(dec_state, out, &framebits[fr * 11]);
                double dt = now_us() - f0;
                if (dt > tmax) tmax = dt;
            }
            double dt = now_us() - t0;
            if (dt < best) best = dt;
            if (tmax > worst) worst = tmax;
            total += dt;
        }
        printf("dec %d frames/pass: avg %.1f us/frame (best pass %.1f), worst frame %.1f us\n",
               frames, total / passes / frames, best / frames, worst);
    }
    return 0;
}
