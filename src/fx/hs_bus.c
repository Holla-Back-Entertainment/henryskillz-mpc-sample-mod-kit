#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "ladspa.h"
#include "hs_bus.h"
#include "hs_bus_curve.h"

enum { IL, IR, OL, OR, FIRST, PORTS = FIRST + BUS_COUNT };
struct biquad { float b0,b1,b2,a1,a2, z1[2],z2[2]; };
struct bus {
    float *port[PORTS];
    unsigned long rate;
    float values[BUS_COUNT], curve[25], attack, release, peak_decay, peak;
    float makeup, mix, hp, reduction, slow, balance, fixed_offset, auto_offset;
    float auto_fast_attack, auto_fast_release, auto_slow_release, auto_slow_lo, auto_slow_hi, auto_voicing;
    struct biquad sc;
    int ready, automatic;
    struct hs_mod mod;
};

static const float defaults[] = {20, 1, 0, 0, 0, 0, 100, 0, HS_MOD_DEFAULTS};
#define B (LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE)
#define D0 (B | LADSPA_HINT_DEFAULT_0)
#define INT0 (D0 | LADSPA_HINT_INTEGER)
#define MAX (B | LADSPA_HINT_DEFAULT_MAXIMUM)
static const LADSPA_PortRangeHint hints[PORTS] = {
    [FIRST+BUS_THRESHOLD] = {MAX,-20,20},
    [FIRST+BUS_RATIO] = {B|LADSPA_HINT_INTEGER|LADSPA_HINT_DEFAULT_LOW,0,6},
    [FIRST+BUS_MAKEUP] = {D0,-5,15},
    [FIRST+BUS_ATTACK] = {INT0,-4,2},
    [FIRST+BUS_RELEASE] = {INT0,-6,0},
    [FIRST+BUS_HPF] = {D0,0,185},
    [FIRST+BUS_MIX] = {MAX,0,100},
    [FIRST+BUS_BYPASS] = {D0|LADSPA_HINT_TOGGLED,0,1},
    HS_MOD_HINTS(FIRST+BUS_MOD, BUS_MOD)
};
#undef B
#undef D0
#undef INT0
#undef MAX

static inline float alpha(unsigned long rate, float ms)
{
    return -expm1f(-1000.0f/(rate*ms));
}

static LADSPA_Handle instantiate(const LADSPA_Descriptor *desc, unsigned long rate)
{
    (void)desc;
    if (!rate) return NULL;
    struct bus *s = calloc(1, sizeof *s);
    if (s) {
        s->rate = rate;
        s->peak_decay = 1-alpha(rate,2.5f);
        s->auto_fast_release = alpha(rate,16);
        s->auto_slow_release = alpha(rate,1040);
        s->auto_slow_lo = alpha(rate,450);
        s->auto_slow_hi = alpha(rate,830);
    }
    return s;
}

static void activate(LADSPA_Handle handle)
{
    struct bus *s = handle;
    s->ready = 0;
    s->peak = s->reduction = s->slow = s->balance = s->auto_voicing = 0;
    memset(&s->sc, 0, sizeof s->sc);
}

static void connect(LADSPA_Handle handle, unsigned long port, LADSPA_Data *value)
{
    struct bus *s = handle;
    if (port < PORTS) s->port[port] = value;
}

static void configure(struct bus *s, const float *v)
{
    static const float attack_ms[] = {.12f,.52f,1.25f,2.35f,7.5f,15.5f,22.5f};
    static const float release_ms[] = {51,70,88,120,156,220,51};
    static const float fixed_att[] = {.077f,.075f,.07f,.054f,0,-.049f,-.092f};
    static const float fixed_rel[] = {0,.017f,.028f,.04f,.048f,.058f};
    static const float long_att[] = {.08f,.08f,.078f,.073f,.058f,.034f,.013f};
    int attack = (int)lroundf(v[BUS_ATTACK])+4, release = (int)lroundf(v[BUS_RELEASE])+6;
    /* The faceplate's own step (bus_marking: a 74-px bar, step = (bar*6+36)/73), so the ratio heard is
       always the label shown; the 1.5 load default plays 2:1. Plain rounding put 3:1 under a 2:1 label. */
    int ratio = ((int)lroundf(v[BUS_RATIO] * (73.f / 6)) * 6 + 36) / 73;
    float t = (v[BUS_THRESHOLD]+20)*.2f;
    int row = t >= 8 ? 7 : (int)t;
    for (int i = 0; i < 25; ++i)
        s->curve[i] = .001f*((1-(t-row))*bus_gr[row][ratio][i] + (t-row)*bus_gr[row+1][ratio][i]);
    s->attack = alpha(s->rate, attack_ms[attack]);
    s->release = alpha(s->rate, release_ms[release]);
    s->auto_fast_attack = alpha(s->rate, .61f*attack_ms[attack]);
    s->automatic = release == 6;
    float blend = (release < 6 ? release : 0)/5.0f;
    s->fixed_offset = fixed_att[attack] + (long_att[attack]-fixed_att[attack])*blend;
    if (release < 5) s->fixed_offset = fixed_att[attack]*(1-blend)+fixed_rel[release];
    s->auto_offset = -.004f*(attack-4);
    s->makeup = powf(10, v[BUS_MAKEUP]*.05f);
    s->mix = .01f*v[BUS_MIX];
    /* BUS_BYPASS stays a port so old projects load, but is ignored: the one Bypass is SampQOL's B3. */
    float hp = v[BUS_HPF] < 10.0f ? 0 : fmaxf(20.0f, v[BUS_HPF]);
    if (!s->ready || hp != s->hp) {
        /* The reference detector rejects bass at about 12 dB/oct. */
        if (hp) {
            float w = 5.71769862953f * hp/s->rate;
            float c = cosf(w), a = .707106781f*sinf(w), gain = 1/(1+a);
            s->sc.b0 = (1+c)*.5f*gain;
            s->sc.b1 = -2*s->sc.b0; s->sc.b2 = s->sc.b0;
            s->sc.a1 = -2*c*gain; s->sc.a2 = (1-a)*gain;
        } else {
            memset(&s->sc, 0, sizeof s->sc);
        }
        s->hp = hp;
    }
    memcpy(s->values, v, sizeof s->values);
    s->ready = 1;
}

static inline float detect(struct biquad *b, float x, int c)
{
    float y = b->b0*x+b->z1[c];
    b->z1[c] = b->b1*x-b->a1*y+b->z2[c];
    b->z2[c] = b->b2*x-b->a2*y;
    return y;
}

static inline float target(const struct bus *s, float peak)
{
    if (peak <= .000251188643f) return 0;
    if (peak >= 1) return s->curve[24];
    float pos = fminf(24, 24 + (20.0f/3.0f)*log10f(peak));
    int i = (int)pos;
    return i >= 24 ? s->curve[24] : s->curve[i]+(pos-i)*(s->curve[i+1]-s->curve[i]);
}

static void run(LADSPA_Handle handle, unsigned long frames)
{
    struct bus *s = handle;
    if (!s->port[IL] || !s->port[IR] || !s->port[OL] || !s->port[OR]) return;
    float v[BUS_COUNT];
    for (int i = 0; i < BUS_COUNT; ++i) {
        float *p = s->port[FIRST+i];
        const LADSPA_PortRangeHint *h = &hints[FIRST+i];
        v[i] = p && isfinite(*p) ? fminf(h->UpperBound,fmaxf(h->LowerBound,*p)) : defaults[i];
    }
    hs_mod_apply(&s->mod, v, BUS_MOD, hints + FIRST, frames, s->rate);
    if (!s->ready || memcmp(v,s->values,sizeof v)) configure(s,v);
    float peak = s->peak, gr = s->reduction, slow = s->slow, balance = s->balance, voicing = s->auto_voicing;
    for (unsigned long i = 0; i < frames; ++i) {
        float l = s->port[IL][i], r = s->port[IR][i];
        float dl = s->hp ? detect(&s->sc,l,0) : l, dr = s->hp ? detect(&s->sc,r,1) : r;
        float linked = fmaxf(fabsf(dl),fabsf(dr));
        peak *= s->peak_decay;
        if (linked > peak) {
            peak = linked;
            balance = fabsf(fabsf(dl)-fabsf(dr))/fmaxf(linked,1.e-20f);
        }
        float want = target(s, peak);
        want *= 1-balance*fmaxf(0, .0066f*(want-8.0f));
        float amount;
        if (s->automatic) {
            /* Two release memories: a fast stage for short hits and a
               slow stage whose contribution grows with sustained signal. */
            float blend = fminf(1,fmaxf(0,(want-15.5f)*.088f));
            float quick = fminf(want*.433f, 7.05f-1.7f*balance);
            float held = (want-quick)*(1.03f-.165f*blend-.365f*balance+s->auto_offset);
            float slow_attack = (s->auto_slow_lo+(s->auto_slow_hi-s->auto_slow_lo)*blend)*(1-.12f*balance);
            float fast_attack = s->auto_fast_attack*(1-.37f*balance);
            gr += (quick > gr ? fast_attack : s->auto_fast_release)*(quick-gr);
            slow += (held > slow ? slow_attack : s->auto_slow_release)*(held-slow);
            /* Lower-level sustained notes lean on the slow stage. Keep
               its voicing through release; avoid a step as the key drops. */
            if (held > slow) voicing += fast_attack * ((1-blend)-voicing);
            amount = gr+slow*(1-.105f*voicing*fminf(1,gr/6.7f));
        } else {
            want += s->fixed_offset*fmaxf(0,want-8);
            float factor = fminf(1.8f, fmaxf(.85f, 1 + (26.8f-want)*.065f));
            factor *= 1-.37f*balance;
            gr += (want > gr ? s->attack*factor : s->release)*(want-gr);
            slow = 0;
            amount = gr;
        }
        float wet_gain = amount < .00001f ? 1 : exp2f(-.166096405f*amount);
        float gain = 1-s->mix+s->mix*s->makeup*wet_gain;
        s->port[OL][i] = l*gain;
        s->port[OR][i] = r*gain;
    }
    s->peak = peak > 1e-20f ? peak : 0;
    s->reduction = gr > 1e-12f ? gr : 0;
    s->slow = slow > 1e-12f ? slow : 0;
    s->balance = balance;
    s->auto_voicing = voicing;
    for (int c = 0; c < 2; ++c) {
        if (!(fabsf(s->sc.z1[c]) >= 1e-24f && isfinite(s->sc.z1[c]))) s->sc.z1[c] = 0;
        if (!(fabsf(s->sc.z2[c]) >= 1e-24f && isfinite(s->sc.z2[c]))) s->sc.z2[c] = 0;
    }
}

#define AI (LADSPA_PORT_INPUT | LADSPA_PORT_AUDIO)
#define AO (LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO)
#define C (LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL)
static const LADSPA_PortDescriptor types[] = {AI,AI,AO,AO,C,C,C,C,C,C,C,C,HS_MOD_TYPES};
static const char *const names[] = {
    "Input L", "Input R", "Output L", "Output R", "Threshold (dB)", "Ratio step",
    "Makeup (dB)", "Attack step", "Release step", "Sidechain HPF (Hz)", "Dry/Wet Mix (%)", "Bypass", HS_MOD_NAMES
};
_Static_assert(sizeof names / sizeof *names == PORTS &&
               sizeof types / sizeof *types == PORTS &&
               sizeof defaults / sizeof *defaults == BUS_COUNT, "Bus comp port count");

/* The open editor turns this effect's MOD target knob to the heard value (hs_mod.h). */
HS_MOD_KNOB_EXPORT(hs_bus_mod_knob, struct bus, hints + FIRST)

LADSPA_Descriptor hs_bus_descriptor = {
    .UniqueID = HS_BUS_ID, .Label = "HS Bus Compressor", .Name = "HS Bus Compressor",
    .Maker = "Henry Skillz", .Copyright = "Prototype",
    .Properties = LADSPA_PROPERTY_HARD_RT_CAPABLE,
    .PortCount = PORTS, .PortDescriptors = types, .PortNames = names, .PortRangeHints = hints,
    .instantiate = instantiate, .connect_port = connect, .activate = activate, .run = run, .cleanup = free
};
