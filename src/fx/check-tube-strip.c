/* HS Tube Strip behaviour check.
 * cc -O2 -I. check-tube-strip.c hs_tube_strip.c -lm -o /tmp/cts && /tmp/cts */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "ladspa.h"
#include "hs_tube_strip.h"
extern const LADSPA_Descriptor hs_tube_strip_descriptor;

enum { RATE = 48000, N = RATE };
static float in[2][N], out[2][N], ctl[TS_COUNT];
static const LADSPA_Descriptor *d = &hs_tube_strip_descriptor;
static LADSPA_Handle h;

/* the hint defaults, as SampQOL loads them */
static float hint_default(int i)
{
    const LADSPA_PortRangeHint *r = &d->PortRangeHints[4+i];
    int k = r->HintDescriptor & LADSPA_HINT_DEFAULT_MASK, lg = LADSPA_IS_HINT_LOGARITHMIC(r->HintDescriptor);
    float lo = r->LowerBound, hi = r->UpperBound;
    if (k == LADSPA_HINT_DEFAULT_MINIMUM) return lo;
    if (k == LADSPA_HINT_DEFAULT_MAXIMUM) return hi;
    if (k == LADSPA_HINT_DEFAULT_0) return 0;
    if (k == LADSPA_HINT_DEFAULT_1) return 1;
    float t = k == LADSPA_HINT_DEFAULT_LOW ? .25f : k == LADSPA_HINT_DEFAULT_HIGH ? .75f : .5f;
    return lg ? expf(logf(lo)*(1-t) + logf(hi)*t) : lo*(1-t) + hi*t;
}
static void reset(void) { for (int i = 0; i < TS_COUNT; ++i) ctl[i] = hint_default(i); d->activate(h); }
static void tone(float hz, float amp) { for (int i = 0; i < N; ++i) in[0][i] = in[1][i] = amp*sinf(6.2831853f*hz*i/RATE); }
static void go(void) { for (int i = 0; i < N; i += 256) {
    for (int c = 0; c < 2; ++c) { d->connect_port(h, c, in[c]+i); d->connect_port(h, 2+c, out[c]+i); }
    d->run(h, N - i < 256 ? N - i : 256); } }
static float db(void) { double a = 0, b = 0; for (int i = N/2; i < N; ++i) { a += out[0][i]*out[0][i]; b += in[0][i]*in[0][i]; } return 10*log10(a/b + 1e-30); }

int main(void)
{
    h = d->instantiate(d, RATE);
    for (int i = 0; i < TS_COUNT; ++i) d->connect_port(h, 4+i, &ctl[i]);

    /* load defaults: comp idle, EQ flat, light drive -> a -12 dBFS tone passes near unity */
    const float hz[] = {50, 1000, 8000};
    for (unsigned k = 0; k < 3; ++k) {
        reset(); tone(hz[k], .25f); go();
        printf("defaults, %5.0f Hz: %+.2f dB\n", hz[k], db());
        assert(fabsf(db()) < .5f);
    }
    reset(); tone(1000, .25f); go();
    assert(hs_tube_strip_gr(h) < .01f);
    /* compressor: threshold -30, ratio 10 pulls a -12 dBFS tone well down */
    reset(); ctl[TS_THRESHOLD] = -30; ctl[TS_RATIO] = 10; tone(1000, .25f); go();
    float gr = hs_tube_strip_gr(h);
    printf("comp -30 / 10:1: %+.1f dB, GR %.1f dB\n", db(), gr);
    assert(db() < -10 && gr > 10);
    /* EQ: bass +12 at 60 Hz lifts 40 Hz, treble -12 at 10k cuts 16 kHz, mids leave 1 kHz alone */
    reset(); ctl[TS_BASS_HZ] = 2; ctl[TS_BASS_DB] = 12; tone(40, .05f); go(); float bass = db();
    reset(); ctl[TS_TREBLE_HZ] = 0; ctl[TS_TREBLE_DB] = -12; tone(16000, .05f); go(); float treble = db();
    reset(); ctl[TS_LMID_HZ] = 3000; ctl[TS_LMID_DB] = 16; ctl[TS_Q] = 1; tone(3000, .02f); go(); float mid = db();
    printf("bass +12: %+.1f dB, treble -12: %+.1f dB, low mid +16 @3k: %+.1f dB\n", bass, treble, mid);
    assert(bass > 8 && treble < -8 && mid > 12);
    /* SC EQ: the boosted mid leaves the audio path */
    ctl[TS_SC] = 1; d->activate(h); go();
    printf("same with SC EQ: %+.2f dB\n", db());
    assert(fabsf(db()) < .5f);
    /* HPF 140 Hz at 6 dB/oct: 35 Hz down about 12 dB */
    reset(); ctl[TS_HPF] = 140; tone(35, .05f); go();
    printf("HPF 140, 35 Hz: %+.1f dB\n", db());
    assert(db() < -9 && db() > -15);
    /* drive 10 on a loud tone stays bounded, bypass is exact */
    reset(); ctl[TS_DRIVE] = 10; tone(100, .9f); go();
    for (int i = 0; i < N; ++i) assert(isfinite(out[0][i]) && fabsf(out[0][i]) < 1.5f);
    reset(); ctl[TS_BYPASS] = 1; ctl[TS_BASS_DB] = 24; tone(100, .3f); go();
    for (int i = 0; i < N; ++i) assert(out[0][i] == in[0][i]);
    puts("check-tube-strip: OK");
    return 0;
}
