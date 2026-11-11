/* HS Pump behaviour check: static curve, timing, AtMod, Sat, mix, bypass.
 * cc -O2 -I. check-pump.c hs_pump.c hs_env.c -lm -o /tmp/check-pump && /tmp/check-pump */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "ladspa.h"
#include "hs_pump.h"
#include "hs_env.h"
#include <unistd.h>
extern const LADSPA_Descriptor hs_pump_descriptor;

enum { RATE = 48000, N = 3*RATE };
static float in[2][N], out[2][N], ctl[PU_COUNT];
static const LADSPA_Descriptor *d = &hs_pump_descriptor;
static LADSPA_Handle h;

static void reset(void)
{
    const float v[PU_COUNT] = {0, 3, 3, 3, 0, 0, 0, 0, 100, 0};
    memcpy(ctl, v, sizeof v);
    d->activate(h);
}
static void tone(float amp, float hz, int from, int to)
{
    for (int i = from; i < to; ++i) in[0][i] = in[1][i] = amp*sinf(6.2831853f*hz*i/RATE);
}
static void go(void) { for (int i = 0; i < N; i += 256) {
    for (int c = 0; c < 2; ++c) { d->connect_port(h, c, in[c]+i); d->connect_port(h, 2+c, out[c]+i); }
    d->run(h, N - i < 256 ? N - i : 256); } }
static float rms_db(int c, int from, int to)
{
    double a = 0, b = 0;
    for (int i = from; i < to; ++i) { a += out[c][i]*out[c][i]; b += in[c][i]*in[c][i]; }
    return 10*log10(a/b);
}

int main(void)
{
    h = d->instantiate(d, RATE);
    for (int i = 0; i < PU_COUNT; ++i) d->connect_port(h, 4+i, &ctl[i]);

    /* Static curves: gain at -30 (below knee), and slope well above threshold. */
    printf("ratio  gain@-30  slope(-6..0 dBFS peak)\n");
    for (int r = 0; r < 9; ++r) {
        float g[3]; const float lv[3] = {-42, -6, 0};
        for (int k = 0; k < 3; ++k) {
            reset(); ctl[PU_RATIO] = r; ctl[PU_ATTACK] = 0; ctl[PU_RELEASE] = 6; ctl[PU_INPUT] = k ? 12 : 0;
            tone(powf(10, lv[k]/20), 1000, 0, N); go(); g[k] = rms_db(0, N/2, N);
        }
        float slope = 6/((g[2]+0)-(g[1]-6)), expect[9] = {1,2,3,4,6,8,10,12,20};
        printf("%d  %6.2f  %5.2f:1\n", r, g[0], slope);
        assert(fabsf(g[0]) < .05f);
        assert(fabsf(slope-expect[r])/expect[r] < .2f);
    }

    /* Attack: a step to 0 dBFS at 4:1; time to reach 63% of the final reduction. */
    for (int k = 0; k < 8; k += 3) {
        reset(); ctl[PU_ATTACK] = k; memset(in, 0, sizeof in);
        for (int i = RATE/10; i < N; ++i) in[0][i] = in[1][i] = 1;
        go();
        float fin = out[0][N-1], t = 0;
        for (int i = RATE/10; i < N; ++i) if (out[0][i] - fin < .37f*(1 - fin)) { t = 1000.f*(i-RATE/10)/RATE; break; }
        printf("attack knob %2d: %.2f ms to 63%%\n", k, t);
    }

    /* AtMod keeps more of the first 2 ms of a step. */
    float first[2];
    for (int a = 0; a < 2; ++a) {
        reset(); ctl[PU_ATTACK] = 3; ctl[PU_ATMOD] = 10*a; memset(in, 0, sizeof in);
        for (int i = RATE/10; i < N; ++i) in[0][i] = in[1][i] = 1;
        go(); first[a] = out[0][RATE/10 + RATE/500];
    }
    printf("level 2 ms into a hit: AtMod off %.3f, on %.3f\n", first[0], first[1]);
    assert(first[1] > first[0]);

    /* Release: time to recover 63% after the signal drops 0 -> -40 dBFS. Opto should be slower at the tail. */
    for (int o = 0; o < 2; ++o) {
        reset(); ctl[PU_RELEASE] = o ? 7 : 4;
        tone(1, 1000, 0, N/2); tone(.01f, 1000, N/2, N); go();
        float g0 = rms_db(0, N/2-480, N/2), g;
        int i = N/2;
        int t63 = 0, t90 = 0;
        for (; i < N-480; i += 480) { g = rms_db(0, i, i+480);
            if (!t63 && g > .37f*g0) t63 = i; if (!t90 && g > .1f*g0) t90 = i; }
        printf("release (%s): gr %.1f dB, 63%% back in %.0f ms, 90%% in %.0f ms\n", o ? "opto" : ".25 s", -g0,
               1000.f*(t63-N/2)/RATE, t90 ? 1000.f*(t90-N/2)/RATE : -1.f);
    }

    /* Sat: a +6 dB sine stays under full scale; below -6 dBFS it does nothing. */
    reset(); ctl[PU_RATIO] = 0; ctl[PU_SAT] = 1; tone(1, 100, 0, N); go();
    float peak = 0; for (int i = N/2; i < N; ++i) peak = fmaxf(peak, fabsf(out[0][i]));
    printf("sat peak, 0 dBFS 100 Hz in at 1:1: %.3f\n", peak);
    assert(peak < 1.0f);
    reset(); ctl[PU_RATIO] = 0; ctl[PU_SAT] = 1; tone(.01f, 1000, 0, N); go();
    printf("sat at -40 dBFS 1 kHz: %.3f dB\n", rms_db(0, N/2, N));
    assert(fabsf(rms_db(0, N/2, N)) < .1f);

    /* Mix 0 and bypass both pass the input untouched. */
    for (int m = 0; m < 2; ++m) {
        reset(); ctl[PU_INPUT] = 20; if (m) ctl[PU_BYPASS] = 1; else ctl[PU_MIX] = 0;
        tone(.5f, 1000, 0, N); go();
        for (int i = 0; i < N; ++i) assert(out[0][i] == in[0][i]);
    }
    /* Key bus: Send publishes the input peak; Key In ducks from bus A and ignores its own signal. */
    reset(); ctl[PU_RATIO] = 0; ctl[PU_SEND] = 2; tone(.5f, 1000, 0, N); go();
    printf("send B: %.3f\n", hs_env_read(2));
    assert(fabsf(hs_env_read(2) - .5f) < .01f && hs_env_read(1) == 0);
    reset(); ctl[PU_RATIO] = 8; ctl[PU_KEY] = 1; tone(.03f, 1000, 0, N);
    hs_env_send(1, 1); go();
    printf("keyed from A (0 dBFS key, -30 dBFS in, 20:1): %.2f dB\n", rms_db(0, N/2, N));
    assert(rms_db(0, N/2, N) < -8);
    reset(); ctl[PU_RATIO] = 8; ctl[PU_KEY] = 3; tone(.9f, 1000, 0, N); go();   /* nothing on C: no ducking */
    printf("keyed from silent C: %.2f dB\n", rms_db(0, N/2, N));
    assert(fabsf(rms_db(0, N/2, N)) < .5f);
    usleep(150000);
    assert(hs_env_read(1) == 0);   /* a stopped sender goes quiet */
    puts("check-pump: OK");
    return 0;
}
