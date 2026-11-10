/* Time one editor redraw's worth of curve math on the device: bench-curve /path/hs-pro-fx-ladspa.so */
#include <dlfcn.h>
#include <stdio.h>
#include <time.h>
#include "ladspa.h"
#include "hs_eq2.h"
int main(int argc, char **argv)
{
    void *lib = dlopen(argv[1], RTLD_NOW);
    if (!lib) { puts(dlerror()); return 1; }
    const LADSPA_Descriptor *(*get)(unsigned long) = dlsym(lib, "ladspa_descriptor");
    int (*resp)(int, int, float *) = dlsym(lib, "hs_eq2_response");
    const LADSPA_Descriptor *d = get(HS_EQ2_ID);
    static float buf[4][128], ctl[EQ_COUNT], db[302];
    void *h = d->instantiate(d, 48000);
    for (int c = 0; c < 4; ++c) d->connect_port(h, c, buf[c]);
    for (int i = 0; i < EQ_COUNT; ++i) d->connect_port(h, 4 + i, &ctl[i]);
    for (int b = 0; b < EQ_BANDS; ++b) {
        float *v = ctl + EQ_FIRST + b * EB_COUNT;
        v[EB_FREQ] = 40 * (b + 1); v[EB_GAIN] = b % 2 ? 4 : -3; v[EB_Q] = 1; v[EB_TYPE] = EQ_BELL;
        v[EB_ON] = b < 6; v[EB_STEREO] = 2;
    }
    d->activate(h); d->run(h, 128);
    struct timespec a, z;
    clock_gettime(CLOCK_MONOTONIC, &a);
    for (int n = 0; n < 20; ++n) { resp(0, 302, db); resp(1, 302, db); resp(3, 302, db); }
    clock_gettime(CLOCK_MONOTONIC, &z);
    printf("one redraw (3 curves x 302 points): %.1f ms\n", ((z.tv_sec - a.tv_sec) * 1e3 + (z.tv_nsec - a.tv_nsec) / 1e6) / 20);
}
