/* HS Crush behaviour check: curves, timing, audio HP, distortion, mix/bypass, meter.
 * cc -O2 -I. check-distressor.c hs_distressor.c hs_env.c -lm -o /tmp/check-distressor && /tmp/check-distressor */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "ladspa.h"
#include "hs_distressor.h"
#include "hs_env.h"
extern const LADSPA_Descriptor hs_distressor_descriptor;

enum { RATE = 48000, N = 6*RATE };
static float in[2][N], out[2][N], ctl[DS_COUNT];
static const LADSPA_Descriptor *d = &hs_distressor_descriptor;
static LADSPA_Handle h;

static void reset(void) { const float v[DS_COUNT] = {5, 5, 5, 7.5f, 4, 0, 0, 0, 100, 0}; memcpy(ctl, v, sizeof v); d->activate(h); }
static void tone(float amp, float hz, int from, int to) { for (int i = from; i < to; ++i) in[0][i] = in[1][i] = amp*sinf(6.2831853f*hz*i/RATE); }
static void go(void) { for (int i = 0; i < N; i += 256) {
    for (int c = 0; c < 2; ++c) { d->connect_port(h, c, in[c]+i); d->connect_port(h, 2+c, out[c]+i); }
    d->run(h, 256); } }
static float out_db(int from, int to) { double a = 0; for (int i = from; i < to; ++i) { assert(isfinite(out[0][i])); a += out[0][i]*out[0][i]; } return 10*log10(a/(to-from)); }
static float gain_db(int from, int to) { double b = 0; for (int i = from; i < to; ++i) b += in[0][i]*in[0][i]; return out_db(from, to) - 10*log10(b/(to-from)); }
/* what SampQOL loads on a fresh insert: the hint default, raw (Ratio arrives as 3.5) */
static float hint_default(int i)
{
    const LADSPA_PortRangeHint *r = &d->PortRangeHints[4+i];
    int k = r->HintDescriptor & LADSPA_HINT_DEFAULT_MASK, lg = LADSPA_IS_HINT_LOGARITHMIC(r->HintDescriptor) && r->LowerBound > 0;
    float lo = r->LowerBound, hi = r->UpperBound;
    if (k == LADSPA_HINT_DEFAULT_MINIMUM) return lo;
    if (k == LADSPA_HINT_DEFAULT_MAXIMUM) return hi;
    if (k == LADSPA_HINT_DEFAULT_0) return 0;
    assert(k == LADSPA_HINT_DEFAULT_LOW || k == LADSPA_HINT_DEFAULT_MIDDLE || k == LADSPA_HINT_DEFAULT_HIGH);  /* DEFAULT_1 reads as the max on the MPC */
    float t = k == LADSPA_HINT_DEFAULT_LOW ? .25f : k == LADSPA_HINT_DEFAULT_HIGH ? .75f : .5f;
    return lg ? expf(logf(lo)*(1-t) + logf(hi)*t) : lo*(1-t) + hi*t;
}
static float thd(float hz)
{
    double re = 0, im = 0, all = 0; int n = N/2;
    for (int i = N/2; i < N; ++i) { double p = 6.283185307*hz*i/RATE; re += out[0][i]*cos(p); im += out[0][i]*sin(p); all += out[0][i]*out[0][i]; }
    double f = 2*(re*re + im*im)/((double)n*n), t = all/n;
    return 100*sqrt(fmax(0, t - f)/f);
}

int main(void)
{
    h = d->instantiate(d, RATE);
    for (int i = 0; i < DS_COUNT; ++i) d->connect_port(h, 4+i, &ctl[i]);
    static const char *const rn[] = {"1:1","2:1","3:1","4:1","6:1","10 Opto","20:1","Max"};
    printf("ratio     out@-40   slope(-6..0 in, input 10)\n");
    for (int r = 0; r < 8; ++r) {
        float o[3]; const float lv[3] = {-40, -6, 0};
        for (int k = 0; k < 3; ++k) {
            reset(); ctl[DS_RATIO] = r; ctl[DS_ATTACK] = 0; ctl[DS_RELEASE] = 6; ctl[DS_INPUT] = k ? 10 : 5;
            tone(powf(10, lv[k]/20), 1000, 0, N); go(); o[k] = out_db(N/2, N);
        }
        float slope = 6/fmaxf(1e-3f, o[2]-o[1]);
        printf("%-8s %7.2f  %7.2f:1\n", rn[r], o[0], slope);
        if (r == 0) assert(fabsf(slope - 1) < .1f);
        if (r >= 3 && r != 5) assert(slope > (r >= 6 ? 8 : .6f*(r == 3 ? 4 : 6)));
    }
    /* load defaults, at the values SampQOL loads from the hints (Output 7.5, Ratio 3.5): a -12 dBFS
       signal stays within 1 dB of the dry level */
    for (int i = 0; i < DS_COUNT; ++i) ctl[i] = hint_default(i);
    assert(ctl[DS_OUTPUT] == 7.5f);
    d->activate(h); tone(.25f, 1000, 0, N); go();
    float load = gain_db(N/2, N);
    printf("load defaults, -12 dBFS: %+.2f dB\n", load);
    assert(fabsf(load) <= 1);

    /* British mode: louder input comes out quieter once well past threshold */
    float b[2];
    for (int k = 0; k < 2; ++k) { reset(); ctl[DS_BRIT] = 1; ctl[DS_INPUT] = 7 + 3*k; tone(.5f, 1000, 0, N); go(); b[k] = out_db(N/2, N); }
    printf("british: input 7 -> %.1f dB, input 10 -> %.1f dB\n", b[0], b[1]);
    assert(b[1] < b[0]);

    /* attack: 63% time of a step for knob 0, 5, 10 */
    for (int k = 0; k <= 10; k += 5) {
        reset(); ctl[DS_ATTACK] = k; ctl[DS_INPUT] = 10; memset(in, 0, sizeof in);
        for (int i = RATE/10; i < N; ++i) in[0][i] = in[1][i] = .5f;
        go(); float fin = out[0][N-1], pk = out[0][RATE/10], t = 0;
        for (int i = RATE/10; i < N; ++i) if (out[0][i] - fin < .37f*(pk - fin)) { t = 1000.f*(i-RATE/10)/RATE; break; }
        printf("attack %2d: %.2f ms\n", k, t);
    }
    /* release: time for gain reduction to fall to 37% after the signal drops away */
    for (int k = 0; k <= 10; k += 5) {
        reset(); ctl[DS_RELEASE] = k; ctl[DS_INPUT] = 10; ctl[DS_RATIO] = 4;
        tone(.5f, 1000, 0, N/4); memset(in[0]+N/4, 0, sizeof(float)*(N-N/4)); memset(in[1]+N/4, 0, sizeof(float)*(N-N/4));
        float gr0 = 0; int t = 0;
        for (int i = 0; i < N; i += 64) {
            for (int c = 0; c < 2; ++c) { d->connect_port(h, c, in[c]+i); d->connect_port(h, 2+c, out[c]+i); }
            d->run(h, 64);
            if (i + 64 == N/4) gr0 = hs_distressor_gr(h);
            if (i >= N/4 && !t && hs_distressor_gr(h) < .37f*gr0) t = i + 64 - N/4;
        }
        printf("release %2d: %.1f dB GR, 63%% recovered in %.0f ms\n", k, gr0, 1000.f*t/RATE);
        assert(t > 0);
    }

    /* audio HP: Bessel, -3 dB at 65 Hz, about -12 dB at 30 Hz */
    float hp[3]; const float hz[3] = {1000, 65, 30};
    for (int k = 0; k < 3; ++k) { reset(); ctl[DS_RATIO] = 0; ctl[DS_AUDIO] = 1; tone(.01f, hz[k], 0, N); go(); hp[k] = gain_db(N/2, N); }
    printf("audio HP: 65 Hz %.1f dB, 30 Hz %.1f dB (re 1 kHz)\n", hp[1]-hp[0], hp[2]-hp[0]);
    assert(fabsf(hp[1]-hp[0] + 3) < 1 && hp[2]-hp[0] < -9 && hp[2]-hp[0] > -16);

    /* distortion at 0 dBFS out, 1:1 */
    static const char *const an[] = {"Clean","","Dist 2","Dist 3"};
    float t[4];
    for (int a = 0; a <= 3; a += (a ? 1 : 2)) {
        reset(); ctl[DS_RATIO] = 0; ctl[DS_AUDIO] = a; ctl[DS_INPUT] = 5; ctl[DS_OUTPUT] = 8 + 2.5f*0;
        tone(1, 220, 0, N); go(); t[a] = thd(220);
        printf("%s THD at 0 dBFS: %.2f%%\n", an[a], t[a]);
    }
    assert(t[0] < .5f && t[2] > 1 && t[3] > t[2]);

    /* meter reads the instance it is given */
    reset(); ctl[DS_INPUT] = 10; tone(.5f, 1000, 0, N); go();
    printf("meter: %.1f dB GR\n", hs_distressor_gr(h));
    assert(hs_distressor_gr(h) > 3 && hs_distressor_gr(NULL) == 0);

    /* mix 0 and bypass pass the input untouched; full-scale noise stays finite */
    for (int m = 0; m < 2; ++m) {
        reset(); ctl[DS_INPUT] = 10; ctl[DS_AUDIO] = 5; if (m) ctl[DS_BYPASS] = 1; else ctl[DS_MIX] = 0;
        tone(.5f, 1000, 0, N); go();
        for (int i = 0; i < N; ++i) assert(out[0][i] == in[0][i]);
    }
    /* Key In from bus D ducks a quiet signal; with no key it would pass untouched at this level. */
    reset(); ctl[DS_RATIO] = 6; ctl[DS_KEY] = 4; tone(.01f, 1000, 0, N); hs_env_send(4, 1); go();
    float keyed = gain_db(N/2, N);
    reset(); ctl[DS_RATIO] = 6; tone(.01f, 1000, 0, N); go();
    float own = gain_db(N/2, N);
    printf("keyed from D: %.2f dB vs own key %.2f dB\n", keyed, own);
    assert(keyed < own - 8);
    puts("check-distressor: OK");
    return 0;
}
