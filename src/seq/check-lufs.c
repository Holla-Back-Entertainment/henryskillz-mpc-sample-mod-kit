/* check-lufs: cc -O2 check-lufs.c -lm && ./a.out   (BS.1770 / EBU Tech 3341 reference points) */
#include <assert.h>
#include <stdio.h>
#include "lufs.h"

static void run(struct lufs *L, double amp, double hz, double secs)
{
    for (long i = 0; i < (long)(secs * L->rate); ++i) {
        float x = (float)(amp * sin(2 * M_PI * hz * i / L->rate));
        lufs_frame(L, x, x);
    }
}

int main(void)
{
    static struct lufs L;
    unsigned rates[] = {44100, 48000};
    for (int k = 0; k < 2; ++k) {
        lufs_init(&L, rates[k]);
        run(&L, pow(10, -23 / 20.), 1000, 20);              /* 3341 case 1: -23 dBFS 1 kHz stereo = -23 LUFS */
        printf("%u Hz: M %.2f S %.2f I %.2f\n", rates[k], L.m, L.s, L.i);
        assert(fabs(L.m + 23) < .1 && fabs(L.s + 23) < .1 && fabs(L.i + 23) < .1);
    }
    /* 3341 case 3-ish: 10 s at -36, 60 s at -23, 10 s at -36 -> relative gate drops the quiet parts: -23 */
    lufs_init(&L, 48000);
    run(&L, pow(10, -36 / 20.), 1000, 10); run(&L, pow(10, -23 / 20.), 1000, 60); run(&L, pow(10, -36 / 20.), 1000, 10);
    printf("gated: I %.2f\n", L.i); assert(fabs(L.i + 23) < .1);
    /* silence: absolute gate holds I, then 2 s of digital silence restarts it */
    run(&L, 0, 1000, 1.5); assert(fabs(L.i + 23) < .1);
    run(&L, 0, 1000, 1); assert(isinf(L.i));
    run(&L, pow(10, -14 / 20.), 1000, 5); printf("after reset: I %.2f\n", L.i); assert(fabs(L.i + 14) < .2);  /* first blocks overlap the silence, as the spec does */
    puts("lufs: OK");
}
