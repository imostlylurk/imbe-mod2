#!/bin/bash
# size_report.sh: static Cortex-M4 footprint (codec2-mod style).
set -e
cd "$(dirname "$0")/.."
OUT=build-size
CC=arm-none-eabi-gcc
CFLAGS="-Os -mcpu=cortex-m4 -mthumb -ffunction-sections -fdata-sections -specs=nano.specs -Iinc"
mkdir -p $OUT

if [ ! -f $OUT/syscalls.o ]; then
    $CC $CFLAGS -c $OUT/syscalls.c -o $OUT/syscalls.o
fi

for s in src/*.cc; do
    o=$OUT/$(basename "$s" .cc)_n.o
    if [ ! -f "$o" ] || [ "$s" -nt "$o" ]; then
        $CC $CFLAGS -fno-exceptions -fno-rtti -c "$s" -o "$o"
    fi
done

cat > $OUT/main_enc.c << 'MEOF'
#include <stdint.h>
#include <string.h>
#include "imbe2.h"
static unsigned char enc_state[16384];
volatile int sink;
int main(void) {
    int16_t speech[160];
    uint8_t bits[11];
    memset(speech, 0, sizeof speech);
    imbe2_encoder_init(enc_state);
    imbe2_encode(enc_state, bits, speech);
    sink = bits[0] + (int)imbe2_encoder_state_size();
    return sink;
}
MEOF

cat > $OUT/main_dec.c << 'MEOF'
#include <stdint.h>
#include <string.h>
#include "imbe2.h"
static unsigned char dec_state[16384];
volatile int sink;
int main(void) {
    int16_t out[160];
    uint8_t bits[11];
    memset(bits, 0, sizeof bits);
    imbe2_decoder_init(dec_state);
    imbe2_decode(dec_state, out, bits);
    sink = out[0] + (int)imbe2_decoder_state_size();
    return sink;
}
MEOF

$CC $CFLAGS -fno-exceptions -fno-rtti -c $OUT/main_enc.c -o $OUT/main_enc.o
$CC $CFLAGS -fno-exceptions -fno-rtti -c $OUT/main_dec.c -o $OUT/main_dec.o
for t in enc dec; do
    $CC $CFLAGS -nostartfiles -Wl,--gc-sections -Wl,-e,main \
        $OUT/main_$t.o $OUT/syscalls.o $OUT/*_n.o -lgcc -lm -o $OUT/img_$t.elf
    arm-none-eabi-size $OUT/img_$t.elf | awk -v tag=$t 'NR==2{printf "%-4s flash(text+rodata) %7d  RAM(data+bss) %7d\n", tag, $1, $2+$3}'
done
