/* The MPC faceplates as a CLAP editor: draws the same panels hs-fx draws on the MPC Sample (src/fx/hs_fx.c,
 * included whole), into a 320x200 buffer: a 40-row header with page (and Para EQ band) buttons over the
 * 320x160 panel. On the MPC the panel copies knob names and values from SampQOL's screen and asks hs-flavor
 * which page is open (dlsym); here a fake "stock" screen is drawn with the same text, and those lookups are
 * answered from this file. Page tables are the MPC's own (src/fx/bank_tables.inc).
 * ponytail: drawing is main-thread only and the panel statics (lamps, notes) are shared by all instances
 * of a plugin; the last-drawn instance wins, as on the MPC where one editor is open at a time. */
#include <dlfcn.h>
#include <stdint.h>
static void *hs_gui_sym(const char *name);
#define dlsym(handle, name) hs_gui_sym(name)
#include "../fx/hs_fx.c"
#undef dlsym
#define constexpr const
typedef struct FlavorBank FlavorBank;
#include "../fx/bank_tables.inc"
#undef constexpr
#include "../fx/ladspa.h"
#include "hs_clap_panel.h"

int hs_eq2_response_of(const float *v, unsigned long rate, int which, int points, float *db);
float hs_distressor_gr(const void *handle);
float hs_tube_strip_gr(const void *handle);

enum { HEAD = 40, MAX_PAGES = 32 };

struct hs_panel {
    const LADSPA_Descriptor *d;
    float *ctl;                       /* indexed by LADSPA port */
    const uint32_t *param_port;       /* control-input ordinal -> port */
    uint32_t nparams;
    enum effect kind;
    const FlavorBank *tab; int ntab;  /* NULL: one page, the controls in port order */
    int page, band;                   /* band: Para EQ only, 0 = global */
    struct effect_page ep;
    uint32_t out[W * (HEAD + H)];
};

/* one visible page: a bank, and which half of a six-control bank (the MPC's SHIFT row) */
struct view { const FlavorBank *b; int half; };

static struct hs_panel *cur;          /* the panel being drawn, for the hook answers below */

static const FlavorBank *eq_tab(int band, int *n)
{
    *n = band ? (int)(sizeof eq_band / sizeof *eq_band) : (int)(sizeof eq_global / sizeof *eq_global);
    return band ? eq_band : eq_global;
}

static int views(const struct hs_panel *s, struct view *v)
{
    int ntab = s->ntab;
    const FlavorBank *tab = s->kind == HS_EQ2 ? eq_tab(s->band, &ntab) : s->tab;
    if (!tab) { v[0] = (struct view){NULL, 0}; return 1; }
    int n = 0;
    for (int i = 0; i < ntab && n < MAX_PAGES - 1; ++i) {
        v[n++] = (struct view){&tab[i], 0};
        if (tab[i].parameters[3] >= 0 || tab[i].parameters[4] >= 0 || tab[i].parameters[5] >= 0)
            v[n++] = (struct view){&tab[i], 1};
    }
    return n;
}

static struct view current(const struct hs_panel *s, int *count)
{
    struct view v[MAX_PAGES];
    int n = views(s, v);
    if (count) *count = n;
    return v[s->page < n ? s->page : 0];
}

/* control ordinal shown on knob i of the open page, or -1 */
static int knob_param(const struct hs_panel *s, int i)
{
    struct view v = current(s, NULL);
    if (!v.b) return i < (int)s->nparams ? i : -1;
    int k = v.b->parameters[v.half * 3 + i];
    if (k >= 0 && s->kind == HS_EQ2 && s->band) k += EQ_FIRST + (s->band - 1) * EB_COUNT;
    return k < (int)s->nparams ? k : -1;
}

static void port_range(const struct hs_panel *s, unsigned long port, float *lo, float *hi, int *lg, int *step)
{
    const LADSPA_PortRangeHint *r = &s->d->PortRangeHints[port];
    int f = r->HintDescriptor;
    *lo = LADSPA_IS_HINT_BOUNDED_BELOW(f) ? r->LowerBound : 0;
    *hi = LADSPA_IS_HINT_BOUNDED_ABOVE(f) ? r->UpperBound : 1;
    if (LADSPA_IS_HINT_SAMPLE_RATE(f)) { *lo *= 48000; *hi *= 48000; }
    if (LADSPA_IS_HINT_TOGGLED(f)) { *lo = 0; *hi = 1; }
    *lg = LADSPA_IS_HINT_LOGARITHMIC(f) && *lo > 0 && *hi > *lo;
    *step = LADSPA_IS_HINT_INTEGER(f) || LADSPA_IS_HINT_TOGGLED(f);
}

/* value <-> the MPC's 0..73 knob position */
static float to_pos(const struct hs_panel *s, unsigned long port, float v)
{
    float lo, hi; int lg, step;
    port_range(s, port, &lo, &hi, &lg, &step);
    if (hi <= lo) return 0;
    float t = lg ? logf(v / lo) / logf(hi / lo) : (v - lo) / (hi - lo);
    return 73 * (t < 0 ? 0 : t > 1 ? 1 : t);
}
static float from_pos(const struct hs_panel *s, unsigned long port, float pos)
{
    float lo, hi; int lg, step;
    port_range(s, port, &lo, &hi, &lg, &step);
    float t = pos < 0 ? 0 : pos > 73 ? 1 : pos / 73;
    float v = lg ? lo * powf(hi / lo, t) : lo + (hi - lo) * t;
    return step ? roundf(v) : v;
}

static void label_text(uint32_t *p, int x, int y, const char *s, uint32_t color)
{
    if (8 * (int)strlen(s) - 2 <= 94) preset_text(p, x, y, s, color);   /* the MPC's text height, near enough */
    else text(p, x, y + 3, s, 30, color);
}

/* The screen SampQOL would have drawn under our overlay: title, then name and value per knob row. */
static void fake_stock(struct hs_panel *s)
{
    uint32_t *p = s->ep.stock;
    memset(p, 0, sizeof s->ep.stock);
    char title[40];
    snprintf(title, sizeof title, "%s", s->d->Name);
    preset_text(p, 12, 11, title, 0xe8782a);       /* orange: what stock_name_display picks up */
    static const int rows[] = {57, 73, 89};
    struct view v = current(s, NULL);
    for (int i = 0; i < KNOBS; ++i) {
        int k = knob_param(s, i);
        if (k < 0) continue;
        unsigned long port = s->param_port[k];
        char name[40];
        if (v.b) snprintf(name, sizeof name, "%s", v.b->labels[v.half * 3 + i]);
        else {
            snprintf(name, sizeof name, "%s", s->d->PortNames[port]);
            char *cut = strstr(name, " (");
            if (cut) *cut = 0;
        }
        label_text(p, 52, rows[i] + 1, name, 0xe8ecec);
        float lo, hi; int lg, step;
        port_range(s, port, &lo, &hi, &lg, &step);
        float x = s->ctl[port];
        char val[24];
        if (LADSPA_IS_HINT_TOGGLED(s->d->PortRangeHints[port].HintDescriptor)) snprintf(val, sizeof val, x >= .5f ? "ON" : "OFF");
        else if (step || fabsf(x) >= 100) snprintf(val, sizeof val, "%.0f", x);
        else snprintf(val, sizeof val, "%.1f", x);
        label_text(p, 207, rows[i] + 1, val, 0xffffff);
    }
}

/* ---- answers to the lookups hs-fx makes into SampQOL / hs-flavor on the MPC ---- */
static const char *bank_title(int *section)
{
    struct view v = cur ? current(cur, NULL) : (struct view){0};
    if (section) *section = v.b ? v.b->module : 0;
    return v.b ? v.b->title : NULL;
}
static const char *knob_unit(int row)
{
    struct view v = cur ? current(cur, NULL) : (struct view){0};
    if (!v.b || row < 0 || row >= KNOBS) return NULL;
    const char *u = v.b->units[v.half * 3 + row];
    return *u ? u : NULL;
}
static int eq_selection(int *page) { *page = cur ? cur->page : 0; return cur ? cur->band : 1; }
static int mod_view(const int *values, int *source, const char **where, const char **label)
{
    (void)values;
    struct view v = cur ? current(cur, NULL) : (struct view){0};
    int mode = v.b && !strcmp(v.b->title, "MOD") ? 1 : v.b && !strcmp(v.b->title, "MOD TIME") ? 2 : 0;
    if (!mode) return 0;
    int base = v.b->parameters[0] - (mode == 1 ? HS_MOD_SRC : HS_MOD_ATTACK);
    *source = (int)lroundf(cur->ctl[cur->param_port[base + HS_MOD_SRC]]);
    int target = (int)lroundf(cur->ctl[cur->param_port[base + HS_MOD_TARGET]]);
    *where = "-"; *label = "-";
    for (int i = 0; cur->tab && i < cur->ntab; ++i)          /* the page and name the target knob has */
        for (int j = 0; j < 6; ++j)
            if (cur->tab[i].parameters[j] == target && strcmp(cur->tab[i].title, "MOD")) {
                *where = cur->tab[i].title; *label = cur->tab[i].labels[j];
                return mode;
            }
    return mode;
}
static const float *eq_values(unsigned *seq)
{
    if (seq) *seq = 0;
    return cur ? &cur->ctl[cur->param_port[0]] : NULL;    /* controls are contiguous, EQ_* order */
}
static int eq_response(int which, int points, float *db)
{
    return cur && hs_eq2_response_of(eq_values(NULL), 48000, which, points, db);
}

static void *hs_gui_sym(const char *name)
{
    static const struct { const char *name; void *fn; } table[] = {
        {"hs_channel_bank", (void *)bank_title}, {"hs_xsat_bank", (void *)bank_title},
        {"hs_bus_bank", (void *)bank_title}, {"hs_pump_bank", (void *)bank_title},
        {"hs_heat_bank", (void *)bank_title}, {"hs_distressor_bank", (void *)bank_title},
        {"hs_tube_strip_bank", (void *)bank_title}, {"hs_knob_unit", (void *)knob_unit},
        {"hs_eq2_selection", (void *)eq_selection}, {"hs_mod_view", (void *)mod_view},
    };
    for (size_t i = 0; i < sizeof table / sizeof *table; ++i)
        if (!strcmp(name, table[i].name)) return table[i].fn;
    return NULL;                       /* presets page, flavor, SampQOL internals: not here */
}

/* ---- public ---- */
hs_panel *hs_panel_new(const LADSPA_Descriptor *d, float *ctl, const uint32_t *param_port, uint32_t nparams)
{
    static const struct { const char *name; enum effect kind; const FlavorBank *tab; int n; } kinds[] = {
#define T(name, kind, tab) {name, kind, tab, (int)(sizeof tab / sizeof *tab)}
        {"HS Saturator", HS_SATURATOR, NULL, 0}, {"HS Air Exciter", HS_AIR_EXCITER, NULL, 0},
        T("HS Channel Strip", HS_CHANNEL, channel_banks), T("HS Drive Sat", HS_XSAT, xsat_banks),
        T("HS Bus Compressor", HS_BUS, bus_banks), {"HS Para EQ", HS_EQ2, NULL, 0},
        T("HS Pump", HS_PUMP, pump_banks), T("HS Heat", HS_HEAT, heat_banks),
        T("HS Crush", HS_DISTRESSOR, dist_banks), {"HS Bass Focus", HS_BASS_FOCUS, NULL, 0},
        T("HS Tube Strip", HS_TUBE_STRIP, tube_banks),
#undef T
    };
    hs_panel *s = calloc(1, sizeof *s);
    if (!s) return NULL;
    s->d = d; s->ctl = ctl; s->param_port = param_port; s->nparams = nparams;   /* ~650 KB: no stack temporary */
    s->kind = OTHER; s->band = 1;
    for (size_t i = 0; i < sizeof kinds / sizeof *kinds; ++i)
        if (!strcmp(d->Name, kinds[i].name)) { s->kind = kinds[i].kind; s->tab = kinds[i].tab; s->ntab = kinds[i].n; }
    eq2_response = eq_response;       /* hs_eq2_view.h: already linked, so it never looks for the MPC library */
    eq2_values = eq_values;
    return s;
}

void hs_panel_free(hs_panel *s) { free(s); }

static void arrow(uint32_t *p, int x, int y, int dir, uint32_t c)
{
    for (int i = 0; i < 6; ++i) rect(p, dir > 0 ? x + i : x + 5 - i, y + i, 1, 11 - 2 * i, c);
}

static void nav_row(uint32_t *p, int y, int h, const char *label)
{
    rect(p, 4, y, 312, h, 0x15424d);
    frame(p, 4, y, 312, h, cyan);
    rect(p, 40, y + 1, 1, h - 2, 0x2a6672);
    rect(p, 279, y + 1, 1, h - 2, 0x2a6672);
    arrow(p, 18, y + h / 2 - 5, -1, 0xc9f5ff);
    arrow(p, 296, y + h / 2 - 5, 1, 0xc9f5ff);
    preset_text(p, 160 - 4 * (int)strlen(label), y + h / 2 - 5, label, 0xc9f5ff);
}

const uint32_t *hs_panel_draw(hs_panel *s, const void *handle)
{
    cur = s;
    int count;
    struct view v = current(s, &count);
    if (s->page >= count) s->page = 0;
    fake_stock(s);
    s->ep.kind = s->kind == OTHER ? HS_SATURATOR : s->kind;
    s->ep.shifted = 0;
    for (int i = 0; i < KNOBS; ++i) {
        int k = knob_param(s, i);
        s->ep.values[i] = s->ep.main_values[i] = k < 0 ? -1 : (int)lroundf(to_pos(s, s->param_port[k], s->ctl[s->param_port[k]]));
        s->ep.bypassed[i] = 0;
    }
    if (s->kind == HS_DISTRESSOR) dist_gr_db = handle ? hs_distressor_gr(handle) : -1;
    if (s->kind == HS_TUBE_STRIP) tube_gr_db = handle ? hs_tube_strip_gr(handle) : -1;
    const uint32_t *panel = draw_gear(&s->ep);
    memcpy(s->out + HEAD * W, panel, sizeof s->ep.rendered);

    uint32_t *p = s->out;
    rect(p, 0, 0, W, HEAD, 0x141b1f);
    rect(p, 0, 0, W, 2, orbit_red);
    rect(p, 0, HEAD - 1, W, 1, rail);
    char label[48];
    if (s->kind == HS_EQ2) {
        snprintf(label, sizeof label, s->band ? "BAND %d" : "GLOBAL", s->band);
        nav_row(p, 4, 16, label);
        snprintf(label, sizeof label, "%s %d/%d", v.b ? v.b->title : "", s->page + 1, count);
        nav_row(p, 21, 16, label);
    } else if (count > 1) {
        snprintf(label, sizeof label, "%s  %d/%d", v.b ? v.b->title : "PAGE", s->page + 1, count);
        nav_row(p, 6, 28, label);
    } else {
        preset_text(p, 160 - 4 * (int)strlen(s->d->Name), 15, s->d->Name, 0xc6d4db);
    }
    cur = NULL;
    return s->out;
}

int hs_panel_click(hs_panel *s, int x, int y)
{
    if (s->kind == HS_EQ2 && y >= HEAD) {      /* the Para EQ's own tabs (hs_eq2_view.h eq2_header) */
        int py = y - HEAD, tabs = s->band ? 4 : 2, x0 = 312 - tabs * 40;
        if (py >= 46 && py < 57 && x >= 9 && x < 10 + 12 * (EQ_BANDS + 1)) {
            s->band = (x - 9) / 12;
            s->page = 0;
            return 1;
        }
        if (py >= 35 && py < 45 && x >= x0 && x < 312) { s->page = (x - x0) / 40; return 1; }
        return 0;
    }
    if (y >= HEAD) return 0;
    int dir = x < 41 ? -1 : x >= 279 ? 1 : 0;
    if (!dir) return 0;
    int count;
    if (s->kind == HS_EQ2 && y < 21) {
        s->band = (s->band + dir + EQ_BANDS + 1) % (EQ_BANDS + 1);
        s->page = 0;
        return 1;
    }
    current(s, &count);
    s->page = (s->page + dir + count) % count;
    return 1;
}

int hs_panel_knob_at(hs_panel *s, int x, int y)
{
    /* the knob boxes only: Para EQ's sit under its tabs and graph, the stock-rack faceplates under their art */
    int top = s->kind == HS_EQ2 ? 125 : s->kind >= DELAY ? 81 : 53;
    if (y < HEAD + top || x < 8) return -1;
    int i = (x - 8) / 103;
    if (i >= KNOBS || (x - 8) % 103 >= 98) return -1;
    int k = knob_param(s, i);
    return k < 0 ? -1 : (int)s->param_port[k];
}

float hs_panel_drag(hs_panel *s, int port, float start, float dy)
{
    return from_pos(s, (unsigned long)port, to_pos(s, (unsigned long)port, start) + dy * .5f);
}
