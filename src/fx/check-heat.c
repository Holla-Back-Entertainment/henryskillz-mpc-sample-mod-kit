/* HS Heat behaviour check.
 * cc -O2 -I. check-heat.c hs_heat.c -lm -o /tmp/check-heat && /tmp/check-heat */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "ladspa.h"
#include "hs_heat.h"
extern const LADSPA_Descriptor hs_heat_descriptor;

enum { RATE = 48000, N = RATE };
static float in[2][N], out[2][N], ctl[HE_COUNT];
static const LADSPA_Descriptor *d = &hs_heat_descriptor;
static LADSPA_Handle h;

static void reset(void)
{
    const float v[HE_COUNT] = {1, 40, 0, 0, 100, 0, 0, 0, 0, 0, 20, 60, 0, 64, 0, 0, 100, 0};
    memcpy(ctl, v, sizeof v);
    d->activate(h);
}
static void tone(float amp, float hz) { for (int i = 0; i < N; ++i) in[0][i] = in[1][i] = amp*sinf(6.2831853f*hz*i/RATE); }
static void go(void) { for (int i = 0; i < N; i += 128) {
    for (int c = 0; c < 2; ++c) { d->connect_port(h, c, in[c]+i); d->connect_port(h, 2+c, out[c]+i); }
    d->run(h, 128); } }
static float level_db(int from) { double a = 0, b = 0;
    for (int i = from; i < N; ++i) { a += out[0][i]*out[0][i]; b += in[0][i]*in[0][i]; } return 10*log10(a/b); }
static float thd(float hz)
{
    double re = 0, im = 0, all = 0; int n = N/2;
    for (int i = N/2; i < N; ++i) { double p = 6.283185307*hz*i/RATE; re += out[0][i]*cos(p); im += out[0][i]*sin(p); all += out[0][i]*out[0][i]; }
    double f = 2*(re*re + im*im)/((double)n*n), t = all/n;
    return 100*sqrt(fmax(0, t - f)/f);
}
static float peak(void) { float p = 0; for (int i = 0; i < N; ++i) { assert(isfinite(out[0][i])); p = fmaxf(p, fabsf(out[0][i])); } return p; }

int main(void)
{
    h = d->instantiate(d, RATE);
    for (int i = 0; i < HE_COUNT; ++i) d->connect_port(h, 4+i, &ctl[i]);
    static const char *const name[] = {"Clean Boost","Saturation","Enhancement","Mid Drive",
                                       "Rough Crunch","Classic Dist","Round Fuzz","High Gain"};
    printf("circuit         THD d0   THD d64  THD d127  level d127  peak\n");
    for (int c = 0; c < 8; ++c) {
        float t[3]; const float drives[3] = {0, 64, 127};
        for (int k = 0; k < 3; ++k) {
            reset(); ctl[HE_CIRCUIT] = c; ctl[HE_DRIVE] = drives[k]; tone(.25f, 220); go(); t[k] = thd(220);
        }
        float lv = level_db(N/2), pk = peak();
        printf("%-14s %7.2f%% %7.2f%% %8.2f%%  %7.1f dB  %.2f\n", name[c], t[0], t[1], t[2], lv, pk);
        assert(t[2] > t[0]);
        assert(pk < 4);
    }
    reset(); ctl[HE_CIRCUIT] = 0; ctl[HE_DRIVE] = 0; tone(.25f, 1000); go();
    assert(fabsf(level_db(N/2)) < .5f && thd(1000) < .5f);

    /* LP2 at freq 40 (~ 175 Hz) cuts 5 kHz; HP2 at the same setting passes it */
    reset(); ctl[HE_CIRCUIT] = 0; ctl[HE_DRIVE] = 0; ctl[HE_FTYPE] = 1; ctl[HE_FREQ] = 40; tone(.1f, 5000); go();
    float lp = level_db(N/2);
    reset(); ctl[HE_CIRCUIT] = 0; ctl[HE_DRIVE] = 0; ctl[HE_FTYPE] = 5; ctl[HE_FREQ] = 40; tone(.1f, 5000); go();
    float hp = level_db(N/2);
    printf("5 kHz through LP2 / HP2 at freq 40: %.1f / %.1f dB\n", lp, hp);
    assert(lp < -40 && fabsf(hp) < 1);

    /* full resonance rings but stays bounded, every filter type */
    for (int t = 1; t <= 7; ++t) {
        reset(); ctl[HE_FTYPE] = t; ctl[HE_RESO] = 127; ctl[HE_DIRT] = 127; ctl[HE_FREQ] = 70; tone(.5f, 110); go();
        float p = peak();
        printf("type %d full reso + dirt: peak %.2f\n", t, p);
        assert(p < 8);
    }

    /* mix 0 is the dry signal untouched */
    reset(); ctl[HE_MIX] = 0; ctl[HE_CIRCUIT] = 7; ctl[HE_DRIVE] = 127; tone(.5f, 300); go();
    for (int i = 0; i < N; ++i) assert(out[0][i] == in[0][i]);

    /* cost: worst case, High Gain + filter + modulation */
    reset(); ctl[HE_CIRCUIT] = 7; ctl[HE_FTYPE] = 1; ctl[HE_ENV_DEPTH] = 100; ctl[HE_LFO_DEPTH] = 100; tone(.5f, 100);
    clock_t t0 = clock(); for (int k = 0; k < 20; ++k) go();
    printf("speed: %.0fx realtime (Mac)\n", 20.0/((double)(clock()-t0)/CLOCKS_PER_SEC));
    puts("check-heat: OK");
    return 0;
}
