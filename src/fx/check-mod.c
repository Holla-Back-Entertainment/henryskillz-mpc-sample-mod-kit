/* check-mod: the MOD pages (hs_mod.h) on every HS effect that has them.
 *  1. Off is bit-identical to the build before MOD: old.so and new.so run the same audio and the same
 *     settings, new.so's MOD ports at their lower bounds (an old project), at their defaults, and with a
 *     source picked whose bus is silent.
 *  2. On, the target moves: a Pump sends bus A, each effect's level parameter follows it down by
 *     depth x range, and lets go after the release: back to bit-identical with off.
 * old.so: hs-pro-fx-ladspa.so built (sh build.sh) from the commit before MOD, a31afd1; new: this build.
 * In the gcc:12 container:
 *   gcc -O2 -Wall -Wextra -I. check-mod.c -ldl -lm -o /tmp/check-mod && /tmp/check-mod old.so hs-pro-fx-ladspa.so */
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

enum { RATE = 48000, BLOCK = 128, BLOCKS = 600, MAXP = 64 };

/* uid, first MOD control, a level parameter that is plain output gain in dB (or close to it) */
static const struct { unsigned long uid; int mod, level; const char *name; } fx[] = {
    {HS_PUMP_ID, PU_MOD, PU_OUTPUT, "Pump"},
    {HS_DISTRESSOR_ID, DS_MOD, DS_OUTPUT, "Crush"},
    {HS_TUBE_STRIP_ID, TS_MOD, TS_OUTPUT, "Tube Strip"},
    {HS_HEAT_ID, HE_MOD, HE_VOLUME, "Heat"},
    {HS_CHANNEL_ID, CH_MOD, CH_OUTPUT, "Channel Strip"},
    {HS_XSAT_ID, XS_MOD, XS_OUTPUT, "Drive Sat"},
    {HS_BUS_ID, BUS_MOD, BUS_MAKEUP, "Bus Compressor"},
};

struct plug { const LADSPA_Descriptor *d; LADSPA_Handle h; float ctl[MAXP]; float in[2][BLOCK], out[2][BLOCK]; };

static const LADSPA_Descriptor *find(void *lib, unsigned long uid)
{
    const LADSPA_Descriptor *(*get)(unsigned long) = (const LADSPA_Descriptor *(*)(unsigned long))dlsym(lib, "ladspa_descriptor");
    assert(get);
    for (unsigned long i = 0; i < 64; ++i) { const LADSPA_Descriptor *d = get(i); if (!d) break; if (d->UniqueID == uid) return d; }
    return NULL;
}

/* what SampQOL loads for a fresh insert: the hint default */
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

static unsigned rng = 1;
static float noise(void) { rng = rng*1664525u + 1013904223u; return (int)(rng >> 8)*(1.0f/8388608) - 1; }

/* one block of program: a decaying tone burst every 50 ms plus noise, the same for every run */
static void program(float in[2][BLOCK], int b, float amp)
{
    for (int i = 0; i < BLOCK; ++i) {
        int n = b*BLOCK + i;
        float e = expf(-(n % 2400)/600.0f);
        in[0][i] = amp*(.6f*e*sinf(6.2831853f*110*n/RATE) + .1f*noise());
        in[1][i] = amp*(.6f*e*sinf(6.2831853f*165*n/RATE) + .1f*noise());
    }
}

static double energy(const struct plug *p)
{
    double e = 0;
    for (int c = 0; c < 2; ++c) for (int i = 0; i < BLOCK; ++i) e += (double)p->out[c][i]*p->out[c][i];
    return e;
}

/* old and new from the same settings, MOD ports set by `mods`; returns 1 when every sample matches.
   Run one after the other with the same rand() seed: Heat's random LFO draws from libc's rand(). */
static float outs[2][BLOCKS][2][BLOCK];
static int same_as_old(const LADSPA_Descriptor *od, const LADSPA_Descriptor *nd, int mod, const float mods[5], int randomize)
{
    static struct plug a, b;
    open_plug(&a, od); open_plug(&b, nd);
    assert(nd->PortCount == od->PortCount + 5 && 4 + mod == (int)od->PortCount);
    if (randomize)                         /* any old setting: a random point of each control's range */
        for (unsigned long i = 4; i < od->PortCount; ++i) {
            const LADSPA_PortRangeHint *h = &od->PortRangeHints[i];
            float t = .5f + .5f*noise(), v = h->LowerBound + t*(h->UpperBound - h->LowerBound);
            if (LADSPA_IS_HINT_INTEGER(h->HintDescriptor) || LADSPA_IS_HINT_TOGGLED(h->HintDescriptor)) v = roundf(v);
            if (!strncmp(od->PortNames[i], "Key", 3)) v = 0;   /* key bus off: the two builds have separate buses */
            a.ctl[i] = b.ctl[i] = v;
        }
    else                                   /* the old build's load values: a hint default may have moved since */
        for (unsigned long i = 4; i < od->PortCount; ++i) b.ctl[i] = a.ctl[i];
    for (int k = 0; k < 5; ++k) b.ctl[4 + mod + k] = mods[k];
    unsigned seed = rng;
    const struct plug *ps[2] = {&a, &b};
    for (int w = 0; w < 2; ++w) {
        struct plug *p = (struct plug *)ps[w];
        rng = seed; srand(1);
        for (int blk = 0; blk < BLOCKS; ++blk) {
            program(p->in, blk, 1);
            p->d->run(p->h, BLOCK);
            memcpy(outs[w][blk], p->out, sizeof p->out);
        }
        p->d->cleanup(p->h);
    }
    return !memcmp(outs[0], outs[1], sizeof outs[0]);
}

int main(int argc, char **argv)
{
    if (argc != 3) { fprintf(stderr, "usage: %s old.so new.so\n", argv[0]); return 2; }
    void *old = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL), *new = dlopen(argv[2], RTLD_NOW | RTLD_LOCAL);
    if (!old || !new) { fprintf(stderr, "%s\n", dlerror()); return 2; }
    setvbuf(stdout, NULL, _IONBF, 0);

    for (unsigned f = 0; f < sizeof fx / sizeof *fx; ++f) {
        const LADSPA_Descriptor *od = find(old, fx[f].uid), *nd = find(new, fx[f].uid);
        assert(od && nd);
        const LADSPA_PortRangeHint *mh = nd->PortRangeHints + 4 + fx[f].mod;
        /* the old-project load: SampQOL fills missing parameters with the lower bounds */
        float lower[5], defaults[5], silent_bus[5] = {2, (float)fx[f].level, 100, 1, 10};
        for (int k = 0; k < 5; ++k) { lower[k] = mh[k].LowerBound; defaults[k] = hint_default(&mh[k]); }
        assert(lower[0] == 0 && defaults[0] == 0);      /* Off is both the lower bound and the default */
        assert(mh[1].UpperBound == fx[f].mod - 1);
        for (int r = 0; r < 4; ++r) {
            assert(same_as_old(od, nd, fx[f].mod, lower, r));
            assert(same_as_old(od, nd, fx[f].mod, defaults, r));
            assert(same_as_old(od, nd, fx[f].mod, silent_bus, r));   /* bus B: nobody sends it */
        }
        printf("%-15s off: bit-identical to the old build (old-project load, defaults, silent bus; 4 settings each)\n", fx[f].name);
    }

    /* On: a Pump on another track sends bus A; each effect's level follows it */
    static struct plug sender, fol, ref;
    const LADSPA_Descriptor *pump = find(new, HS_PUMP_ID);
    for (unsigned f = 0; f < sizeof fx / sizeof *fx; ++f) {
        const LADSPA_Descriptor *nd = find(new, fx[f].uid);
        open_plug(&sender, pump);
        sender.ctl[4 + PU_SEND] = 1;
        open_plug(&fol, nd); open_plug(&ref, nd);
        int m = 4 + fx[f].mod;
        const LADSPA_PortRangeHint *lh = &nd->PortRangeHints[4 + fx[f].level];
        float range = lh->UpperBound - lh->LowerBound, base = fol.ctl[4 + fx[f].level];
        float depth = -fminf(40, 100*(base - lh->LowerBound)/range);   /* stays inside the range */
        fol.ctl[m + HS_MOD_SRC] = 1;
        fol.ctl[m + HS_MOD_TARGET] = fx[f].level;
        fol.ctl[m + HS_MOD_DEPTH] = depth;
        fol.ctl[m + HS_MOD_ATTACK] = 5;
        fol.ctl[m + HS_MOD_RELEASE] = 50;
        double on = 0, off = 0, after_on = 0, after_off = 0;
        int identical_after = 1;
        rng = 7;
        for (int blk = 0; blk < 800; ++blk) {
            int loud = blk < 400;                       /* sender plays 0 dBFS, then silence */
            for (int c = 0; c < 2; ++c) for (int i = 0; i < BLOCK; ++i)
                sender.in[c][i] = loud ? sinf(6.2831853f*1000*(blk*BLOCK + i)/RATE) : 0;
            pump->run(sender.h, BLOCK);
            program(fol.in, blk, .25f); memcpy(ref.in, fol.in, sizeof fol.in);
            nd->run(fol.h, BLOCK); nd->run(ref.h, BLOCK);
            if (blk >= 200 && blk < 400) { on += energy(&fol); off += energy(&ref); }
            if (blk >= 700) {                           /* 50 ms release x ~30 after the sender stops */
                after_on += energy(&fol); after_off += energy(&ref);
                if (memcmp(fol.out, ref.out, sizeof fol.out)) identical_after = 0;
            }
        }
        float moved = 10*log10f((float)(on/off)), want = depth*.01f*range;
        printf("%-15s on: level %+.1f dB of range %g, depth %.0f%%: output moved %+.2f dB (target %+.2f in its own units); "
               "after release %s\n", fx[f].name, base, range, depth, moved, want,
               identical_after ? "bit-identical to off" : "DIFFERENT");
        assert(moved < -2);                             /* the target followed the bus */
        assert(identical_after);
        (void)after_on; (void)after_off;
        pump->cleanup(sender.h); nd->cleanup(fol.h); nd->cleanup(ref.h);
    }

    /* log target: the Channel Strip high-pass follows the bus up and takes the 110 Hz tone with it */
    {
        const LADSPA_Descriptor *nd = find(new, HS_CHANNEL_ID);
        open_plug(&sender, pump); sender.ctl[4 + PU_SEND] = 1;
        open_plug(&fol, nd); open_plug(&ref, nd);
        int m = 4 + CH_MOD;
        fol.ctl[m + HS_MOD_SRC] = 1; fol.ctl[m + HS_MOD_TARGET] = CH_HP; fol.ctl[m + HS_MOD_DEPTH] = 100;
        fol.ctl[m + HS_MOD_ATTACK] = 5; fol.ctl[m + HS_MOD_RELEASE] = 50;
        double on = 0, off = 0;
        for (int blk = 0; blk < 400; ++blk) {
            for (int c = 0; c < 2; ++c) for (int i = 0; i < BLOCK; ++i)
                sender.in[c][i] = sinf(6.2831853f*1000*(blk*BLOCK + i)/RATE);
            pump->run(sender.h, BLOCK);
            for (int c = 0; c < 2; ++c) for (int i = 0; i < BLOCK; ++i)
                fol.in[c][i] = ref.in[c][i] = .25f*sinf(6.2831853f*110*(blk*BLOCK + i)/RATE);
            nd->run(fol.h, BLOCK); nd->run(ref.h, BLOCK);
            if (blk >= 200) { on += energy(&fol); off += energy(&ref); }
        }
        float moved = 10*log10f((float)(on/off));
        printf("Channel Strip   log target: HP 20 Hz -> 350 Hz at full envelope cuts a 110 Hz tone %+.1f dB\n", moved);
        assert(moved < -10);
    }
    printf("check-mod: all passed\n");
    return 0;
}
