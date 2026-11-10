/* Two Para EQ inserts: the editor must follow the one being edited, not the one that ran last. */
#include <assert.h>
#include <stdio.h>
#include <ladspa.h>
#include "hs_eq2.h"
extern const LADSPA_Descriptor hs_eq2_descriptor;
const float *hs_eq2_values(unsigned *seq);
enum { FIRST = 4 };
static float audio[4][64];
struct inst { LADSPA_Handle h; float c[EQ_COUNT]; };
static void make(struct inst *x, float input) {
    const LADSPA_Descriptor *d = &hs_eq2_descriptor;
    x->h = d->instantiate(d, 44100);
    for (int i = 0; i < 4; ++i) d->connect_port(x->h, i, audio[i]);
    for (int i = 0; i < EQ_COUNT; ++i) { x->c[i] = d->PortRangeHints[FIRST + i].LowerBound; d->connect_port(x->h, FIRST + i, &x->c[i]); }
    x->c[EQ_MIDSIDE] = x->c[EQ_PARALLEL] = x->c[EQ_BYPASS] = 0; x->c[EQ_OUTPUT] = 0;
    x->c[EQ_INPUT] = input;
    d->activate(x->h);
}
static float shown(void) { return hs_eq2_values(0)[EQ_INPUT]; }
int main(void) {
    const LADSPA_Descriptor *d = &hs_eq2_descriptor;
    struct inst a, b;
    make(&a, -1); d->run(a.h, 64); assert(shown() == -1);                 /* first instance owns */
    make(&b, 2);  d->run(b.h, 64); d->run(a.h, 64); assert(shown() == -1); /* adding B doesn't steal */
    d->activate(b.h); d->run(b.h, 64); assert(shown() == -1);             /* nor re-activating it */
    b.c[EQ_INPUT] = 3; d->run(b.h, 64); assert(shown() == 3);              /* editing B moves the editor */
    d->run(a.h, 64); assert(shown() == 3);                                 /* A running doesn't take it back */
    a.c[EQ_INPUT] = -2; d->run(a.h, 64); assert(shown() == -2);           /* editing A does */
    d->cleanup(a.h); d->run(b.h, 64); assert(shown() == 3);               /* owner deleted: B takes over */
    d->cleanup(b.h);
    puts("owner check passed");
}
