/* HS Tube Strip: a tube channel strip. Tube preamp drive, a 6 dB/oct high-pass, an opto
 * compressor and a four-band EQ, with the EQ before or after the compressor. Not a model of any unit.
 * DESIGN: HPF 30-140 Hz at 6 dB/oct (below 30 = off); opto threshold -30..+20 (dB re -10 dBFS),
 *   ratio 1-20, attack 2-200 ms, release 0.1-5 s; side-chain switch moves both mids out of the
 *   audio path into the detector (de-essing); bass shelf 15/30/60/150 Hz +-24 dB; mids 30 Hz-4.5 kHz
 *   and 200 Hz-20 kHz +-16 dB with a low/high Q switch; treble shelf 10/15/20/32 kHz +-20 dB.
 * GUESSED (marked "guess"): the tube curve and its 2nd-harmonic share, the threshold reference,
 *   the opto knee and its program-dependent release, the two Q values and the shelf slopes. */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "ladspa.h"
#include "hs_tube_strip.h"

enum { IL, IR, OL, OR, FIRST, PORTS = FIRST + TS_COUNT };
enum { BASS, LMID, HMID, TREBLE, BANDS };

static const float bass_hz[] = {15, 30, 60, 150}, treble_hz[] = {10000, 15000, 20000, 32000};

struct biquad { float b0, b1, b2, a1, a2, z1[2], z2[2]; };

struct tube_strip {
    float *port[PORTS];
    float rate;
    float values[TS_COUNT];
    struct biquad eq[BANDS], sc[2];
    float drive, hpf_coef, out_gain, threshold, slope, attack, release;
    float hp_x[2], hp_y[2], dc_x[2], dc_y[2], dc_coef, gr;
    int ready, hpf_on, eq_first, sc_on, bypass;
    struct hs_mod mod;
};

#define B (LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE)
#define LOG (B | LADSPA_HINT_LOGARITHMIC)
#define TOG (B | LADSPA_HINT_TOGGLED | LADSPA_HINT_DEFAULT_0)
#define STEP (B | LADSPA_HINT_INTEGER)
/* SampQOL loads the hint defaults: the strip comes in with the compressor idle and the EQ flat. */
static const float defaults[TS_COUNT] = {2.5f, 20, 0, 20, 5.75f, 20, 707.1f, 0, 0, 1, 0, 0, 367.4f, 2000, 0, 1, 0, 0, 0, HS_MOD_DEFAULTS};
static const LADSPA_PortRangeHint hints[PORTS] = {
    [FIRST+TS_DRIVE] = {B|LADSPA_HINT_DEFAULT_LOW, 0, 10},
    [FIRST+TS_HPF] = {B|LADSPA_HINT_DEFAULT_MINIMUM, 20, 140},
    [FIRST+TS_OUTPUT] = {B|LADSPA_HINT_DEFAULT_MIDDLE, -20, 20},
    [FIRST+TS_THRESHOLD] = {B|LADSPA_HINT_DEFAULT_MAXIMUM, -30, 20},
    [FIRST+TS_RATIO] = {B|LADSPA_HINT_DEFAULT_LOW, 1, 20},
    [FIRST+TS_ATTACK] = {LOG|LADSPA_HINT_DEFAULT_MIDDLE, 2, 200},
    [FIRST+TS_RELEASE] = {LOG|LADSPA_HINT_DEFAULT_MIDDLE, 100, 5000},
    [FIRST+TS_ORDER] = {TOG, 0, 1},
    [FIRST+TS_SC] = {TOG, 0, 1},
    [FIRST+TS_BASS_HZ] = {STEP|LADSPA_HINT_DEFAULT_1, 0, 3},
    [FIRST+TS_BASS_DB] = {B|LADSPA_HINT_DEFAULT_MIDDLE, -24, 24},
    [FIRST+TS_LMID_DB] = {B|LADSPA_HINT_DEFAULT_MIDDLE, -16, 16},
    [FIRST+TS_LMID_HZ] = {LOG|LADSPA_HINT_DEFAULT_MIDDLE, 30, 4500},
    [FIRST+TS_HMID_HZ] = {LOG|LADSPA_HINT_DEFAULT_MIDDLE, 200, 20000},
    [FIRST+TS_HMID_DB] = {B|LADSPA_HINT_DEFAULT_MIDDLE, -16, 16},
    [FIRST+TS_TREBLE_HZ] = {STEP|LADSPA_HINT_DEFAULT_1, 0, 3},
    [FIRST+TS_TREBLE_DB] = {B|LADSPA_HINT_DEFAULT_MIDDLE, -20, 20},
    [FIRST+TS_Q] = {TOG, 0, 1},
    [FIRST+TS_BYPASS] = {TOG, 0, 1},
    HS_MOD_HINTS(FIRST+TS_MOD, TS_MOD)
};
#undef B
#undef LOG
#undef TOG
#undef STEP

static inline float coef(float rate, float ms) { return -expm1f(-1000.0f/(rate*ms)); }

/* RBJ cookbook. kind: 0 peak, 1 low shelf, 2 high shelf. */
static void tune(struct biquad *f, float rate, int kind, float hz, float q, float db)
{
    hz = fminf(hz, .45f*rate);
    float A = powf(10, db/40), w = 6.28318530718f*hz/rate, c = cosf(w), s = sinf(w);
    float b0, b1, b2, a0, a1, a2;
    if (kind == 0) {
        float a = s/(2*q);
        b0 = 1 + a*A; b1 = -2*c; b2 = 1 - a*A; a0 = 1 + a/A; a1 = -2*c; a2 = 1 - a/A;
    } else {
        float a = s/2*sqrtf(2), r = 2*sqrtf(A)*a, sg = kind == 1 ? 1 : -1;   /* shelf slope 1 */
        b0 = A*((A+1) - sg*(A-1)*c + r); b1 = sg*2*A*((A-1) - sg*(A+1)*c); b2 = A*((A+1) - sg*(A-1)*c - r);
        a0 = (A+1) + sg*(A-1)*c + r;      a1 = -sg*2*((A-1) + sg*(A+1)*c);  a2 = (A+1) + sg*(A-1)*c - r;
    }
    f->b0 = b0/a0; f->b1 = b1/a0; f->b2 = b2/a0; f->a1 = a1/a0; f->a2 = a2/a0;
}

static inline float bq(struct biquad *f, float x, int c)
{
    float y = f->b0*x + f->z1[c];
    f->z1[c] = f->b1*x - f->a1*y + f->z2[c];
    f->z2[c] = f->b2*x - f->a2*y;
    return y;
}

static LADSPA_Handle instantiate(const LADSPA_Descriptor *d, unsigned long rate)
{
    (void)d;
    if (!rate) return NULL;
    struct tube_strip *s = calloc(1, sizeof *s);
    if (s) { s->rate = rate; s->dc_coef = 1 - 6.2831853f*5/rate; }
    return s;
}

static void activate(LADSPA_Handle h)
{
    struct tube_strip *s = h;
    s->ready = 0;
    s->gr = 0;
    for (int c = 0; c < 2; ++c) {
        for (int k = 0; k < BANDS; ++k) s->eq[k].z1[c] = s->eq[k].z2[c] = 0;
        for (int k = 0; k < 2; ++k) s->sc[k].z1[c] = s->sc[k].z2[c] = 0;
        s->hp_x[c] = s->hp_y[c] = s->dc_x[c] = s->dc_y[c] = 0;
    }
}

static void connect(LADSPA_Handle h, unsigned long port, LADSPA_Data *p)
{
    struct tube_strip *s = h;
    if (port < PORTS) s->port[port] = p;
}

static void configure(struct tube_strip *s, const float *v)
{
    float q = v[TS_Q] >= .5f ? 2.0f : .7f;                                   /* guess */
    tune(&s->eq[BASS], s->rate, 1, bass_hz[(int)lroundf(v[TS_BASS_HZ])], 0, v[TS_BASS_DB]);
    tune(&s->eq[LMID], s->rate, 0, v[TS_LMID_HZ], q, v[TS_LMID_DB]);
    tune(&s->eq[HMID], s->rate, 0, v[TS_HMID_HZ], q, v[TS_HMID_DB]);
    tune(&s->eq[TREBLE], s->rate, 2, treble_hz[(int)lroundf(v[TS_TREBLE_HZ])], 0, v[TS_TREBLE_DB]);
    /* the detector hears the mids as boosts, whatever their sign, so a de-esser works with a cut too */
    tune(&s->sc[0], s->rate, 0, v[TS_LMID_HZ], q, fabsf(v[TS_LMID_DB]));
    tune(&s->sc[1], s->rate, 0, v[TS_HMID_HZ], q, fabsf(v[TS_HMID_DB]));
    s->hpf_on = v[TS_HPF] >= 30;
    s->hpf_coef = expf(-6.2831853f*v[TS_HPF]/s->rate);
    s->drive = powf(10, v[TS_DRIVE]*1.2f*.05f);                               /* 0..+12 dB into the tube, guess */
    s->threshold = v[TS_THRESHOLD] - 10;                                      /* dBFS, guess: 0 on the knob = -10 dBFS */
    s->slope = 1 - 1/v[TS_RATIO];
    s->attack = coef(s->rate, v[TS_ATTACK]);
    s->release = coef(s->rate, v[TS_RELEASE]);
    s->out_gain = powf(10, v[TS_OUTPUT]*.05f);
    s->eq_first = v[TS_ORDER] >= .5f;
    s->sc_on = v[TS_SC] >= .5f;
    s->bypass = v[TS_BYPASS] >= .5f;
    memcpy(s->values, v, sizeof s->values);
    s->ready = 1;
}

/* Triode-ish: a small even term for the 2nd harmonic, then a soft top. Drive 0 is clean. */
static inline float tube(struct tube_strip *s, float x, int c)
{
    if (s->values[TS_DRIVE] <= 0) return x;
    float g = s->drive*x;
    float y = tanhf(.9f*(g + .08f*g*g))/(.9f*s->drive);   /* unity for small signals; 2nd-harmonic share guess */
    float out = y - s->dc_x[c] + s->dc_coef*s->dc_y[c];
    s->dc_x[c] = y; s->dc_y[c] = out;
    return out;
}

static inline float eq(struct tube_strip *s, float x, int c)
{
    x = bq(&s->eq[BASS], x, c);
    if (!s->sc_on) { x = bq(&s->eq[LMID], x, c); x = bq(&s->eq[HMID], x, c); }
    return bq(&s->eq[TREBLE], x, c);
}

static void run(LADSPA_Handle h, unsigned long frames)
{
    struct tube_strip *s = h;
    if (!s->port[IL] || !s->port[IR] || !s->port[OL] || !s->port[OR]) return;
    float v[TS_COUNT];
    for (int i = 0; i < TS_COUNT; ++i) {
        float *p = s->port[FIRST+i];
        const LADSPA_PortRangeHint *r = &hints[FIRST+i];
        v[i] = p && isfinite(*p) ? fminf(r->UpperBound, fmaxf(r->LowerBound, *p)) : defaults[i];
    }
    hs_mod_apply(&s->mod, v, TS_MOD, hints + FIRST, frames, (unsigned long)s->rate);
    if (!s->ready || memcmp(v, s->values, sizeof v)) configure(s, v);

    float gr = s->gr;
    for (unsigned long i = 0; i < frames; ++i) {
        float x[2] = {s->port[IL][i], s->port[IR][i]};
        if (s->bypass) { s->port[OL][i] = x[0]; s->port[OR][i] = x[1]; continue; }
        float det = 0;
        for (int c = 0; c < 2; ++c) {
            x[c] = tube(s, x[c], c);
            if (s->hpf_on) {
                float y = s->hpf_coef*(s->hp_y[c] + x[c] - s->hp_x[c]);
                s->hp_x[c] = x[c]; s->hp_y[c] = y; x[c] = y;
            }
            if (s->eq_first) x[c] = eq(s, x[c], c);
            float d = x[c];
            if (s->sc_on) d = bq(&s->sc[1], bq(&s->sc[0], d, c), c);
            det = fmaxf(det, fabsf(d));     /* ponytail: always stereo-linked */
        }
        /* opto: 6 dB soft knee, and the release slows as the cell holds more reduction (guess) */
        float over = (det > 1e-6f ? 20*log10f(det) : -120) - s->threshold, want;
        want = over <= -3 ? 0 : over >= 3 ? s->slope*over : s->slope*(over + 3)*(over + 3)/12;
        gr += (want > gr ? s->attack : s->release/(1 + gr*.1f))*(want - gr);
        if (gr > 60) gr = 60;   /* ponytail: caps the program-dependent release slowdown on absurd input */
        float vca = powf(10, -.05f*gr);
        for (int c = 0; c < 2; ++c) {
            float y = x[c]*vca;
            if (!s->eq_first) y = eq(s, y, c);
            s->port[c ? OR : OL][i] = y*s->out_gain;
        }
    }
    s->gr = isfinite(gr) && gr > 1e-9f ? gr : 0;
    for (int c = 0; c < 2; ++c) {
        float *st[2*BANDS + 4 + 6];
        int n = 0;
        for (int k = 0; k < BANDS; ++k) { st[n++] = &s->eq[k].z1[c]; st[n++] = &s->eq[k].z2[c]; }
        for (int k = 0; k < 2; ++k) { st[n++] = &s->sc[k].z1[c]; st[n++] = &s->sc[k].z2[c]; }
        st[n++] = &s->hp_x[c]; st[n++] = &s->hp_y[c]; st[n++] = &s->dc_x[c]; st[n++] = &s->dc_y[c];
        for (int k = 0; k < n; ++k) if (!(fabsf(*st[k]) >= 1e-24f && isfinite(*st[k]))) *st[k] = 0;
    }
}

float hs_tube_strip_gr(const void *handle)
{
    const struct tube_strip *s = handle;
    return s ? s->gr : 0;
}

#define AI (LADSPA_PORT_INPUT | LADSPA_PORT_AUDIO)
#define AO (LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO)
#define C (LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL)
static const LADSPA_PortDescriptor types[] = {AI,AI,AO,AO,C,C,C,C,C,C,C,C,C,C,C,C,C,C,C,C,C,C,C,HS_MOD_TYPES};
static const char *const names[] = {
    "Input L", "Input R", "Output L", "Output R", "Drive", "HPF (Hz, <30 off)", "Output (dB)",
    "Threshold (dB)", "Ratio", "Attack (ms)", "Release (ms)", "EQ Before Comp", "SC EQ",
    "Bass Hz (15,30,60,150)", "Bass (dB)", "Low Mid (dB)", "Low Mid (Hz)", "High Mid (Hz)", "High Mid (dB)",
    "Treble Hz (10k,15k,20k,32k)", "Treble (dB)", "High Q", "Bypass", HS_MOD_NAMES
};
_Static_assert(sizeof names / sizeof *names == PORTS && sizeof types / sizeof *types == PORTS,
               "Tube Strip port count");

LADSPA_Descriptor hs_tube_strip_descriptor = {
    .UniqueID = HS_TUBE_STRIP_ID, .Label = "HS Tube Strip", .Name = "HS Tube Strip",
    .Maker = "Henry Skillz", .Copyright = "Prototype",
    .Properties = LADSPA_PROPERTY_HARD_RT_CAPABLE,
    .PortCount = PORTS, .PortDescriptors = types, .PortNames = names, .PortRangeHints = hints,
    .instantiate = instantiate, .connect_port = connect, .activate = activate, .run = run, .cleanup = free
};
