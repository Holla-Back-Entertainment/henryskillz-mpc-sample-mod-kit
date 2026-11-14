/* check-mod-knob: the drawn knob of a MOD target follows what the audio hears (hs_mod.h).
 *  1. hs_mod_knob_step puts values on the faceplate knob's 0-73 steps as the host does: linear, log
 *     for log hints, integers rounded; out of range and NaN stay on the knob.
 *  2. hs_mod_knob reports nothing while MOD rests, and the step difference set -> heard while it runs.
 *  3. Every MOD effect in hs-pro-fx-ladspa.so exports hs_<name>_mod_knob: a Pump sends bus A, the
 *     effect's level follows it down by depth x range, and the knob delta is that many steps; after the
 *     release it is back to 0, and with Source Off there is no target.
 * In the hs-flavor-tools container, after sh build.sh:
 *   gcc -O2 -Wall -Wextra -Werror -I. check-mod-knob.c -ldl -lm -o /tmp/check-mod-knob && /tmp/check-mod-knob hs-pro-fx-ladspa.so */
#include <assert.h>
#include <dlfcn.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ladspa.h"
#include "hs_pump.h"
#include "hs_distressor.h"
#include "hs_tube_strip.h"
#include "hs_heat.h"
#include "hs_channel.h"
#include "hs_xsat.h"
#include "hs_bus.h"

enum { RATE = 48000, BLOCK = 128, MAXP = 64 };

static const struct { unsigned long uid; int mod, level; const char *name, *knob; } fx[] = {
    {HS_PUMP_ID, PU_MOD, PU_OUTPUT, "Pump", "hs_pump_mod_knob"},
    {HS_DISTRESSOR_ID, DS_MOD, DS_OUTPUT, "Crush", "hs_distressor_mod_knob"},
    {HS_TUBE_STRIP_ID, TS_MOD, TS_OUTPUT, "Tube Strip", "hs_tube_strip_mod_knob"},
    {HS_HEAT_ID, HE_MOD, HE_VOLUME, "Heat", "hs_heat_mod_knob"},
    {HS_CHANNEL_ID, CH_MOD, CH_OUTPUT, "Channel Strip", "hs_channel_mod_knob"},
    {HS_XSAT_ID, XS_MOD, XS_OUTPUT, "Drive Sat", "hs_xsat_mod_knob"},
    {HS_BUS_ID, BUS_MOD, BUS_MAKEUP, "Bus Compressor", "hs_bus_mod_knob"},
};

struct plug { const LADSPA_Descriptor *d; LADSPA_Handle h; float ctl[MAXP]; float in[2][BLOCK], out[2][BLOCK]; };

static const LADSPA_Descriptor *find(void *lib, unsigned long uid)
{
    const LADSPA_Descriptor *(*get)(unsigned long) = (const LADSPA_Descriptor *(*)(unsigned long))dlsym(lib, "ladspa_descriptor");
    assert(get);
    for (unsigned long i = 0; i < 64; ++i) { const LADSPA_Descriptor *d = get(i); if (!d) break; if (d->UniqueID == uid) return d; }
    return NULL;
}

static float hint_default(const LADSPA_PortRangeHint *h)
{
    int k = h->HintDescriptor & LADSPA_HINT_DEFAULT_MASK, lg = LADSPA_IS_HINT_LOGARITHMIC(h->HintDescriptor) && h->LowerBound > 0;
    float lo = h->LowerBound, hi = h->UpperBound;
    float t = k == LADSPA_HINT_DEFAULT_MINIMUM ? 0 : k == LADSPA_HINT_DEFAULT_LOW ? .25f : k == LADSPA_HINT_DEFAULT_MIDDLE ? .5f
            : k == LADSPA_HINT_DEFAULT_HIGH ? .75f : k == LADSPA_HINT_DEFAULT_MAXIMUM ? 1 : -1;
    if (t >= 0) return lg ? expf(logf(lo) + t*(logf(hi) - logf(lo))) : lo + t*(hi - lo);
    return k == LADSPA_HINT_DEFAULT_1 ? 1 : k == LADSPA_HINT_DEFAULT_100 ? 100 : k == LADSPA_HINT_DEFAULT_440 ? 440 : 0;
}

static void open_plug(struct plug *p, const LADSPA_Descriptor *d)
{
    memset(p, 0, sizeof *p);
    p->d = d;
    p->h = d->instantiate(d, RATE);
    assert(p->h && d->PortCount <= MAXP);
    for (unsigned long i = 0; i < d->PortCount; ++i) {
        if (i < 4) d->connect_port(p->h, i, i < 2 ? p->in[i] : p->out[i - 2]);
        else { p->ctl[i] = hint_default(&d->PortRangeHints[i]); d->connect_port(p->h, i, &p->ctl[i]); }
    }
    d->activate(p->h);
}

static void pure_checks(void)
{
    const LADSPA_PortRangeHint lin = {LADSPA_HINT_BOUNDED_BELOW|LADSPA_HINT_BOUNDED_ABOVE, -24, 24};
    const LADSPA_PortRangeHint lg = {LADSPA_HINT_BOUNDED_BELOW|LADSPA_HINT_BOUNDED_ABOVE|LADSPA_HINT_LOGARITHMIC, 20, 20000};
    const LADSPA_PortRangeHint in = {LADSPA_HINT_BOUNDED_BELOW|LADSPA_HINT_BOUNDED_ABOVE|LADSPA_HINT_INTEGER, 0, 4};
    const LADSPA_PortRangeHint bad = {LADSPA_HINT_BOUNDED_BELOW|LADSPA_HINT_BOUNDED_ABOVE, 1, 1};
    assert(hs_mod_knob_step(&lin, -24) == 0 && hs_mod_knob_step(&lin, 24) == HS_MOD_KNOB);
    assert(hs_mod_knob_step(&lin, 0) == 37);                     /* 36.5 rounds up */
    assert(hs_mod_knob_step(&lin, -99) == 0 && hs_mod_knob_step(&lin, 99) == HS_MOD_KNOB);
    assert(hs_mod_knob_step(&lin, NAN) == 0);
    assert(hs_mod_knob_step(&lg, 20) == 0 && hs_mod_knob_step(&lg, 20000) == HS_MOD_KNOB);
    assert(hs_mod_knob_step(&lg, sqrtf(20*20000.0f)) == 37);    /* the geometric middle is the knob's middle */
    assert(hs_mod_knob_step(&lg, 2000) == 49);                   /* 2/3 of the way on a log knob, not 7 % */
    assert(hs_mod_knob_step(&in, 2.4f) == 37 && hs_mod_knob_step(&in, 2.6f) == 55);   /* integers round first */
    assert(hs_mod_knob_step(&bad, 1) == 0);

    LADSPA_PortRangeHint hints[2] = {lin, lg};
    struct hs_mod m = {0};
    int delta = 7;
    assert(hs_mod_knob(&m, hints, &delta) == -1 && delta == 0);  /* calloc'd instance: no target */
    hs_mod_publish(&m, 1, 200, 2000);
    assert(hs_mod_knob(&m, hints, &delta) == 1 && delta == hs_mod_knob_step(&lg, 2000) - hs_mod_knob_step(&lg, 200));
    hs_mod_publish(&m, 0, 0, -12);
    assert(hs_mod_knob(&m, hints, &delta) == 0 && delta == 18 - 37);
    hs_mod_publish(&m, -1, 0, 0);
    assert(hs_mod_knob(&m, hints, &delta) == -1 && delta == 0);
    printf("pure: knob steps (linear, log, integer, clamps) and set -> heard deltas\n");
}

int main(int argc, char **argv)
{
    if (argc != 2) { fprintf(stderr, "usage: %s hs-pro-fx-ladspa.so\n", argv[0]); return 2; }
    setvbuf(stdout, NULL, _IONBF, 0);
    pure_checks();
    void *lib = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!lib) { fprintf(stderr, "%s\n", dlerror()); return 2; }

    static struct plug sender, fol;
    const LADSPA_Descriptor *pump = find(lib, HS_PUMP_ID);
    for (unsigned f = 0; f < sizeof fx / sizeof *fx; ++f) {
        const LADSPA_Descriptor *nd = find(lib, fx[f].uid);
        int (*knob)(const void *, int *) = (int (*)(const void *, int *))dlsym(lib, fx[f].knob);
        assert(nd && knob);
        open_plug(&sender, pump);
        sender.ctl[4 + PU_SEND] = 1;
        open_plug(&fol, nd);
        int m = 4 + fx[f].mod, delta = 99;
        const LADSPA_PortRangeHint *lh = &nd->PortRangeHints[4 + fx[f].level];
        float range = lh->UpperBound - lh->LowerBound, base = fol.ctl[4 + fx[f].level];
        float depth = -fminf(40, 100*(base - lh->LowerBound)/range);
        fol.ctl[m + HS_MOD_TARGET] = fx[f].level;
        fol.ctl[m + HS_MOD_DEPTH] = depth;
        fol.ctl[m + HS_MOD_ATTACK] = 5;
        fol.ctl[m + HS_MOD_RELEASE] = 50;
        int off_target = 0, loud_target = -1, loud_delta = 0, rest_target = -1, rest_delta = 99;
        for (int blk = 0; blk < 800; ++blk) {
            if (blk == 100) fol.ctl[m + HS_MOD_SRC] = 1;          /* Source Off for the first 100 blocks */
            int loud = blk < 400;                                 /* sender plays 0 dBFS, then silence */
            for (int c = 0; c < 2; ++c) for (int i = 0; i < BLOCK; ++i) {
                sender.in[c][i] = loud ? sinf(6.2831853f*1000*(blk*BLOCK + i)/RATE) : 0;
                fol.in[c][i] = .1f*sinf(6.2831853f*220*(blk*BLOCK + i)/RATE);
            }
            pump->run(sender.h, BLOCK);
            nd->run(fol.h, BLOCK);
            if (blk == 99) off_target = knob(fol.h, &delta);
            if (blk == 399) loud_target = knob(fol.h, &loud_delta);
            if (blk == 799) rest_target = knob(fol.h, &rest_delta);
        }
        int want = (int)lroundf(depth*.01f*HS_MOD_KNOB);
        printf("%-15s off: target %d; on: target %d (level %d), knob %+d steps (want %+d); released: %+d steps\n",
               fx[f].name, off_target, loud_target, fx[f].level, loud_delta, want, rest_delta);
        assert(off_target == -1);
        assert(loud_target == fx[f].level);
        assert(loud_delta < 0 && abs(loud_delta - want) <= 1);
        assert(rest_target == fx[f].level && rest_delta == 0);    /* still routed, envelope back to rest */
        pump->cleanup(sender.h); nd->cleanup(fol.h);
    }
    assert(((int (*)(const void *, int *))dlsym(lib, "hs_pump_mod_knob"))(NULL, &(int){0}) == -1);
    printf("check-mod-knob: all passed\n");
    return 0;
}
