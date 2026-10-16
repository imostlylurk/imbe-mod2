/* imbe2_api.cc: codec2-mod style C API over the split imbe engine. */

#include <new>
#include <stddef.h>

#include "imbe2.h"
#include "imbe_vocoder.h"

extern "C" {

struct imbe2
{
    imbe_encoder enc; /* first member: imbe2_encode(&combined) works */
    imbe_decoder dec;
};

size_t imbe2_state_size(void)
{
    return sizeof(imbe2_t);
}

void imbe2_init(void *state)
{
    new (state) imbe2_t();
}

void *imbe2_encoder_from(void *combined)
{
    return &((imbe2_t *)combined)->enc;
}

void *imbe2_decoder_from(void *combined)
{
    return &((imbe2_t *)combined)->dec;
}

/* split states: encoder-only / decoder-only objects */

size_t imbe2_encoder_state_size(void)
{
    return sizeof(imbe_encoder);
}

size_t imbe2_decoder_state_size(void)
{
    return sizeof(imbe_decoder);
}

void imbe2_encoder_init(void *state)
{
    new (state) imbe_encoder();
}

void imbe2_decoder_init(void *state)
{
    new (state) imbe_decoder();
}

static void pack_words(const int16_t *w, uint8_t *out)
{
    /* 8 words x 11 bits = 88 bits, word 0 in the LOW 11 bits,
       big-endian byte order. */
    unsigned __int128 all = 0;
    for (int i = 7; i >= 0; i--)
        all = (all << 11) | (unsigned)(w[i] & 0x7FF);
    for (int i = 0; i < 11; i++)
        out[i] = (uint8_t)((all >> (8 * (10 - i))) & 0xFF);
}

static void unpack_words(const uint8_t *in, int16_t *w)
{
    unsigned __int128 all = 0;
    for (int i = 0; i < 11; i++)
        all = (all << 8) | in[i];
    for (int i = 0; i < 8; i++)
    {
        w[i] = (int16_t)(all & 0x7FF);
        all >>= 11;
    }
}

void imbe2_encode(void *state, uint8_t *bits, const int16_t *speech)
{
    int16_t words[8];
    ((imbe_encoder *)state)->imbe_encode(words,
                                         const_cast<int16_t *>(speech));
    pack_words(words, bits);
}

void imbe2_decode(void *state, int16_t *speech, const uint8_t *bits)
{
    int16_t words[8];
    unpack_words(bits, words);
    ((imbe_decoder *)state)->imbe_decode(words, speech);
}

} /* extern "C" */
