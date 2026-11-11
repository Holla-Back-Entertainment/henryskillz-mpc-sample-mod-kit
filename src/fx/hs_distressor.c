/* HS Crush: a character compressor with eight curves, an Opto mode, detector filters and
 * harmonic distortion. Not a model of any unit.
 * DESIGN: knobs 0-10; attack 50 us-30 ms; release .05-3.5 s (to 20 s in Opto); the eight curves
 *   (1:1 = no compression, only the circuit's colour; 2:1 has a +15 dB knee; 2:1-4:1 long soft knees;
 *   6:1 the default; 10:1 Opto with its own detector; 20:1 and Max brick wall, Max slightly lower
 *   output and a release that is fast first then slower); detector HP and 6 kHz band emphasis;
 *   audio HP = Bessel, -3 dB 65 Hz, -12 dB 30 Hz, 18 dB/oct; Dist 2 mostly 2nd harmonic,
 *   THD .05-3 %; Dist 3 adds 3rd, up to 20 %, tape-like flattening, rising with VCA output level;
 *   clean THD .025-.3 %; output 8 ~ +4 levels; a British-style mode.
 * GUESSED (marked "guess"): every threshold and knee value, the input/output dB maps, the detector
 *   HP corner and band gain, the opto law, the British mode curve and all distortion amounts. */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "ladspa.h"
#include "hs_distressor.h"

enum { IL, IR, OL, OR, FIRST, PORTS = FIRST + DS_COUNT, RATIOS = 8, OPTO = 5, NUKE = 7 };

static const float ratio_value[RATIOS]  = {1, 2, 3, 4, 6, 10, 20, 1000};
static const float threshold_db[RATIOS] = {0, -26, -22, -20, -18, -18, -14, -16};  /* guess */
static const float knee_db[RATIOS]      = {0, 30, 24, 18, 10, 12, 3, 1};           /* 2:1 +15 dB each side; rest guess */

struct biquad { float b0, b1, b2, a1, a2, z1[2], z2[2]; };

struct distressor {
    float *port[PORTS];
    unsigned long rate;
    float values[DS_COUNT];
    float in_gain, out_gain, mix, slope, knee, threshold, attack, release, opto_charge, opto_tail;
    float d2, d3, dc_coef, hp1_coef;
    struct biquad det_hp, det_band, aud_hp;
    float gr, tail, hp1_x[2], hp1_y[2], dc_x[2], dc_y[2];
    int ready, ratio, det_mode, aud_hp_on, brit, bypass;
};

#ifndef OUT_MAKEUP_DB
#define OUT_MAKEUP_DB 4
#endif
static const float defaults[] = {5, 5, 5, 7.5f, 4, 0, 0, 0, 100, 0};
#define B (LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE)
#define TOG (B | LADSPA_HINT_TOGGLED | LADSPA_HINT_DEFAULT_0)
#define STEP (B | LADSPA_HINT_INTEGER)
static const LADSPA_PortRangeHint hints[PORTS] = {
    [FIRST+DS_INPUT] = {B|LADSPA_HINT_DEFAULT_MIDDLE,0,10},
    [FIRST+DS_ATTACK] = {B|LADSPA_HINT_DEFAULT_MIDDLE,0,10},
    [FIRST+DS_RELEASE] = {B|LADSPA_HINT_DEFAULT_MIDDLE,0,10},
    [FIRST+DS_OUTPUT] = {B|LADSPA_HINT_DEFAULT_HIGH,0,10},     /* 7.5: matches defaults[] */
    [FIRST+DS_RATIO] = {STEP|LADSPA_HINT_DEFAULT_MIDDLE,0,RATIOS-1},
    [FIRST+DS_DETECTOR] = {STEP|LADSPA_HINT_DEFAULT_0,0,3},
    [FIRST+DS_AUDIO] = {STEP|LADSPA_HINT_DEFAULT_0,0,5},
    [FIRST+DS_BRIT] = {TOG,0,1},
    [FIRST+DS_MIX] = {B|LADSPA_HINT_DEFAULT_MAXIMUM,0,100},
    [FIRST+DS_BYPASS] = {TOG,0,1}
};
#undef B
#undef TOG
#undef STEP

static inline float coef(unsigned long rate, float ms) { return -expm1f(-1000.0f/(rate*ms)); }

static void highpass(struct biquad *f, float rate, float hz, float q)
{
    float w = 6.28318530718f*hz/rate, c = cosf(w), a = sinf(w)/(2*q), g = 1/(1+a);
    f->b0 = (1+c)*.5f*g; f->b1 = -2*f->b0; f->b2 = f->b0; f->a1 = -2*c*g; f->a2 = (1-a)*g;
}

static void peak(struct biquad *f, float rate, float hz, float q, float db)
{
    float A = powf(10, db/40), w = 6.28318530718f*hz/rate, c = cosf(w), a = sinf(w)/(2*q), g = 1/(1+a/A);
    f->b0 = (1+a*A)*g; f->b1 = -2*c*g; f->b2 = (1-a*A)*g; f->a1 = -2*c*g; f->a2 = (1-a/A)*g;
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
    struct distressor *s = calloc(1, sizeof *s);
    if (s) {
        s->rate = rate;
        s->dc_coef = 1 - 6.2831853f*5/rate;
        highpass(&s->det_hp, rate, 100, .7071f);       /* guess: corner not published */
        peak(&s->det_band, rate, 6000, .8f, 12);        /* 6 kHz published; gain and Q guess */
        /* 3rd-order Bessel high-pass, -3 dB at 65 Hz: one real pole + one pair (Q .691) */
        highpass(&s->aud_hp, rate, 65/1.4476f, .691f);
        s->hp1_coef = expf(-6.2831853f*(65/1.3227f)/rate);
        s->opto_charge = coef(rate, 400);             /* guess */
        s->opto_tail = coef(rate, 3000);              /* guess */
    }
    return s;
}

static void activate(LADSPA_Handle h)
{
    struct distressor *s = h;
    s->ready = 0;
    s->gr = s->tail = 0;
    struct biquad *f[] = {&s->det_hp, &s->det_band, &s->aud_hp};
    for (int c = 0; c < 2; ++c) {
        for (int k = 0; k < 3; ++k) f[k]->z1[c] = f[k]->z2[c] = 0;
        s->hp1_x[c] = s->hp1_y[c] = s->dc_x[c] = s->dc_y[c] = 0;
    }
}

static void connect(LADSPA_Handle h, unsigned long port, LADSPA_Data *p)
{
    struct distressor *s = h;
    if (port < PORTS) s->port[port] = p;
}

static void configure(struct distressor *s, const float *v)
{
    int r = (int)lroundf(v[DS_RATIO]), aud = (int)lroundf(v[DS_AUDIO]);
    s->ratio = r;
    s->brit = v[DS_BRIT] >= .5f;
    /* British mode (guess): all-buttons style, past infinity, so loud hits come out quieter */
    s->slope = s->brit ? 1.15f : 1 - 1/ratio_value[r];
    s->knee = s->brit ? 4 : knee_db[r];
    s->threshold = s->brit ? -20 : threshold_db[r];
    float attack_ms = .05f*powf(600, v[DS_ATTACK]*.1f);                     /* 50 us .. 30 ms */
    float release_ms = 50*powf(r == OPTO ? 400 : 70, v[DS_RELEASE]*.1f);    /* .05 .. 3.5 s, Opto 20 s */
    s->attack = coef(s->rate, r == OPTO ? attack_ms*3 + 2 : attack_ms);    /* guess: opto is slower */
    s->release = coef(s->rate, release_ms);
    s->in_gain = powf(10, (4*v[DS_INPUT] - 20)*.05f);   /* guess: 0 -> -20 dB, 5 -> 0, 10 -> +20 */
    /* Output 7.5 is what SampQOL loads (DEFAULT_HIGH). Its make-up keeps a -12 dBFS
       signal near unity at load defaults, so inserting it doesn't drop the level. */
    s->out_gain = powf(10, (4*(v[DS_OUTPUT] - 7.5f) + OUT_MAKEUP_DB)*.05f);
    s->mix = v[DS_MIX]*.01f;
    s->det_mode = (int)lroundf(v[DS_DETECTOR]);
    s->aud_hp_on = aud == 1 || aud >= 4;
    int dist = aud == 2 || aud == 4 ? 2 : aud == 3 || aud == 5 ? 3 : 0;
    /* guess: 2nd-harmonic term sized for the published THD at 0 dBFS (clean .25 %, Dist 2 3 %) */
    s->d2 = dist ? .06f : .005f;
    s->d3 = dist == 3 ? 1.6f : 0;
    if (s->brit) s->d2 *= 2;
    s->bypass = v[DS_BYPASS] >= .5f;
    memcpy(s->values, v, sizeof s->values);
    s->ready = 1;
}

static inline float computer(const struct distressor *s, float level)
{
    float over = level - s->threshold, half = .5f*s->knee;
    if (over <= -half) return 0;
    if (over >= half) return s->slope*over;
    float t = over + half;
    return s->slope*t*t/(2*s->knee);
}

static inline float colour(struct distressor *s, float x, int c)
{
    float y = x + s->d2*x*x;
    if (s->d3 > 0) y = tanhf(s->d3*y)/s->d3;      /* tape-like rounding of top and bottom */
    float out = y - s->dc_x[c] + s->dc_coef*s->dc_y[c];
    s->dc_x[c] = y; s->dc_y[c] = out;
    return out;
}

static void run(LADSPA_Handle h, unsigned long frames)
{
    struct distressor *s = h;
    if (!s->port[IL] || !s->port[IR] || !s->port[OL] || !s->port[OR]) return;
    float v[DS_COUNT];
    for (int i = 0; i < DS_COUNT; ++i) {
        float *p = s->port[FIRST+i];
        const LADSPA_PortRangeHint *r = &hints[FIRST+i];
        v[i] = p && isfinite(*p) ? fminf(r->UpperBound, fmaxf(r->LowerBound, *p)) : defaults[i];
    }
    if (!s->ready || memcmp(v, s->values, sizeof v)) configure(s, v);

    float gr = s->gr, tail = s->tail;
    for (unsigned long i = 0; i < frames; ++i) {
        float dry[2] = {s->port[IL][i], s->port[IR][i]}, x[2];
        if (s->bypass) { s->port[OL][i] = dry[0]; s->port[OR][i] = dry[1]; continue; }
        float det = 0;
        for (int c = 0; c < 2; ++c) {
            x[c] = dry[c]*s->in_gain;
            if (s->aud_hp_on) {
                float y = s->hp1_coef*(s->hp1_y[c] + x[c] - s->hp1_x[c]);
                s->hp1_x[c] = x[c]; s->hp1_y[c] = y;
                x[c] = bq(&s->aud_hp, y, c);
            }
            float d = x[c];
            if (s->det_mode & 1) d = bq(&s->det_hp, d, c);
            if (s->det_mode & 2) d = bq(&s->det_band, d, c);
            det = fmaxf(det, fabsf(d));     /* ponytail: always stereo-linked, no Link button */
        }
        if (s->ratio || s->brit) {
            float want = det > 1e-6f ? computer(s, 20*log10f(det)) : 0;
            if (s->ratio == OPTO) {
                /* Opto (guess): the last few dB hang on a slow, program-dependent tail */
                float tail_want = fminf(3, want);
                tail += (tail_want > tail ? s->opto_charge : s->opto_tail)*(tail_want - tail);
                want = fmaxf(0, want - tail);
            } else {
                tail = 0;
            }
            float rel = s->release;
            if (s->ratio == NUKE) rel = fminf(1, rel*(1 + gr*.25f));   /* fast first, then slower */
            gr += (want > gr ? s->attack : rel)*(want - gr);
        } else {
            gr = tail = 0;
        }
        float vca = powf(10, -.05f*(gr + tail));
        for (int c = 0; c < 2; ++c) {
            float y = colour(s, x[c]*vca, c)*s->out_gain;    /* output after the colour: it sets level, not drive */
            float out = dry[c] + s->mix*(y - dry[c]);
            s->port[c ? OR : OL][i] = out;
        }
    }
    s->gr = isfinite(gr) && gr > 1e-9f ? gr : 0;
    s->tail = isfinite(tail) && tail > 1e-9f ? tail : 0;
    struct biquad *f[] = {&s->det_hp, &s->det_band, &s->aud_hp};
    for (int c = 0; c < 2; ++c) {
        float *st[] = {&f[0]->z1[c], &f[0]->z2[c], &f[1]->z1[c], &f[1]->z2[c], &f[2]->z1[c], &f[2]->z2[c],
                       &s->hp1_x[c], &s->hp1_y[c], &s->dc_x[c], &s->dc_y[c]};
        for (int k = 0; k < 10; ++k) if (!(fabsf(*st[k]) >= 1e-24f && isfinite(*st[k]))) *st[k] = 0;
    }
}

float hs_distressor_gr(const void *handle)
{
    const struct distressor *s = handle;
    return s ? s->gr + s->tail : 0;
}

#define AI (LADSPA_PORT_INPUT | LADSPA_PORT_AUDIO)
#define AO (LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO)
#define C (LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL)
static const LADSPA_PortDescriptor types[] = {AI,AI,AO,AO,C,C,C,C,C,C,C,C,C,C};
static const char *const names[] = {
    "Input L", "Input R", "Output L", "Output R", "Input", "Attack", "Release", "Output",
    "Ratio (1,2,3,4,6,10 Opto,20,Max)", "Detector (Norm,HP,Band,HP+Band)",
    "Audio (Clean,HP,Dist 2,Dist 3,HP+D2,HP+D3)", "British Mode", "Mix (%)", "Bypass"
};
_Static_assert(sizeof names / sizeof *names == PORTS && sizeof types / sizeof *types == PORTS &&
               sizeof defaults / sizeof *defaults == DS_COUNT, "Crush port count");

const LADSPA_Descriptor hs_distressor_descriptor = {
    .UniqueID = HS_DISTRESSOR_ID, .Label = "hs_distressor", .Name = "HS Crush",
    .Maker = "Henry Skillz", .Copyright = "Prototype",
    .Properties = LADSPA_PROPERTY_HARD_RT_CAPABLE,
    .PortCount = PORTS, .PortDescriptors = types, .PortNames = names, .PortRangeHints = hints,
    .instantiate = instantiate, .connect_port = connect, .activate = activate, .run = run, .cleanup = free
};
