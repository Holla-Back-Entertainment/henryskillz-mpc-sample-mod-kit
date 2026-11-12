#include <ladspa.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "hs_channel.h"
#include "hs_channel_cal.h"

enum { LEFT_IN, RIGHT_IN, LEFT_OUT, RIGHT_OUT, FIRST_CONTROL };
enum { HP_1, HP_2, LP, LOW, LOW_MID, HIGH_MID, HIGH, DS_HP, DS_SHELF, FILTER_COUNT };

struct biquad {
    float b0, b1, b2, a1, a2;
    float z1[2], z2[2];
    int enabled;
};

struct channel {
    float rate;
    float *ports[FIRST_CONTROL + CH_COUNT];
    float parameters[CH_COUNT];
    struct biquad filters[FILTER_COUNT];
    float comp_env, gate_env, comp_reduction, gate_reduction, comp_g1, comp_g2, comp_charge;
    int gate_open, gate_hold_left, gate_hold;
    float gate_depth;
    float ds_env, ds_reduction, ds_attack, ds_release, ds_shelf_db;
    int ds_tick, ds_band;
    float input, drive, output;
    float comp_attack1, comp_attack2, comp_offset, comp_knee;
    float comp_charge_on, comp_charge_off, comp_release_dial_ms;
    float gate_step, gate_release, detector_decay;
    float comp_threshold, comp_floor, comp_slope, gate_threshold, gate_close;
    unsigned configured;
    struct hs_mod mod;
};

static const float defaults[CH_COUNT] = {
    [CH_HP] = 20, [CH_LP] = 16000,
    [CH_LF_HZ] = 100, [CH_LM_HZ] = 700, [CH_LM_Q] = 1,
    [CH_HM_HZ] = 2500, [CH_HM_Q] = 1, [CH_HF_HZ] = 8000,
    [CH_COMP_THRESHOLD] = 10, [CH_COMP_RATIO] = 1, [CH_COMP_RELEASE] = 632.4555f,
    [CH_GATE_THRESHOLD] = -30, [CH_GATE_RELEASE] = 632.4555f,
    [CH_DS_FREQ] = 6000, [CH_DS_THRESHOLD] = -25,
    [CH_MOD+HS_MOD_ATTACK] = 5.623f, [CH_MOD+HS_MOD_RELEASE] = 223.6f   /* HS_MOD_DEFAULTS */
};

static const char *const names[] = {
    "Input L", "Input R", "Output L", "Output R",
    "Input (dB)", "Drive (%)", "Output (dB)", "HP (Hz)", "LP (Hz)", "EQ First",
    "LF Hz", "LF dB", "LF Bell", "LM Hz", "LM dB", "LM Q",
    "HM Hz", "HM dB", "HM Q", "HF Hz", "HF dB", "HF Bell",
    "Comp Thresh", "Comp Ratio", "Make-Up dB", "Fast Attack", "Release ms",
    "Hard Knee", "Gate Thresh", "Gate Range", "Release ms", "Fast Attack", "Expand",
    "DS Freq Hz", "DS Thresh dB", "DS Range dB", "DS Mode", HS_MOD_NAMES
};

#define B (LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE)
#define L LADSPA_HINT_LOGARITHMIC
#define D0 LADSPA_HINT_DEFAULT_0
#define LO LADSPA_HINT_DEFAULT_MINIMUM
#define MID LADSPA_HINT_DEFAULT_MIDDLE
#define HI LADSPA_HINT_DEFAULT_MAXIMUM
#define PORT(index) [FIRST_CONTROL + index]
static const LADSPA_PortRangeHint hints[] = {
    PORT(CH_INPUT) = {B | MID, -18, 18},
    PORT(CH_DRIVE) = {B | D0, 0, 100},
    PORT(CH_OUTPUT) = {B | MID, -18, 18},
    PORT(CH_HP) = {B | L | LO, 20, 350},
    PORT(CH_LP) = {B | L | HI, 3000, 16000},
    PORT(CH_EQ_FIRST) = {B | LADSPA_HINT_TOGGLED | D0, 0, 1},
    PORT(CH_LF_HZ) = {B | L | MID, 30, 450},
    PORT(CH_LF_DB) = {B | MID, -18, 18},
    PORT(CH_LF_BELL) = {B | LADSPA_HINT_TOGGLED | D0, 0, 1},
    PORT(CH_LM_HZ) = {B | L | MID, 200, 2500},
    PORT(CH_LM_DB) = {B | MID, -18, 18},
    PORT(CH_LM_Q) = {B | L | MID, 0.5, 4},
    PORT(CH_HM_HZ) = {B | L | MID, 600, 7000},
    PORT(CH_HM_DB) = {B | MID, -18, 18},
    PORT(CH_HM_Q) = {B | L | MID, 0.5, 4},
    PORT(CH_HF_HZ) = {B | L | MID, 1500, 16000},
    PORT(CH_HF_DB) = {B | MID, -18, 18},
    PORT(CH_HF_BELL) = {B | LADSPA_HINT_TOGGLED | D0, 0, 1},
    PORT(CH_COMP_THRESHOLD) = {B | HI, -20, 10},
    PORT(CH_COMP_RATIO) = {B | L | LO, 1, 20},
    PORT(CH_MAKEUP) = {B | D0, 0, 18},
    PORT(CH_COMP_FAST) = {B | LADSPA_HINT_TOGGLED | D0, 0, 1},
    PORT(CH_COMP_RELEASE) = {B | L | MID, 100, 4000},
    PORT(CH_COMP_HARD) = {B | LADSPA_HINT_TOGGLED | D0, 0, 1},
    PORT(CH_GATE_THRESHOLD) = {B | LO, -30, 10},
    PORT(CH_GATE_RANGE) = {B | D0, 0, 40},
    PORT(CH_GATE_RELEASE) = {B | L | MID, 100, 4000},
    PORT(CH_GATE_FAST) = {B | LADSPA_HINT_TOGGLED | D0, 0, 1},
    PORT(CH_GATE_EXPAND) = {B | LADSPA_HINT_TOGGLED | D0, 0, 1},
    PORT(CH_DS_FREQ) = {B | L | MID, 3000, 12000},
    PORT(CH_DS_THRESHOLD) = {B | MID, -50, 0},
    PORT(CH_DS_RANGE) = {B | D0, 0, 18},
    PORT(CH_DS_MODE) = {B | D0, 0, 3},   /* thirds of the turn: split, wide, listen (hs-fx labels them) */
    HS_MOD_HINTS(FIRST_CONTROL + CH_MOD, CH_MOD)
};
#undef PORT
#undef B
#undef L
#undef D0
#undef LO
#undef MID
#undef HI

static float db_gain(float db) { return expf(db * 0.11512925465f); }

static float time_coefficient(float milliseconds, float rate)
{
    return -expm1f(-1000.0f / (milliseconds * rate));
}

enum shape { HIGH_PASS_ONE_POLE, HIGH_PASS, LOW_PASS, BELL, LOW_SHELF, HIGH_SHELF };

static void tune(struct biquad *f, enum shape shape, float hz, float db, float q, float rate, int enabled)
{
    if (!enabled) {
        memset(f, 0, sizeof *f);
        return;
    }
    float w = 6.28318530718f * fminf(hz, rate * 0.46f) / rate;
    float c = cosf(w), sine = sinf(w);
    float alpha = sine / (2.0f * q);
    float a = db_gain(db * 0.5f);
    float b0, b1, b2, a0, a1, a2;
    switch (shape) {
    case HIGH_PASS_ONE_POLE: {
        float k = tanf(w * 0.5f);
        b0 = 1; b1 = -1; b2 = 0;
        a0 = 1 + k; a1 = k - 1; a2 = 0;
        break;
    }
    case HIGH_PASS:
    case LOW_PASS:
        b0 = shape == HIGH_PASS ? (1 + c) * 0.5f : (1 - c) * 0.5f;
        b1 = (shape == HIGH_PASS ? -2 : 2) * b0;
        b2 = b0;
        a0 = 1 + alpha; a1 = -2 * c; a2 = 1 - alpha;
        break;
    case BELL:
        b0 = 1 + alpha * a; b1 = -2 * c; b2 = 1 - alpha * a;
        a0 = 1 + alpha / a; a1 = b1; a2 = 1 - alpha / a;
        break;
    default: {
        float sign = shape == LOW_SHELF ? 1.0f : -1.0f;
        float sum = a + 1, delta = a - 1;
        float tilt = 2 * sqrtf(a) * alpha;
        b0 = a * (sum - sign * delta * c + tilt);
        b1 = 2 * sign * a * (delta - sign * sum * c);
        b2 = a * (sum - sign * delta * c - tilt);
        a0 = sum + sign * delta * c + tilt;
        a1 = -2 * sign * (delta + sign * sum * c);
        a2 = sum + sign * delta * c - tilt;
        break;
    }}
    f->b0 = b0 / a0; f->b1 = b1 / a0; f->b2 = b2 / a0;
    f->a1 = a1 / a0; f->a2 = a2 / a0;
    f->enabled = 1;
}

static float filter(struct biquad *f, int channel, float x)
{
    if (!f->enabled) return x;
    float y = f->b0 * x + f->z1[channel];
    f->z1[channel] = f->b1 * x - f->a1 * y + f->z2[channel];
    f->z2[channel] = f->b2 * x - f->a2 * y;
    return y;
}

static float equalize(struct channel *s, int c, float x)
{
    for (int i = LOW; i <= HIGH; ++i) x = filter(&s->filters[i], c, x);
    return x;
}

static float preamp(float x, float drive)
{
    if (drive == 0 || x == 0) return x;
    /* Unity small-signal gain. Drive blends in a progressively stronger
     * asymmetric saturating stage, centered so digital silence stays silent. */
    float k = 1.0f + drive * 5.0f;
    float biased = k * x + 0.18f;
    float y = biased / (1.0f + fabsf(biased)) - 0.18f / 1.18f;
    return x + drive * (y * (1.18f * 1.18f / k) - x);
}

/* ---- Reference calibration lookups; tables in hs_channel_cal.h ---- */
#define LEN(a) ((int)(sizeof (a) / sizeof *(a)))
static float lerp(float a, float b, float t) { return a + (b - a) * t; }

static int locate(const float *x, int n, float v, float *t)
{
    if (v <= x[0]) { *t = 0; return 0; }
    if (v >= x[n - 1]) { *t = 1; return n - 2; }
    int i = 0;
    while (v > x[i + 1]) ++i;
    *t = (v - x[i]) / (x[i + 1] - x[i]);
    return i;
}

/* real centre = dial x ratio, interpolated over dial frequency and |gain| (6 / 15 dB rows) */
static float cal_fc(const float *x, const float *g6, const float *g15, int n, float hz, float db)
{
    float t, tg = fminf(fmaxf((fabsf(db) - 6) / 9, 0), 1);
    int i = locate(x, n, logf(hz), &t);
    return hz * lerp(lerp(g6[i], g6[i + 1], t), lerp(g15[i], g15[i + 1], t), tg);
}

/* table[f][gain] */
static float cal_2d(const float *fx, int nf, const float *table, float hz, float db)
{
    float tf, tg;
    int i = locate(fx, nf, logf(hz), &tf), j = locate(cal_gains, 3, fabsf(db), &tg);
    const float *a = table + i * 3, *b = table + (i + 1) * 3;
    return lerp(lerp(a[j], a[j + 1], tg), lerp(b[j], b[j + 1], tg), tf);
}

/* table[f][q][gain] */
static float cal_3d(const float *fx, int nf, const float *qx, const float *table, float hz, float q, float db)
{
    float tf, tq, tg, v[2][2];
    int i = locate(fx, nf, logf(hz), &tf), k = locate(qx, 3, logf(q), &tq), j = locate(cal_gains, 3, fabsf(db), &tg);
    for (int a = 0; a < 2; ++a)
        for (int b = 0; b < 2; ++b) {
            const float *r = table + ((i + a) * 3 + (k + b)) * 3;
            v[a][b] = lerp(r[j], r[j + 1], tg);
        }
    return lerp(lerp(v[0][0], v[0][1], tq), lerp(v[1][0], v[1][1], tq), tf);
}

#define TUNE_OUTER(f, shape, tag, hz, db, rate) \
    tune(f, shape, cal_fc(tag##_fc_x, tag##_fc_g6, tag##_fc_g15, LEN(tag##_fc_x), hz, db), \
         (db) * cal_2d(tag##_qg_f, LEN(tag##_qg_f), tag##_g_ratio, hz, db), \
         cal_2d(tag##_qg_f, LEN(tag##_qg_f), tag##_q, hz, db), rate, (db) != 0)
#define TUNE_MID(f, tag, hz, q, db, rate) \
    tune(f, BELL, cal_fc(tag##_fc_x, tag##_fc_g6, tag##_fc_g15, LEN(tag##_fc_x), hz, db), \
         (db) * cal_3d(tag##_qg_f, LEN(tag##_qg_f), tag##_qg_q, tag##_g_ratio, hz, q, db), \
         (q) * cal_3d(tag##_qg_f, LEN(tag##_qg_f), tag##_qg_q, tag##_q_ratio, hz, q, db), rate, (db) != 0)

static void configure(struct channel *s, const float *p)
{
    float rate = s->rate;
    /* The 242 high pass is third order. The 12 dB/octave low pass
     * remains second order. The outer bell bands use the documented Q 1.3. */
    int hp_on = p[CH_HP] > 20.01f;
    float t, hp = p[CH_HP], lp = p[CH_LP];
    int i = locate(hp_fc_x, LEN(hp_fc_x), logf(hp), &t);  /* real corner for this dial position */
    hp *= lerp(hp_fc_ratio[i], hp_fc_ratio[i + 1], t);
    i = locate(lp_fc_x, LEN(lp_fc_x), logf(lp), &t);
    lp *= lerp(lp_fc_ratio[i], lp_fc_ratio[i + 1], t);
    tune(&s->filters[HP_1], HIGH_PASS_ONE_POLE, hp, 0, 1, rate, hp_on);
    tune(&s->filters[HP_2], HIGH_PASS, hp, 0, 1, rate, hp_on);
    tune(&s->filters[LP], LOW_PASS, lp, 0, .7071f, rate, p[CH_LP] < 15999);
    /* Bands are placed where the reference plugin puts them for each dial position:
     * measured centre, width and gain, not the dial values (see hs_channel_cal.h). */
    if (p[CH_LF_BELL] >= .5f) TUNE_OUTER(&s->filters[LOW], BELL, lfb, p[CH_LF_HZ], p[CH_LF_DB], rate);
    else TUNE_OUTER(&s->filters[LOW], LOW_SHELF, lfs, p[CH_LF_HZ], p[CH_LF_DB], rate);
    TUNE_MID(&s->filters[LOW_MID], lmf, p[CH_LM_HZ], p[CH_LM_Q], p[CH_LM_DB], rate);
    TUNE_MID(&s->filters[HIGH_MID], hmf, p[CH_HM_HZ], p[CH_HM_Q], p[CH_HM_DB], rate);
    if (p[CH_HF_BELL] >= .5f) TUNE_OUTER(&s->filters[HIGH], BELL, hfb, p[CH_HF_HZ], p[CH_HF_DB], rate);
    else TUNE_OUTER(&s->filters[HIGH], HIGH_SHELF, hfs, p[CH_HF_HZ], p[CH_HF_DB], rate);
    s->input = db_gain(p[CH_INPUT]);
    s->drive = p[CH_DRIVE] / 100;
    s->output = db_gain(p[CH_OUTPUT] + p[CH_MAKEUP]);
    /* Static curve and timing fitted to the reference plugin: one wide quadratic knee per ratio
     * (ratio 20 = the reference infinity), threshold moving 0.83 dB per dial dB, and a
     * two-stage smoother with a program-dependent release (see dynamics()) blended COMP_BLEND : 1-COMP_BLEND. */
    float tr;
    int ri = locate(comp_ratio_x, LEN(comp_ratio_x), logf(fmaxf(p[CH_COMP_RATIO], 1)), &tr);
    int fast = p[CH_COMP_FAST] >= .5f;
    s->comp_slope = lerp(comp_slope[ri], comp_slope[ri + 1], tr);
    s->comp_knee = p[CH_COMP_HARD] >= .5f ? 0.5f : lerp(comp_knee_db[ri], comp_knee_db[ri + 1], tr);
    s->comp_threshold = lerp(comp_thresh_off[ri], comp_thresh_off[ri + 1], tr) - 20
        + COMP_THRESH_PER_DIAL * (p[CH_COMP_THRESHOLD] + 20);
    s->comp_offset = fast ? COMP_FAST_LEVEL_DB : 0;
    s->comp_floor = db_gain(s->comp_threshold - s->comp_offset - s->comp_knee * 0.5f);
    s->comp_attack1 = time_coefficient(fast ? COMP_FAST_ATTACK_MS : COMP_ATTACK1_MS, rate);
    s->comp_attack2 = time_coefficient(fast ? COMP_FAST_ATTACK_MS : COMP_ATTACK2_MS, rate);
    /* Program-dependent release: a charge (0..1) builds while compressing and drains when not;
     * it blends the release from COMP_RELEASE_FAST_MS (short hits) to the dial-scaled time (held). */
    s->comp_charge_on = time_coefficient(fmaxf(COMP_CHARGE_ON_MS + COMP_CHARGE_ON_PER_DIAL * p[CH_COMP_RELEASE], 50), rate);
    s->comp_charge_off = time_coefficient(COMP_CHARGE_OFF_MS, rate);
    s->comp_release_dial_ms = COMP_RELEASE_PER_DIAL * p[CH_COMP_RELEASE];
    i = locate(gate_open_x, LEN(gate_open_x), p[CH_GATE_THRESHOLD], &t);  /* real opening level per dial */
    s->gate_threshold = lerp(gate_open_db[i], gate_open_db[i + 1], t);
    float hysteresis = 2 + 6 * (10 - p[CH_GATE_THRESHOLD]) / 40;
    s->gate_close = s->gate_threshold - hysteresis;
    s->gate_step = (p[CH_GATE_FAST] >= .5f ? 400000 : 20000) / rate;
    i = locate(gate_rel_x, LEN(gate_rel_x), logf(p[CH_GATE_RELEASE]), &t);  /* real hold + fall per release dial */
    s->gate_hold = (int)(lerp(gate_hold_ms[i], gate_hold_ms[i + 1], t) * rate / 1000);
    s->gate_release = time_coefficient(lerp(gate_tau_ms[i], gate_tau_ms[i + 1], t), rate);
    s->gate_depth = GATE_DEPTH_PER_RANGE * p[CH_GATE_RANGE];
    tune(&s->filters[DS_HP], HIGH_PASS, p[CH_DS_FREQ], 0, .7071f, rate, 1);
    s->ds_attack = time_coefficient(.5f, rate);
    s->ds_release = time_coefficient(60, rate);
    if (p[CH_DS_RANGE] == 0) s->ds_reduction = 0;
    tune(&s->filters[DS_SHELF], HIGH_SHELF, p[CH_DS_FREQ], -s->ds_reduction, .7071f, rate, 1);
    s->ds_shelf_db = s->ds_reduction;
    if (s->comp_slope == 0) s->comp_reduction = s->comp_g1 = s->comp_g2 = 0;
    if (p[CH_GATE_RANGE] == 0) {
        s->gate_open = 0;
        s->gate_reduction = 0;
    }
    memcpy(s->parameters, p, sizeof s->parameters);
    s->configured = 1;
}

static float dynamics(struct channel *s, float peak)
{
    const float *p = s->parameters;
    if (s->comp_slope > 0) {
        s->comp_env = fmaxf(peak, s->comp_env * s->detector_decay);
        float target = 0;
        if (s->comp_env > s->comp_floor) {
            float over = 20 * log10f(s->comp_env) + s->comp_offset - s->comp_threshold;
            float half = s->comp_knee * 0.5f;  /* quadratic knee, comp_knee dB wide */
            target = s->comp_slope * (over >= half ? over : (over + half) * (over + half) / (2 * s->comp_knee));
        }
        float on = target > 0.5f;
        s->comp_charge += (on - s->comp_charge) * (on > s->comp_charge ? s->comp_charge_on : s->comp_charge_off);
        if (target > s->comp_g1 && target > s->comp_g2) {
            s->comp_g1 += (target - s->comp_g1) * s->comp_attack1;
            s->comp_g2 += (target - s->comp_g2) * s->comp_attack2;
        } else {  /* ponytail: per-sample powf/expm1f while releasing; table it if the Sample's CPU complains */
            float release_ms = fmaxf(COMP_RELEASE_FAST_MS + (s->comp_release_dial_ms - COMP_RELEASE_FAST_MS)
                                     * powf(s->comp_charge, COMP_CHARGE_EXP), 1);
            s->comp_g1 += (target - s->comp_g1)
                * (target > s->comp_g1 ? s->comp_attack1 : time_coefficient(release_ms, s->rate));
            s->comp_g2 += (target - s->comp_g2)
                * (target > s->comp_g2 ? s->comp_attack2 : time_coefficient(release_ms * COMP_RELEASE2_X, s->rate));
        }
        s->comp_reduction = COMP_BLEND * s->comp_g1 + (1 - COMP_BLEND) * s->comp_g2;
    }
    if (p[CH_GATE_RANGE] > 0) {
        s->gate_env = fmaxf(peak, s->gate_env * s->detector_decay);
        float level = 20 * log10f(fmaxf(s->gate_env, 1e-12f));
        s->gate_open = level >= (s->gate_open ? s->gate_close : s->gate_threshold);
        if (s->gate_open) s->gate_hold_left = s->gate_hold;
        else if (s->gate_hold_left > 0) --s->gate_hold_left;
        float target = 0;
        if (!s->gate_open && s->gate_hold_left == 0)
            target = p[CH_GATE_EXPAND] >= .5f
                ? fminf(s->gate_depth, fmaxf(s->gate_threshold - level, 0))
                : s->gate_depth;
        if (target <= s->gate_reduction)
            s->gate_reduction = fmaxf(target, s->gate_reduction - s->gate_step);
        else
            s->gate_reduction += (target - s->gate_reduction) * s->gate_release;
    }
    return db_gain(-(s->comp_reduction + s->gate_reduction));
}

/* De-esser: a 12 dB/oct high-pass at DS Freq feeds a peak detector; above DS Thresh it turns down
 * at 6:1, never more than DS Range. Split pulls a high shelf at DS Freq down by that much (retuned every
 * 16 samples), Wide turns down everything. Listen plays the detector band. Mode picks one of the three. */
static void deess(struct channel *s, const float *p, float *left, float *right)
{
    float hl = filter(&s->filters[DS_HP], 0, *left), hr = filter(&s->filters[DS_HP], 1, *right);
    if (p[CH_DS_MODE] >= 2) { *left = hl; *right = hr; return; }
    s->ds_env = fmaxf(fmaxf(fabsf(hl), fabsf(hr)), s->ds_env * s->detector_decay);
    float over = s->ds_env > 1e-9f ? 20 * log10f(s->ds_env) - p[CH_DS_THRESHOLD] : 0;
    float target = fminf(p[CH_DS_RANGE], fmaxf(over, 0) * (5.0f / 6));
    s->ds_reduction += (target - s->ds_reduction) * (target > s->ds_reduction ? s->ds_attack : s->ds_release);
    if (p[CH_DS_MODE] >= 1) {
        float g = db_gain(-s->ds_reduction);
        *left *= g; *right *= g;
        return;
    }
    if (++s->ds_tick >= 16) {
        s->ds_tick = 0;
        if (fabsf(s->ds_reduction - s->ds_shelf_db) > .02f) {
            tune(&s->filters[DS_SHELF], HIGH_SHELF, p[CH_DS_FREQ], -s->ds_reduction, .7071f, s->rate, 1);
            s->ds_shelf_db = s->ds_reduction;
        }
    }
    *left = filter(&s->filters[DS_SHELF], 0, *left);
    *right = filter(&s->filters[DS_SHELF], 1, *right);
}

static LADSPA_Handle instantiate(const LADSPA_Descriptor *descriptor, unsigned long rate)
{
    (void)descriptor;
    if (rate < 8000 || rate > 384000) return NULL;
    struct channel *s = calloc(1, sizeof *s);
    if (s) {
        s->rate = rate;
        s->detector_decay = 1 - time_coefficient(6, s->rate);
    }
    return s;
}

static void connect_port(LADSPA_Handle handle, unsigned long port, LADSPA_Data *data)
{
    struct channel *s = handle;
    if (port < FIRST_CONTROL + CH_COUNT) s->ports[port] = data;
}

static void activate(LADSPA_Handle handle)
{
    struct channel *s = handle;
    memset(s->filters, 0, sizeof s->filters);
    s->comp_env = s->gate_env = 0;
    s->comp_reduction = s->gate_reduction = s->comp_g1 = s->comp_g2 = s->comp_charge = 0;
    s->gate_open = s->gate_hold_left = 0;
    s->ds_env = s->ds_reduction = 0;
    s->configured = 0;
}

static void run(LADSPA_Handle handle, unsigned long frames)
{
    struct channel *s = handle;
    float p[CH_COUNT];
    for (unsigned i = 0; i < CH_COUNT; ++i) {
        const float *control = s->ports[FIRST_CONTROL + i];
        const LADSPA_PortRangeHint *hint = &hints[FIRST_CONTROL + i];
        p[i] = control && isfinite(*control)
            ? fminf(hint->UpperBound, fmaxf(hint->LowerBound, *control)) : defaults[i];
    }
    hs_mod_apply(&s->mod, p, CH_MOD, hints + FIRST_CONTROL, frames, (unsigned long)s->rate);
    if (!s->configured || memcmp(p, s->parameters, sizeof p)) configure(s, p);
    if (!s->ports[LEFT_IN] || !s->ports[RIGHT_IN] || !s->ports[LEFT_OUT] || !s->ports[RIGHT_OUT]) return;

    int eq_first = p[CH_EQ_FIRST] >= .5f;
    int dynamics_on = p[CH_COMP_RATIO] > 1.0001f || p[CH_GATE_RANGE] > 0;
    int deess_on = p[CH_DS_RANGE] > 0 || p[CH_DS_MODE] >= 2;
    int ds_band = deess_on ? (int)fminf(p[CH_DS_MODE], 2) : -1;
    if (ds_band != s->ds_band) {          /* off, or a new mode: start clean, no stale filter tail or envelope */
        s->ds_band = ds_band;
        for (int f = DS_HP; f <= DS_SHELF; ++f) {
            memset(s->filters[f].z1, 0, sizeof s->filters[f].z1);
            memset(s->filters[f].z2, 0, sizeof s->filters[f].z2);
        }
        s->ds_env = s->ds_reduction = s->ds_shelf_db = 0;
        tune(&s->filters[DS_SHELF], HIGH_SHELF, p[CH_DS_FREQ], 0, .7071f, s->rate, 1);
    }
    for (unsigned long i = 0; i < frames; ++i) {
        float left = preamp(s->ports[LEFT_IN][i] * s->input, s->drive);
        float right = preamp(s->ports[RIGHT_IN][i] * s->input, s->drive);
        for (int i = HP_1; i <= LP; ++i) {
            left = filter(&s->filters[i], 0, left);
            right = filter(&s->filters[i], 1, right);
        }
        if (eq_first) { left = equalize(s, 0, left); right = equalize(s, 1, right); }
        if (dynamics_on) {
            float gain = dynamics(s, fmaxf(fabsf(left), fabsf(right)));
            left *= gain;
            right *= gain;
        }
        if (!eq_first) { left = equalize(s, 0, left); right = equalize(s, 1, right); }
        if (deess_on) deess(s, p, &left, &right);
        s->ports[LEFT_OUT][i] = left * s->output;
        s->ports[RIGHT_OUT][i] = right * s->output;
    }
    /* Drop inaudible filter and detector residue once per block. */
    if (!(s->comp_env >= 1e-20f && isfinite(s->comp_env))) s->comp_env = 0;
    if (!(s->gate_env >= 1e-20f && isfinite(s->gate_env))) s->gate_env = 0;
    if (!(s->comp_charge >= 1e-20f && isfinite(s->comp_charge))) s->comp_charge = 0;
    if (!(s->comp_reduction >= 1e-20f && isfinite(s->comp_reduction))) s->comp_reduction = s->comp_g1 = s->comp_g2 = 0;
    if (!(s->gate_reduction >= 1e-20f && isfinite(s->gate_reduction))) s->gate_reduction = 0;
    if (!(s->ds_env >= 1e-20f && isfinite(s->ds_env))) s->ds_env = 0;
    if (!(s->ds_reduction >= 1e-20f && isfinite(s->ds_reduction))) s->ds_reduction = 0;
    for (int i = 0; i < FILTER_COUNT; ++i)
        for (int c = 0; c < 2; ++c) {
            if (!(fabsf(s->filters[i].z1[c]) >= 1e-20f && isfinite(s->filters[i].z1[c]))) s->filters[i].z1[c] = 0;
            if (!(fabsf(s->filters[i].z2[c]) >= 1e-20f && isfinite(s->filters[i].z2[c]))) s->filters[i].z2[c] = 0;
        }
}

#define AUDIO_IN (LADSPA_PORT_INPUT | LADSPA_PORT_AUDIO)
#define AUDIO_OUT (LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO)
#define CONTROL (LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL)
static const LADSPA_PortDescriptor descriptors[] = {
    AUDIO_IN, AUDIO_IN, AUDIO_OUT, AUDIO_OUT,
    CONTROL, CONTROL, CONTROL, CONTROL, CONTROL, CONTROL,
    CONTROL, CONTROL, CONTROL, CONTROL, CONTROL, CONTROL,
    CONTROL, CONTROL, CONTROL, CONTROL, CONTROL, CONTROL,
    CONTROL, CONTROL, CONTROL, CONTROL, CONTROL, CONTROL,
    CONTROL, CONTROL, CONTROL, CONTROL, CONTROL,
    CONTROL, CONTROL, CONTROL, CONTROL,
    HS_MOD_TYPES
};
_Static_assert(sizeof names / sizeof *names == FIRST_CONTROL + CH_COUNT, "parameter names");
_Static_assert(sizeof descriptors / sizeof *descriptors == FIRST_CONTROL + CH_COUNT, "port descriptors");

LADSPA_Descriptor hs_channel_descriptor = {
    .UniqueID = HS_CHANNEL_ID, .Label = "hs_channel_strip", .Name = "HS Channel Strip",
    .Maker = "Henry Skillz", .Copyright = "Original DSP",
    .Properties = LADSPA_PROPERTY_HARD_RT_CAPABLE,
    .PortCount = FIRST_CONTROL + CH_COUNT, .PortDescriptors = descriptors,
    .PortNames = names, .PortRangeHints = hints,
    .instantiate = instantiate, .connect_port = connect_port,
    .activate = activate, .run = run, .cleanup = free
};
