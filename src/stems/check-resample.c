/* Resampler check: cc -O2 -Dmain=engine_main check-resample.c -lm -o /tmp/crs && /tmp/crs
   48 kHz and 96 kHz 1 kHz sines come out as a 1 kHz sine at 44.1 kHz, same level, same length. */
#include "hs_stems.c"
#undef main
#include <assert.h>
int main(void) {
    uint32_t rates[] = {48000, 96000, 22050};
    for (int r = 0; r < 3; r++) {
        uint32_t sr = rates[r]; long n = sr;             /* one second */
        float *in = xcalloc((size_t)n * 2, sizeof(float));
        for (long i = 0; i < n; i++) in[i * 2] = in[i * 2 + 1] = 0.5f * sinf(2 * M_PI * 1000 * i / sr);
        long m; float *o = resample(in, n, sr, &m);
        assert(labs(m - 44100) <= 1);
        double err = 0;
        for (long j = 2000; j < m - 2000; j++) {         /* skip the edges */
            double want = 0.5 * sin(2 * M_PI * 1000 * j / 44100.0);
            double e = fabs(o[j * 2] - want); if (e > err) err = e;
            assert(o[j * 2] == o[j * 2 + 1]);
        }
        printf("%u Hz -> 44100: %ld frames, max error %.2g\n", sr, m, err);
        assert(err < 2e-3);                              /* better than -48 dB of the 0.5 sine */
        free(in); free(o);
    }
    puts("check-resample: OK");
    return 0;
}
