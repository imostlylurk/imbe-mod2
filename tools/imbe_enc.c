/* imbe_enc: encode raw 8 kHz mono s16le speech to an IMBE file.
 *
 * usage: imbe_enc input.raw output.imbe [--raw]
 *
 * Writes an 8-byte header ("IMB1", version, reserved) so files are
 * self-describing; --raw writes headerless 11-byte frames (the
 * standard 88-bit prioritized info stream, u0 first).  Pads the
 * final partial frame with silence.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef ENC_DIAG_MISSING_GUARD
#endif
int enc_diag_frame; /* diag frame counter (engine ENC_DIAG) */

#include "imbe2.h"

int main(int argc, char *argv[])
{
    int raw = argc == 4 && !strcmp(argv[3], "--raw");
    if ((argc != 3 && argc != 4) || (argc == 4 && !raw))
    {
        fprintf(stderr, "usage: %s input.raw output.imbe [--raw]\n", argv[0]);
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
        uint8_t hdr[8] = {'I', 'M', 'B', '1', IMBE2_FILE_VERSION, 0, 0, 0};
        fwrite(hdr, 1, sizeof(hdr), fout);
    }

    static unsigned char state[8192]; /* >= imbe2_state_size() */
    if (imbe2_encoder_state_size() > sizeof(state))
    {
        fprintf(stderr, "state too small: need %zu\n", imbe2_state_size());
        return 1;
    }
    imbe2_encoder_init(state);

    int16_t speech[IMBE2_SAMPLES_PER_FRAME];
    uint8_t bits[IMBE2_BYTES_PER_FRAME];
    long frames = 0;
    size_t n;

    while ((n = fread(speech, 2, IMBE2_SAMPLES_PER_FRAME, fin)) > 0)
    {
        enc_diag_frame++;
        for (size_t i = n; i < IMBE2_SAMPLES_PER_FRAME; i++)
            speech[i] = 0;
        imbe2_encode(state, bits, speech);
        fwrite(bits, 1, sizeof(bits), fout);
        frames++;
    }

    fclose(fin);
    fclose(fout);
    fprintf(stderr, "%s: %ld frames (%.2f s)\n", argv[2], frames,
            frames * 0.02);
    return 0;
}
