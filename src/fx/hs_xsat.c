#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "ladspa.h"
#include "hs_xsat.h"
#include "hs_xsat_curves.h"

enum { IL, IR, OL, OR, FIRST, PORTS = FIRST + XS_COUNT };
struct xsat {
    float *port[PORTS];
    float values[XS_COUNT];
    float table[2][XS_STEPS + 1];
    float gain_in, gain_out, drive, mix, dc_coefficient;
    float previous[2], dc[2];
    int ready, bypass;
};

static const float defaults[] = {0, 0, 50, 50, 100, 0, 0, 0, 0};
#define B (LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE)
#define ZERO (B | LADSPA_HINT_DEFAULT_0)
#define MID (B | LADSPA_HINT_DEFAULT_MIDDLE)
#define MAX (B | LADSPA_HINT_DEFAULT_MAXIMUM)
#define TOGGLE (ZERO | LADSPA_HINT_TOGGLED)
static const LADSPA_PortRangeHint hints[PORTS] = {
    [FIRST+XS_DRIVE] = {ZERO,-12,24},
    [FIRST+XS_SHAPE] = {ZERO,-50,50},
    [FIRST+XS_DEPTH] = {MID,0,100},
    [FIRST+XS_HARMONICS] = {MID,0,100},
    [FIRST+XS_MIX] = {MAX,0,100},
    [FIRST+XS_HEADROOM] = {TOGGLE,0,1},
    [FIRST+XS_INPUT] = {ZERO,-30,0},
    [FIRST+XS_OUTPUT] = {ZERO,-30,0},
    [FIRST+XS_BYPASS] = {TOGGLE,0,1}
};
#undef B
#undef ZERO
#undef MID
#undef MAX
#undef TOGGLE

static LADSPA_Handle instantiate(const LADSPA_Descriptor *desc, unsigned long rate)
{
    (void)desc;
    if (!rate) return NULL;
    struct xsat *s = calloc(1, sizeof *s);
    if (s) s->dc_coefficient = -expm1f(-30.15928947f / rate);
    return s;
}

static void activate(LADSPA_Handle instance)
{
    struct xsat *s = instance;
    memset(s->previous, 0, sizeof s->previous);
    memset(s->dc, 0, sizeof s->dc);
    s->ready = 0;
}

static void connect(LADSPA_Handle instance, unsigned long port, LADSPA_Data *value)
{
    struct xsat *s = instance;
    if (port < PORTS) s->port[port] = value;
}

static void configure(struct xsat *s, const float *v)
{
    float d = (v[XS_DRIVE] + 12.0f)/6.0f;
    float sh = (v[XS_SHAPE] + 50.0f)/50.0f;
    float dep = v[XS_DEPTH]/50.0f;
    int i = d < 6 ? (int)d : 5, j = sh < 2 ? (int)sh : 1, k = dep < 2 ? (int)dep : 1;
    int head = v[XS_HEADROOM] >= .5f;
    float asym = 1.0f - .01f*v[XS_HARMONICS];
    asym *= asym;
    d -= i; sh -= j; dep -= k;
    memset(s->table, 0, sizeof s->table);
    for (int x = 0; x < 2; ++x)
        for (int y = 0; y < 2; ++y)
            for (int z = 0; z < 2; ++z) {
                const int16_t (*curves)[XS_STEPS+1] = xs_curves[i+x][j+y][k+z][head];
                float weight = (x ? d : 1-d) * (y ? sh : 1-sh) * (z ? dep : 1-dep) / 12000.0f;
                for (int n = 0; n <= XS_STEPS; ++n) {
                    s->table[0][n] += curves[0][n] * weight;
                    s->table[1][n] += curves[1][n] * weight * asym;
                }
            }
    s->drive = powf(10, v[XS_DRIVE]/20);
    s->gain_in = powf(10, v[XS_INPUT]/20);
    s->gain_out = powf(10, v[XS_OUTPUT]/20);
    s->mix = v[XS_MIX] * .01f;
    s->bypass = v[XS_BYPASS] >= .5f;
    memcpy(s->values, v, sizeof s->values);
    s->ready = 1;
}

static inline float shape(const struct xsat *s, float x)
{
    float v = fminf(fabsf(x) * (.5f * XS_STEPS), XS_STEPS);
    int i = (int)v;
    int j = i + (i < XS_STEPS);
    float f = v - i;
    float odd = s->table[0][i] + f * (s->table[0][j] - s->table[0][i]);
    float even = s->table[1][i] + f * (s->table[1][j] - s->table[1][i]);
    return (x < 0 ? -odd : odd) + even;
}

static void run(LADSPA_Handle instance, unsigned long frames)
{
    struct xsat *s = instance;
    if (!s->port[IL] || !s->port[IR] || !s->port[OL] || !s->port[OR]) return;
    float v[XS_COUNT];
    for (int i = 0; i < XS_COUNT; ++i) {
        float *p = s->port[FIRST+i];
        const LADSPA_PortRangeHint *h = &hints[FIRST+i];
        v[i] = p && isfinite(*p) ? fminf(h->UpperBound, fmaxf(h->LowerBound, *p)) : defaults[i];
    }
    if (!s->ready || memcmp(v, s->values, sizeof v)) configure(s, v);
    for (unsigned long i = 0; i < frames; ++i) {
        float in[] = {s->port[IL][i], s->port[IR][i]};
        for (int c = 0; c < 2; ++c) {
            float input = in[c]*s->gain_in;
            float d = input*s->drive;
            float now = shape(s, d);
            float wet = .5f*(now + shape(s, .5f*(d+s->previous[c])));
            s->previous[c] = d;
            s->dc[c] += (wet-s->dc[c])*s->dc_coefficient;
            if (!(fabsf(s->dc[c]) >= 1.e-24f && isfinite(s->dc[c]))) s->dc[c] = 0;
            s->port[OL+c][i] = s->bypass ? in[c]
                : s->gain_out*(input*(1-s->mix) + (wet-s->dc[c])*s->mix);
        }
    }
}

#define AI (LADSPA_PORT_INPUT | LADSPA_PORT_AUDIO)
#define AO (LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO)
#define C (LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL)
static const LADSPA_PortDescriptor types[] = {AI,AI,AO,AO,C,C,C,C,C,C,C,C,C};
static const char *const names[] = {
    "Input L", "Input R", "Output L", "Output R", "Drive (dB)", "Shape",
    "Depth (%)", "Harmonics (%)", "Dry/Wet Mix (%)", "+6dB Headroom",
    "Input Gain (dB)", "Output Gain (dB)", "Bypass"
};
_Static_assert(sizeof names / sizeof *names == PORTS &&
               sizeof types / sizeof *types == PORTS &&
               sizeof defaults / sizeof *defaults == XS_COUNT, "Drive Sat port count");

const LADSPA_Descriptor hs_xsat_descriptor = {
    .UniqueID = HS_XSAT_ID, .Label = "hs_drive_sat", .Name = "HS Drive Sat",
    .Maker = "Henry Skillz", .Copyright = "Prototype",
    .Properties = LADSPA_PROPERTY_HARD_RT_CAPABLE,
    .PortCount = PORTS, .PortDescriptors = types, .PortNames = names, .PortRangeHints = hints,
    .instantiate = instantiate, .connect_port = connect, .activate = activate, .run = run, .cleanup = free
};
