/* check-registry: the preset registry sees an instance's control values and leaves descriptor identity alone.
 * Built with the ladspa sources in build.sh's container:
 *   gcc -O2 -I. -DHS_MPC_LADSPA_RECALL check-registry.c <ladspa sources> hs_registry.c -lm -pthread */
#include <assert.h>
#include <ladspa.h>
#include <stdio.h>
#include "hs_pump.h"

extern const LADSPA_Descriptor hs_pump_descriptor;
const LADSPA_Descriptor *ladspa_descriptor(unsigned long index);
int hs_ladspa_controls(const void *h, const char **label, float *val, float *lo, float *hi, unsigned char *integer, int max);

int main(void)
{
    const LADSPA_Descriptor *d = NULL;
    for (unsigned long i = 0; (d = ladspa_descriptor(i)); ++i) if (d->UniqueID == hs_pump_descriptor.UniqueID) break;
    assert(d == &hs_pump_descriptor);                       /* identity kept for hs-fx */
    LADSPA_Handle h = d->instantiate(d, 48000);
    float ports[64] = {0}, io[4][64];
    int c = 0;
    for (unsigned long p = 0; p < d->PortCount; ++p) {
        if (LADSPA_IS_PORT_CONTROL(d->PortDescriptors[p])) { ports[p] = 0.5f + c++; d->connect_port(h, p, &ports[p]); }
        else d->connect_port(h, p, io[p % 4]);
    }
    const char *label; float v[64], lo[64], hi[64]; unsigned char in[64];
    int n = hs_ladspa_controls(h, &label, v, lo, hi, in, 64);
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("%s: %d controls, first %.1f last %.1f, ratio integer=%d\n", label, n, v[0], v[n - 1], in[1]);
    assert(n == c && v[0] == 0.5f && v[n - 1] == n - 0.5f);
    unsigned long first = 0; while (!LADSPA_IS_PORT_CONTROL(d->PortDescriptors[first])) ++first;
    ports[first] = 7.25f; hs_ladspa_controls(h, NULL, v, lo, hi, in, 64); assert(v[0] == 7.25f);   /* live, not a copy */
    d->cleanup(h);
    assert(hs_ladspa_controls(h, NULL, v, lo, hi, in, 64) == -1);   /* forgotten after cleanup */
    assert(hs_ladspa_controls((void *)0x1234, NULL, v, lo, hi, in, 64) == -1);
    puts("registry: OK");
    return 0;
}
