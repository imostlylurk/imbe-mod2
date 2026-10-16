/* imbe_dec: decode an IMBE file to raw 8 kHz mono s16le speech.
 *
 * usage: imbe_dec input.imbe output.raw [--raw]
 *
 * Honours the 8-byte "IMB1" header if present; --raw forces
 * headerless interpretation (11 bytes per frame, word 0 in the low
 * bits, big-endian).
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "imbe2.h"

int main(int argc, char *argv[])
{
    int raw = argc == 4 && !strcmp(argv[3], "--raw");
    if ((argc != 3 && argc != 4) || (argc == 4 && !raw))
    {
        fprintf(stderr, "usage: %s input.imbe output.raw [--raw]\n", argv[0]);
        return 1;
    }

    FILE *fin = fopen(argv[1], "rb");
    if (!fin)
    {
        perror(argv[1]);
        return 1;
    }
    FILE *fout = fopen(argv[2], "wb");
    if (!fout)
    {
        perror(argv[2]);
        fclose(fin);
        return 1;
    }

    if (!raw)
    {
        uint8_t hdr[8];
        if (fread(hdr, 1, sizeof(hdr), fin) == sizeof(hdr) &&
            !memcmp(hdr, "IMB1", 4))
        {
            if (hdr[4] != IMBE2_FILE_VERSION)
                fprintf(stderr, "warning: file version %d, built for %d\n",
                        hdr[4], IMBE2_FILE_VERSION);
        }
        else
        {
            fseek(fin, 0, SEEK_SET); /* headerless after all */
        }
    }

    static unsigned char state[8192];
    if (imbe2_decoder_state_size() > sizeof(state))
    {
        fprintf(stderr, "state too small: need %zu\n", imbe2_state_size());
        return 1;
    }
    imbe2_decoder_init(state);

    uint8_t bits[IMBE2_BYTES_PER_FRAME];
    int16_t speech[IMBE2_SAMPLES_PER_FRAME];
    long frames = 0;

    while (fread(bits, 1, sizeof(bits), fin) == sizeof(bits))
    {
        imbe2_decode(state, speech, bits);
        fwrite(speech, 2, IMBE2_SAMPLES_PER_FRAME, fout);
        frames++;
    }

    fclose(fin);
    fclose(fout);
    fprintf(stderr, "%s: %ld frames (%.2f s)\n", argv[2], frames,
            frames * 0.02);
    return 0;
}
