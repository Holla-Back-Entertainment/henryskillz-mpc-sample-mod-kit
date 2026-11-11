/* HS Bass Focus behaviour check.
 * cc -O2 -I. check-bass-focus.c hs_bass_focus.c -lm -o /tmp/cbf && /tmp/cbf */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "ladspa.h"
#include "hs_bass_focus.h"
extern const LADSPA_Descriptor hs_bass_focus_descriptor;

enum { RATE = 48000, N = RATE };
static float in[2][N], out[2][N], ctl[BF_COUNT];
static const LADSPA_Descriptor *d = &hs_bass_focus_descriptor;
static LADSPA_Handle h;

/* load defaults as SampQOL sets them from the hints: crossover 122 Hz, bass 0 dB, mono on */
static void reset(void) { ctl[BF_FREQ] = sqrtf(30*500); ctl[BF_BASS] = .25f*-24 + .75f*8; ctl[BF_MONO] = 1; d->activate(h); }
static void tone(float hz, float r_sign) { for (int i = 0; i < N; ++i) { in[0][i] = .25f*sinf(6.2831853f*hz*i/RATE); in[1][i] = r_sign*in[0][i]; } }
static void go(void) { for (int i = 0; i < N; i += 256) {
    for (int c = 0; c < 2; ++c) { d->connect_port(h, c, in[c]+i); d->connect_port(h, 2+c, out[c]+i); }
    d->run(h, 256); } }
static float db(int c) { double a = 0, b = 0; for (int i = N/2; i < N; ++i) { a += out[c][i]*out[c][i]; b += in[c][i]*in[c][i]; } return 10*log10(a/b + 1e-30); }

int main(void)
{
    h = d->instantiate(d, RATE);
    for (int i = 0; i < BF_COUNT; ++i) d->connect_port(h, 4+i, &ctl[i]);
    assert(fabsf(.25f*-24 + .75f*8) < 1e-6f);

    /* load defaults: a mono signal passes flat at every frequency (LR4 sums to an all-pass) */
    const float hz[] = {30, 80, 122, 200, 1000, 10000};
    for (unsigned k = 0; k < sizeof hz / sizeof *hz; ++k) {
        reset(); tone(hz[k], 1); go();
        printf("defaults, %5.0f Hz: %+.2f dB\n", hz[k], db(0));
        assert(fabsf(db(0)) < .1f);
    }
    /* bass -24 dB: 40 Hz drops, 2 kHz untouched */
    reset(); ctl[BF_BASS] = -24; tone(40, 1); go(); float lo = db(0);
    reset(); ctl[BF_BASS] = -24; tone(2000, 1); go(); float hi = db(0);
    printf("bass -24: 40 Hz %+.1f dB, 2 kHz %+.2f dB\n", lo, hi);
    assert(lo < -18 && fabsf(hi) < .2f);
    /* mono: an out-of-phase 50 Hz cancels; out-of-phase 3 kHz keeps its width */
    reset(); tone(50, -1); go(); float side_lo = db(0);
    reset(); tone(3000, -1); go(); float side_hi = db(0);
    printf("mono on, L=-R: 50 Hz %+.1f dB, 3 kHz %+.2f dB\n", side_lo, side_hi);
    assert(side_lo < -20 && fabsf(side_hi) < .2f);
    /* mono off: the out-of-phase 50 Hz stays */
    reset(); ctl[BF_MONO] = 0; tone(50, -1); go();
    printf("mono off, L=-R: 50 Hz %+.2f dB\n", db(0));
    assert(fabsf(db(0)) < .2f);
    /* sweeping the crossover mid-signal stays bounded */
    reset(); tone(100, 1);
    for (int i = 0; i < N; i += 256) {
        ctl[BF_FREQ] = 30 + 470*(i/(float)N);
        for (int c = 0; c < 2; ++c) { d->connect_port(h, c, in[c]+i); d->connect_port(h, 2+c, out[c]+i); }
        d->run(h, 256);
    }
    for (int i = 0; i < N; ++i) assert(isfinite(out[0][i]) && fabsf(out[0][i]) < .5f);
    puts("check-bass-focus: OK");
    return 0;
}
