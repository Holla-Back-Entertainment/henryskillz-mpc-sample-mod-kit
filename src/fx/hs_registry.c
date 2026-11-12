/* hs_registry.c: remembers every live HS effect instance and where the host put its control ports,
 * so the preset page can read the open effect's current settings (hs_ladspa_controls).
 *
 * At load, each descriptor's instantiate / connect_port / cleanup is swapped in place for a wrapper
 * so descriptor pointers stay the same: hs-fx compares them to find the open editor. The descriptor
 * tables are deliberately writable; this avoids changing protection on a page shared with unrelated
 * runtime data. The wrappers only record and then call the original. */
#include <ladspa.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>

enum { MAX_DESC = 16, MAX_INST = 256, MAX_PORT = 96 };   /* ponytail: fixed tables; past 256 live instances presets just can't read the extra ones */

struct orig { LADSPA_Handle (*inst)(const LADSPA_Descriptor *, unsigned long);
              void (*conn)(LADSPA_Handle, unsigned long, LADSPA_Data *);
              void (*clean)(LADSPA_Handle); const LADSPA_Descriptor *d; };
static struct orig orig[MAX_DESC];
static struct inst { LADSPA_Handle h; int d; LADSPA_Data *port[MAX_PORT]; } inst[MAX_INST];
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

static struct inst *find(LADSPA_Handle h)
{
    for (int i = 0; i < MAX_INST; ++i) if (inst[i].h == h) return &inst[i];
    return NULL;
}

static void note(LADSPA_Handle h, int d)
{
    if (!h) return;
    pthread_mutex_lock(&lock);
    struct inst *r = find(NULL);
    if (r) { memset(r, 0, sizeof *r); r->h = h; r->d = d; }
    pthread_mutex_unlock(&lock);
}
static void port(LADSPA_Handle h, unsigned long p, LADSPA_Data *x)
{
    pthread_mutex_lock(&lock);
    struct inst *r = find(h);
    if (r && p < MAX_PORT) r->port[p] = x;
    pthread_mutex_unlock(&lock);
}
static void forget(LADSPA_Handle h)
{
    pthread_mutex_lock(&lock);
    struct inst *r = find(h);
    if (r) r->h = NULL;
    pthread_mutex_unlock(&lock);
}

#define WRAP(i) \
    static LADSPA_Handle inst##i(const LADSPA_Descriptor *d, unsigned long rate) \
    { LADSPA_Handle h = orig[i].inst(d, rate); note(h, i); return h; } \
    static void conn##i(LADSPA_Handle h, unsigned long p, LADSPA_Data *x) { port(h, p, x); orig[i].conn(h, p, x); } \
    static void clean##i(LADSPA_Handle h) { forget(h); orig[i].clean(h); }
WRAP(0) WRAP(1) WRAP(2) WRAP(3) WRAP(4) WRAP(5) WRAP(6) WRAP(7)
WRAP(8) WRAP(9) WRAP(10) WRAP(11) WRAP(12) WRAP(13) WRAP(14) WRAP(15)
#define ROW(i) {inst##i, conn##i, clean##i, NULL}
static const struct orig wrappers[MAX_DESC] = { ROW(0), ROW(1), ROW(2), ROW(3), ROW(4), ROW(5), ROW(6), ROW(7),
                                                ROW(8), ROW(9), ROW(10), ROW(11), ROW(12), ROW(13), ROW(14), ROW(15) };

const LADSPA_Descriptor *ladspa_descriptor(unsigned long index);

__attribute__((constructor)) static void hs_registry_init(void)
{
    for (int i = 0; i < MAX_DESC; ++i) {
        LADSPA_Descriptor *d = (LADSPA_Descriptor *)ladspa_descriptor((unsigned long)i);
        if (!d) break;
        if (d->instantiate == wrappers[i].inst) continue;
        orig[i] = (struct orig){ d->instantiate, d->connect_port, d->cleanup, d };
        d->instantiate = wrappers[i].inst; d->connect_port = wrappers[i].conn; d->cleanup = wrappers[i].clean;
    }
}

/* Current values of the instance's control inputs, in the host's parameter order (control input
 * ports in port order). lo/hi are the LADSPA bounds (0..1 when unbounded). integer[] is flags:
 * 1 = integer or toggled, 2 = logarithmic (the host maps its 0..1000 knob on a log curve).
 * Returns the count, or -1 when the handle is not a live HS instance. *label gets the effect label. */
int hs_ladspa_controls(const void *h, const char **label, float *val, float *lo, float *hi, unsigned char *integer, int max)
{
    int n = -1;
    pthread_mutex_lock(&lock);
    struct inst *r = h ? find((LADSPA_Handle)h) : NULL;
    if (r) {
        const LADSPA_Descriptor *d = orig[r->d].d;
        if (label) *label = d->Label;
        n = 0;
        for (unsigned long p = 0; p < d->PortCount && p < MAX_PORT && n < max; ++p) {
            LADSPA_PortDescriptor pd = d->PortDescriptors[p];
            if (!LADSPA_IS_PORT_CONTROL(pd) || !LADSPA_IS_PORT_INPUT(pd)) continue;
            const LADSPA_PortRangeHint *rh = &d->PortRangeHints[p];
            int b = LADSPA_IS_HINT_BOUNDED_BELOW(rh->HintDescriptor) && LADSPA_IS_HINT_BOUNDED_ABOVE(rh->HintDescriptor);
            lo[n] = b ? rh->LowerBound : 0; hi[n] = b ? rh->UpperBound : 1;
            integer[n] = (LADSPA_IS_HINT_INTEGER(rh->HintDescriptor) || LADSPA_IS_HINT_TOGGLED(rh->HintDescriptor))
                       | (LADSPA_IS_HINT_LOGARITHMIC(rh->HintDescriptor) && lo[n] > 0 ? 2 : 0);
            val[n] = r->port[p] ? *r->port[p] : lo[n];
            ++n;
        }
    }
    pthread_mutex_unlock(&lock);
    return n;
}
