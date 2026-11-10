/* hs_eq2_response() must match what run() does to audio: compare with a measured impulse response. */
#include <assert.h>
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include "ladspa.h"
#include "hs_eq2.h"
extern const LADSPA_Descriptor hs_eq2_descriptor;
int hs_eq2_response(int, int, float *);
enum { N = 1 << 15, RATE = 48000, P = 64 };
static float in[2][N], out[2][N], ctl[EQ_COUNT];
int main(void)
{
    const LADSPA_Descriptor *d = &hs_eq2_descriptor;
    void *h = d->instantiate(d, RATE);
    for (int c = 0; c < 2; ++c) { d->connect_port(h, c, in[c]); d->connect_port(h, c + 2, out[c]); }
    for (int i = 0; i < EQ_COUNT; ++i) d->connect_port(h, 4 + i, &ctl[i]);
    float band[][EB_COUNT] = {  /* freq gain q type on bypass bell family slope stereo */
        {55, 0, 1, EQ_HPF, 1, 0, 0, 4, 2, 2}, {380, -4, 2.1f, EQ_BELL, 1, 0, 0, 0, 0, 2},
        {3000, 6, 1, EQ_BELL, 1, 0, 3, 0, 0, 2}, {7600, 5, .7f, EQ_HIGH_SHELF, 1, 0, 0, 0, 0, 2}};
    for (int b = 0; b < EQ_BANDS; ++b) {
        static const float off[EB_COUNT] = {632, 0, 1, 4, 0, 0, 0, 0, 0, 2};
        for (int k = 0; k < EB_COUNT; ++k)
            ctl[EQ_FIRST + b * EB_COUNT + k] = b < 4 ? band[b][k] : off[k];
    }
    for (int parallel = 0; parallel < 2; ++parallel) {
        ctl[EQ_PARALLEL] = parallel;
        d->activate(h);
        in[0][0] = 1;
        d->run(h, N);
        float curve[P], worst = 0;
        assert(hs_eq2_response(0, P, curve));
        for (int k = 0; k < P; ++k) {
            double hz = 20 * pow(1000, (double)k / (P - 1));
            double complex s = 0;
            for (int n = 0; n < N; ++n) s += out[0][n] * cexp(-I * 2 * M_PI * hz * n / RATE);
            float diff = fabsf(curve[k] - (float)(20 * log10(cabs(s))));
            if (diff > worst) worst = diff;
        }
        printf("parallel %d: drawn curve vs audio, worst %.3f dB\n", parallel, worst);
        assert(worst < .05f);
    }
    float flat[P];
    assert(hs_eq2_response(2 + 10, P, flat));  /* an unused band draws flat */
    for (int k = 0; k < P; ++k) assert(fabsf(flat[k]) < 1e-4f);
    puts("curve check passed");
}
