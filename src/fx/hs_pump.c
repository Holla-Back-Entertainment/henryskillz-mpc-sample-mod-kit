/* HS Pump: a stepped feedback compressor with an Opto release, attack modifier and output saturation.
 * DESIGN: ratio list incl. 1:1; attack and release steps; release
 *   step 8 is Opto with a program-dependent tail; feedback detector taken before the Sat stage;
 *   110 Hz detector HP; 12:1/20:1 and 4:1/6:1 sit at higher thresholds than 8:1/10:1; .5 s and Opto
 *   lower the threshold; 2:1/3:1 have very long knees; AtMod slows the first ms of attack and at high
 *   settings limits total gain reduction; Sat is fixed, after the VCA, thickens lows, softens highs,
 *   and works harder on 4:1/6:1 than on 8:1/10:1.
 * GUESSED (marked "guess", replace with a real unit's measurements): every dB value, knee width,
 *   opto time constant, AtMod law and the Sat curve. */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "ladspa.h"
#include "hs_pump.h"
#include "hs_env.h"

enum { IL, IR, OL, OR, FIRST, PORTS = FIRST + PU_COUNT, RATIOS = 9, STEPS = 8, OPTO = 7 };

static const float ratio_value[RATIOS] = {1, 2, 3, 4, 6, 8, 10, 12, 20};
static const float knee_db[RATIOS]     = {0, 20, 16, 10, 8, 6, 6, 4, 3};        /* guess, ordered as published */
static const float threshold_db[RATIOS] = {0, -20, -20, -16, -16, -18, -18, -14, -14}; /* guess, ordered as published */
static const float sat_drive_db[RATIOS] = {3, 3, 3, 6, 6, 0, 0, 3, 3};        /* guess, ordered as published */
static const float attack_ms[STEPS]  = {.2f, 1, 3, 6, 10, 20, 30, 40};
static const float release_ms[STEPS] = {50, 100, 150, 200, 250, 300, 500, 300};  /* last = Opto fast stage */
static const float LONG_RELEASE_THRESHOLD_DB = -2;   /* guess: .5 s and Opto lower the threshold */
static const float OPTO_TAIL_DB = 4, OPTO_CHARGE_MS = 600, OPTO_TAIL_MS = 2500;  /* guess */

struct pump {
    float *port[PORTS];
    unsigned long rate;
    float values[PU_COUNT];
    float in_gain, out_gain, mix, slope, knee, threshold, attack, release, onset_rate, gr_cap;
    float sat_in, sat_out, opto_charge, opto_tail, sat_lp, dc_coef;
    float hb0, hb1, hb2, ha1, ha2, z1[2], z2[2];
    float gr, tail, onset, lp[2], dc_x[2], dc_y[2];
    int ready, ratio, opto, atmod, det_hp, sat, bypass, key, send;
    struct hs_mod mod;
};

static const float defaults[] = {0, 3, 3, 3, 0, 0, 0, 0, 100, 0, 0, 0, HS_MOD_DEFAULTS};
#define B (LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE)
#define TOG (B | LADSPA_HINT_TOGGLED | LADSPA_HINT_DEFAULT_0)
#define STEP (B | LADSPA_HINT_INTEGER | LADSPA_HINT_DEFAULT_MIDDLE)
static const LADSPA_PortRangeHint hints[PORTS] = {
    [FIRST+PU_INPUT] = {B|LADSPA_HINT_DEFAULT_0,0,30},
    [FIRST+PU_RATIO] = {STEP,0,RATIOS-1},
    [FIRST+PU_ATTACK] = {STEP,0,STEPS-1},
    [FIRST+PU_RELEASE] = {STEP,0,STEPS-1},
    [FIRST+PU_ATMOD] = {B|LADSPA_HINT_DEFAULT_0,0,10},
    [FIRST+PU_DET_HP] = {TOG,0,1},
    [FIRST+PU_SAT] = {TOG,0,1},
    [FIRST+PU_OUTPUT] = {B|LADSPA_HINT_DEFAULT_0,-20,20},
    [FIRST+PU_MIX] = {B|LADSPA_HINT_DEFAULT_MAXIMUM,0,100},
    [FIRST+PU_BYPASS] = {TOG,0,1},
    [FIRST+PU_KEY] = {B|LADSPA_HINT_INTEGER|LADSPA_HINT_DEFAULT_0,0,HS_ENV_BUSES},
    [FIRST+PU_SEND] = {B|LADSPA_HINT_INTEGER|LADSPA_HINT_DEFAULT_0,0,HS_ENV_BUSES},
    HS_MOD_HINTS(FIRST+PU_MOD, PU_MOD)
};
#undef B
#undef TOG
#undef STEP

static inline float coef(unsigned long rate, float ms) { return -expm1f(-1000.0f/(rate*ms)); }

static LADSPA_Handle instantiate(const LADSPA_Descriptor *d, unsigned long rate)
{
    (void)d;
    if (!rate) return NULL;
    struct pump *s = calloc(1, sizeof *s);
    if (s) {
        s->rate = rate;
        s->opto_charge = coef(rate, OPTO_CHARGE_MS);
        s->opto_tail = coef(rate, OPTO_TAIL_MS);
        s->sat_lp = coef(rate, 1000.0f/(6.2831853f*9000));   /* guess: Sat softens the top above ~9 kHz */
        s->dc_coef = 1 - 6.2831853f*5/rate;
        /* 110 Hz, 12 dB/oct high-pass, detector only (slope not published) */
        float w = 6.28318530718f*110/rate, c = cosf(w), a = .70710678f*sinf(w), g = 1/(1+a);
        s->hb0 = (1+c)*.5f*g; s->hb1 = -2*s->hb0; s->hb2 = s->hb0;
        s->ha1 = -2*c*g; s->ha2 = (1-a)*g;
    }
    return s;
}

static void activate(LADSPA_Handle h)
{
    struct pump *s = h;
    s->ready = 0;
    s->gr = s->tail = s->onset = 0;
    for (int c = 0; c < 2; ++c) s->z1[c] = s->z2[c] = s->lp[c] = s->dc_x[c] = s->dc_y[c] = 0;
}

static void connect(LADSPA_Handle h, unsigned long port, LADSPA_Data *p)
{
    struct pump *s = h;
    if (port < PORTS) s->port[port] = p;
}

static void configure(struct pump *s, const float *v)
{
    int r = (int)lroundf(v[PU_RATIO]), a = (int)lroundf(v[PU_ATTACK]), rel = (int)lroundf(v[PU_RELEASE]);
    float R = ratio_value[r];
    s->ratio = r;
    s->slope = R - 1;                       /* feedback: GR = (R-1) x overshoot of the output */
    s->knee = knee_db[r];
    s->threshold = threshold_db[r] + (rel >= STEPS-2 ? LONG_RELEASE_THRESHOLD_DB : 0);
    /* A one-pole inside a feedback loop of gain R rings past a*R = 2; keep it well under. */
    float limit = r ? 1.5f/R : 1;
    s->attack = fminf(coef(s->rate, attack_ms[a]), limit);
    s->release = fminf(coef(s->rate, release_ms[rel]), limit);
    s->opto = rel == OPTO;
    /* AtMod (guess): a second attack stage up to 4x slower, and a cap on total reduction. */
    float am = v[PU_ATMOD]*.1f;
    s->atmod = am > 0;
    s->onset_rate = coef(s->rate, attack_ms[a]*(1 + 3*am) + 1.5f*am);
    s->gr_cap = am > 0 ? 40 - 22*am : 1e9f;
    s->sat_in = powf(10, sat_drive_db[r]*.05f);
    s->sat_out = 1/s->sat_in;
    s->in_gain = powf(10, v[PU_INPUT]*.05f);
    s->out_gain = powf(10, v[PU_OUTPUT]*.05f);
    s->mix = v[PU_MIX]*.01f;
    s->det_hp = v[PU_DET_HP] >= .5f;
    s->sat = v[PU_SAT] >= .5f;
    s->bypass = v[PU_BYPASS] >= .5f;
    s->key = (int)lroundf(v[PU_KEY]);
    s->send = (int)lroundf(v[PU_SEND]);
    memcpy(s->values, v, sizeof s->values);
    s->ready = 1;
}

static inline float computer(const struct pump *s, float level)
{
    float over = level - s->threshold, half = .5f*s->knee;
    if (over <= -half) return 0;
    if (over >= half) return s->slope*over;
    float t = over + half;
    return s->slope*t*t/(2*s->knee);
}

/* Germanium-style Sat (guess): soft above -6 dBFS, slightly asymmetric for 2nd harmonic. */
static inline float sat_curve(float x)
{
    x += .08f*x*x;
    float a = fabsf(x);
    return a <= .5f ? x : copysignf(.5f + .5f*tanhf((a-.5f)*2), x);
}

static inline float saturate(struct pump *s, float x, int c)
{
    float y = sat_curve(x*s->sat_in)*s->sat_out;
    s->lp[c] += s->sat_lp*(y - s->lp[c]);
    float out = s->lp[c] - s->dc_x[c] + s->dc_coef*s->dc_y[c];   /* remove the asymmetry's DC */
    s->dc_x[c] = s->lp[c]; s->dc_y[c] = out;
    return out;
}

static void run(LADSPA_Handle h, unsigned long frames)
{
    struct pump *s = h;
    if (!s->port[IL] || !s->port[IR] || !s->port[OL] || !s->port[OR]) return;
    float v[PU_COUNT];
    for (int i = 0; i < PU_COUNT; ++i) {
        float *p = s->port[FIRST+i];
        const LADSPA_PortRangeHint *r = &hints[FIRST+i];
        v[i] = p && isfinite(*p) ? fminf(r->UpperBound, fmaxf(r->LowerBound, *p)) : defaults[i];
    }
    hs_mod_apply(&s->mod, v, PU_MOD, hints + FIRST, frames, s->rate);
    if (!s->ready || memcmp(v, s->values, sizeof v)) configure(s, v);

    float gr = s->gr, tail = s->tail, onset = s->onset, sent = 0;
    /* Key In from another track: a block-old level (a few ms), fed forward since the loop can't hear it */
    float key = s->key ? hs_env_read(s->key)*s->in_gain : 0;
    float key_want = s->key && key > 1e-6f ? computer(s, 20*log10f(key))/ratio_value[s->ratio] : 0;
    for (unsigned long i = 0; i < frames; ++i) {
        float dry_l = s->port[IL][i], dry_r = s->port[IR][i];
        sent = fmaxf(sent, fmaxf(fabsf(dry_l), fabsf(dry_r)));
        if (s->bypass) { s->port[OL][i] = dry_l; s->port[OR][i] = dry_r; continue; }
        float l = dry_l*s->in_gain, r = dry_r*s->in_gain;
        float applied = s->atmod ? onset : gr + tail;
        float vca = exp2f(-.166096405f*applied);
        l *= vca; r *= vca;
        if (s->ratio && s->key) {
            tail = 0;
            gr += (key_want > gr ? s->attack : s->release)*(key_want - gr);
        } else if (s->ratio) {
            /* feedback detector: it hears the VCA output, before Sat */
            float dl = l, dr = r;
            if (s->det_hp) {
                dl = s->hb0*l + s->z1[0]; s->z1[0] = s->hb1*l - s->ha1*dl + s->z2[0]; s->z2[0] = s->hb2*l - s->ha2*dl;
                dr = s->hb0*r + s->z1[1]; s->z1[1] = s->hb1*r - s->ha1*dr + s->z2[1]; s->z2[1] = s->hb2*r - s->ha2*dr;
            }
            float peak = fmaxf(fabsf(dl), fabsf(dr));
            /* the loop sees the gain it set, so add it back to find what the input is asking for */
            float want = peak > 1e-6f ? computer(s, 20*log10f(peak) + (gr + tail - applied)) : 0;
            float need = want;   /* feedback: settles where gr = (R-1)/R x input overshoot */
            if (s->opto) {
                /* Opto: the last few dB hang on a slow, program-dependent tail */
                float tail_want = fminf(OPTO_TAIL_DB, need);
                tail += (tail_want > tail ? s->opto_charge : s->opto_tail)*(tail_want - tail);
                float fast_want = fmaxf(0, need - tail);
                gr += (fast_want > gr ? s->attack : s->release)*(fast_want - gr);
            } else {
                tail = 0;
                float target = fmaxf(0, need);
                gr += (target > gr ? s->attack : s->release)*(target - gr);
            }
            if (gr + tail > s->gr_cap) gr = fmaxf(0, s->gr_cap - tail);
        } else {
            gr = tail = 0;
        }
        float total = gr + tail;
        onset = total > onset ? onset + s->onset_rate*(total - onset) : total;
        if (s->sat) { l = saturate(s, l, 0); r = saturate(s, r, 1); }
        l *= s->out_gain; r *= s->out_gain;
        s->port[OL][i] = dry_l + s->mix*(l - dry_l);
        s->port[OR][i] = dry_r + s->mix*(r - dry_r);
    }
    if (s->send && !s->bypass) hs_env_send(s->send, sent);
    /* flush denormals; recover from a NaN sample */
    s->gr = isfinite(gr) && gr > 1e-9f ? gr : 0;
    s->tail = isfinite(tail) && tail > 1e-9f ? tail : 0;
    s->onset = isfinite(onset) && onset > 1e-9f ? onset : 0;
    for (int c = 0; c < 2; ++c) {
        float *st[] = {&s->z1[c], &s->z2[c], &s->lp[c], &s->dc_x[c], &s->dc_y[c]};
        for (int k = 0; k < 5; ++k) if (!(fabsf(*st[k]) >= 1e-24f && isfinite(*st[k]))) *st[k] = 0;
    }
}

#define AI (LADSPA_PORT_INPUT | LADSPA_PORT_AUDIO)
#define AO (LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO)
#define C (LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL)
static const LADSPA_PortDescriptor types[] = {AI,AI,AO,AO,C,C,C,C,C,C,C,C,C,C,C,C,HS_MOD_TYPES};
static const char *const names[] = {
    "Input L", "Input R", "Output L", "Output R", "Input (dB)",
    "Ratio (1,2,3,4,6,8,10,12,20)", "Attack (.2,1,3,6,10,20,30,40 ms)",
    "Release (.05,.1,.15,.2,.25,.3,.5 s, Opto)", "AtMod", "Detector HP 110 Hz", "Sat",
    "Output (dB)", "Mix (%)", "Bypass", "Key In (Own,A,B,C,D)", "Key Send (Off,A,B,C,D)", HS_MOD_NAMES
};
_Static_assert(sizeof names / sizeof *names == PORTS && sizeof types / sizeof *types == PORTS &&
               sizeof defaults / sizeof *defaults == PU_COUNT, "Pump port count");

LADSPA_Descriptor hs_pump_descriptor = {
    .UniqueID = HS_PUMP_ID, .Label = "HS Pump", .Name = "HS Pump",
    .Maker = "SampQOL Lab", .Copyright = "Prototype",
    .Properties = LADSPA_PROPERTY_HARD_RT_CAPABLE,
    .PortCount = PORTS, .PortDescriptors = types, .PortNames = names, .PortRangeHints = hints,
    .instantiate = instantiate, .connect_port = connect, .activate = activate, .run = run, .cleanup = free
};
