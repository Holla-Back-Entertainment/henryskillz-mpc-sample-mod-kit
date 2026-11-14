#ifndef HS_MOD_H
#define HS_MOD_H
/* MOD: any HS effect parameter follows a key bus envelope (hs_env.h). Five control ports appended
   after an effect's own: Source Off/A-D, Target (the effect's own parameter index), Depth +/-100 %
   of the target's range, Attack and Release of the follower. Old projects load the missing ports at
   their lower bounds, so Source must stay "Off" at 0. The effect hears set value + depth x envelope,
   worked out once per run() block; SampQOL's bar and readout keep the set value, and hs-fx turns the
   drawn knob to the heard value (hs_mod_knob, read through each effect's hs_<name>_mod_knob). */
enum { HS_MOD_SRC, HS_MOD_TARGET, HS_MOD_DEPTH, HS_MOD_ATTACK, HS_MOD_RELEASE, HS_MOD_PORTS };

/* The MOD and MOD TIME pages for banks.inc, base = the effect's first MOD parameter. */
#define HS_MOD_BANKS(base, module) \
    {"MOD", module, {(base)+HS_MOD_SRC, (base)+HS_MOD_TARGET, (base)+HS_MOD_DEPTH, -1, -1, -1}, \
        {"Source", "Target", "Depth", "", "", ""}, {"", "", "%"}}, \
    {"MOD TIME", module, {(base)+HS_MOD_ATTACK, (base)+HS_MOD_RELEASE, -1, -1, -1, -1}, \
        {"Attack", "Release", "", "", "", ""}, {"MS", "MS"}}

#ifndef __cplusplus
#include <math.h>
#include "ladspa.h"
#include "hs_env.h"

#define HS_MOD_TYPE (LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL)
#define HS_MOD_TYPES HS_MOD_TYPE, HS_MOD_TYPE, HS_MOD_TYPE, HS_MOD_TYPE, HS_MOD_TYPE
#define HS_MOD_NAMES "Mod Source (Off,A,B,C,D)", "Mod Target", "Mod Depth (%)", "Mod Attack (ms)", "Mod Release (ms)"
#define HS_MOD_DEFAULTS 0, 0, 0, 5.623f, 223.6f     /* the hint defaults below */
/* first = port index of the Source port, targets = how many of the effect's own controls come before it */
#define HS_MOD_HINTS(first, targets) \
    [(first)+HS_MOD_SRC] = {LADSPA_HINT_BOUNDED_BELOW|LADSPA_HINT_BOUNDED_ABOVE|LADSPA_HINT_INTEGER|LADSPA_HINT_DEFAULT_0, 0, HS_ENV_BUSES}, \
    [(first)+HS_MOD_TARGET] = {LADSPA_HINT_BOUNDED_BELOW|LADSPA_HINT_BOUNDED_ABOVE|LADSPA_HINT_INTEGER|LADSPA_HINT_DEFAULT_0, 0, (targets)-1}, \
    [(first)+HS_MOD_DEPTH] = {LADSPA_HINT_BOUNDED_BELOW|LADSPA_HINT_BOUNDED_ABOVE|LADSPA_HINT_DEFAULT_MIDDLE, -100, 100}, \
    [(first)+HS_MOD_ATTACK] = {LADSPA_HINT_BOUNDED_BELOW|LADSPA_HINT_BOUNDED_ABOVE|LADSPA_HINT_LOGARITHMIC|LADSPA_HINT_DEFAULT_LOW, 1, 1000}, \
    [(first)+HS_MOD_RELEASE] = {LADSPA_HINT_BOUNDED_BELOW|LADSPA_HINT_BOUNDED_ABOVE|LADSPA_HINT_LOGARITHMIC|LADSPA_HINT_DEFAULT_MIDDLE, 10, 5000}

/* env: the follower. shown/set/heard: published for the editor's knob, written by run() and read
   from the UI thread with atomics, never locked. shown = target + 1 while MOD moves it, else 0. */
struct hs_mod { float env; int shown; float set, heard; };

static inline void hs_mod_publish(struct hs_mod *m, int target, float set, float heard)
{
    __atomic_store(&m->set, &set, __ATOMIC_RELAXED);
    __atomic_store(&m->heard, &heard, __ATOMIC_RELAXED);
    __atomic_store_n(&m->shown, target + 1, __ATOMIC_RELEASE);
}

/* v = the effect's clamped control values, MOD ports at v[first]; hints = the effect's hints for
   v[0..]. Call before the effect compares v with its last settings. Source Off leaves v untouched.
   ponytail: the bus is a block-old peak, so the follower runs at block rate, not per sample. */
static inline void hs_mod_apply(struct hs_mod *m, float *v, int first, const LADSPA_PortRangeHint *hints,
                                unsigned long frames, unsigned long rate)
{
    const float *mod = v + first;
    int src = (int)lroundf(mod[HS_MOD_SRC]), t = (int)lroundf(mod[HS_MOD_TARGET]);
    if (!src || t < 0 || t >= first || !frames || !rate) { hs_mod_publish(m, -1, 0, 0); return; }
    float peak = hs_env_read(src);
    float level = peak > 1e-3f ? fminf(1, 1 + 20*log10f(peak)/60) : 0;   /* -60..0 dBFS -> 0..1 */
    float ms = level > m->env ? mod[HS_MOD_ATTACK] : mod[HS_MOD_RELEASE];
    m->env += -expm1f(-1000.0f*frames/(rate*ms))*(level - m->env);
    if (!(m->env >= 1e-6f)) m->env = 0;                                  /* denormal / NaN */
    const LADSPA_PortRangeHint *h = &hints[t];
    float lo = h->LowerBound, hi = h->UpperBound, d = mod[HS_MOD_DEPTH]*.01f*m->env;
    if (!LADSPA_IS_HINT_BOUNDED_BELOW(h->HintDescriptor) || !LADSPA_IS_HINT_BOUNDED_ABOVE(h->HintDescriptor)) {
        hs_mod_publish(m, -1, 0, 0);
        return;
    }
    float set = v[t], x = LADSPA_IS_HINT_LOGARITHMIC(h->HintDescriptor) && lo > 0 && v[t] > 0
        ? v[t]*powf(hi/lo, d) : v[t] + d*(hi - lo);
    v[t] = fminf(hi, fmaxf(lo, x));
    hs_mod_publish(m, t, set, v[t]);
}

/* The faceplate knob has HS_MOD_KNOB steps (the bar under it: 74 pixels, 0-73). Where x sits on it,
   with the host's own mapping: linear, or a log curve for logarithmic hints; integers round first. */
enum { HS_MOD_KNOB = 73 };
static inline int hs_mod_knob_step(const LADSPA_PortRangeHint *h, float x)
{
    float lo = h->LowerBound, hi = h->UpperBound, f;
    if (!(hi > lo) || !isfinite(x)) return 0;
    if (LADSPA_IS_HINT_INTEGER(h->HintDescriptor) || LADSPA_IS_HINT_TOGGLED(h->HintDescriptor)) x = roundf(x);
    f = LADSPA_IS_HINT_LOGARITHMIC(h->HintDescriptor) && lo > 0 && x > 0
        ? logf(x/lo)/logf(hi/lo) : (x - lo)/(hi - lo);
    return (int)lroundf(fminf(1, fmaxf(0, f))*HS_MOD_KNOB);
}

/* UI thread: the target the MOD is moving (-1: none) and *delta, the knob steps from the set value
   to the heard one. A difference of steps, so the knob sits exactly on SampQOL's bar when MOD rests. */
static inline int hs_mod_knob(const struct hs_mod *m, const LADSPA_PortRangeHint *hints, int *delta)
{
    int t = __atomic_load_n(&m->shown, __ATOMIC_ACQUIRE) - 1;
    float set, heard;
    __atomic_load(&m->set, &set, __ATOMIC_RELAXED);
    __atomic_load(&m->heard, &heard, __ATOMIC_RELAXED);
    *delta = t >= 0 ? hs_mod_knob_step(&hints[t], heard) - hs_mod_knob_step(&hints[t], set) : 0;
    return t;
}

/* Each effect exports one, next to its hints: int hs_<name>_mod_knob(const void *handle, int *delta). */
#define HS_MOD_KNOB_EXPORT(name, type, own_hints) \
    int name(const void *handle, int *delta); \
    int name(const void *handle, int *delta) \
    { \
        const type *s = handle; \
        *delta = 0; \
        return s ? hs_mod_knob(&s->mod, (own_hints), delta) : -1; \
    }
#endif
#endif
