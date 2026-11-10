/* One NaN input sample must not silence an effect for good: after it, normal audio must come
 * back finite within a few blocks. Controls sit mid-range so filters and dynamics are active. */
#include <dlfcn.h>
#include <math.h>
#include <stdio.h>
#include <ladspa.h>
int main(int argc, char **argv) {
    void *lib = dlopen(argv[1], RTLD_NOW);
    LADSPA_Descriptor_Function df = (LADSPA_Descriptor_Function)dlsym(lib, "ladspa_descriptor");
    int bad = 0;
    for (unsigned long d = 0; df(d); ++d) {
        const LADSPA_Descriptor *p = df(d);
        LADSPA_Handle h = p->instantiate(p, 44100);
        static float audio[8][128], ctl[256];
        int a = 0;
        for (unsigned long i = 0; i < p->PortCount; ++i) {
            if (LADSPA_IS_PORT_AUDIO(p->PortDescriptors[i])) { p->connect_port(h, i, audio[a++]); continue; }
            LADSPA_PortRangeHint r = p->PortRangeHints[i];
            ctl[i] = (r.LowerBound + r.UpperBound) / 2;
            p->connect_port(h, i, &ctl[i]);
        }
        if (p->activate) p->activate(h);
        int finite = 1;
        for (int b = 0; b < 40; ++b) {
            for (int k = 0; k < 128; ++k) audio[0][k] = audio[1][k] = .3f * sinf((b * 128 + k) * .05f);
            if (b == 2) audio[0][5] = audio[1][5] = NAN;
            p->run(h, 128);
            if (b == 39) for (int k = 0; k < 128; ++k) for (int c = 2; c < a; ++c) finite &= isfinite(audio[c][k]);
        }
        printf("%-40s %s\n", p->Name, finite ? "recovers" : "STUCK (NaN forever)");
        bad |= !finite;
        p->cleanup(h);
    }
    return bad;
}
