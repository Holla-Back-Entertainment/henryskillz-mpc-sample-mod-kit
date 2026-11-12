/* HS effects as CLAP plugins: wraps every LADSPA descriptor that hs_ladspa.c exports, so the DSP
 * is the exact code the MPC runs. One stereo in, one stereo out; each control input port becomes a
 * CLAP parameter whose id is the LADSPA port index.
 * ponytail: parameter events are applied at the start of each block (not sample-accurate) and
 * there is no state save yet; add both when a host that keeps projects needs them. */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <clap/clap.h>
#include "../fx/ladspa.h"

const LADSPA_Descriptor *ladspa_descriptor(unsigned long index);

#define MAX_FX 32
static clap_plugin_descriptor_t descs[MAX_FX];
static char ids[MAX_FX][96];
static uint32_t fx_count;
static const char *features[] = {CLAP_PLUGIN_FEATURE_AUDIO_EFFECT, CLAP_PLUGIN_FEATURE_STEREO, NULL};

typedef struct {
    clap_plugin_t plugin;
    const clap_host_t *host;
    const LADSPA_Descriptor *d;
    LADSPA_Handle h;
    float *ctl;                       /* one slot per LADSPA port: control values and output scratch */
    int in[2], out[2];                /* audio port indices */
    uint32_t nparams;
    uint32_t *param_port;             /* CLAP param index -> LADSPA port index */
} fx_t;

/* The LADSPA default rules (ladspa.h, LADSPA_HINT_DEFAULT_*). */
static float ladspa_default(const LADSPA_PortRangeHint *r, float rate) {
    int f = r->HintDescriptor, lg = LADSPA_IS_HINT_LOGARITHMIC(f);
    float lo = r->LowerBound, hi = r->UpperBound;
    if (LADSPA_IS_HINT_SAMPLE_RATE(f)) { lo *= rate; hi *= rate; }
    #define MIX(a) (lg && lo > 0 && hi > 0 ? expf(logf(lo) * (1 - (a)) + logf(hi) * (a)) : lo * (1 - (a)) + hi * (a))
    switch (f & LADSPA_HINT_DEFAULT_MASK) {
    case LADSPA_HINT_DEFAULT_MINIMUM: return lo;
    case LADSPA_HINT_DEFAULT_LOW:     return MIX(.25f);
    case LADSPA_HINT_DEFAULT_MIDDLE:  return MIX(.5f);
    case LADSPA_HINT_DEFAULT_HIGH:    return MIX(.75f);
    case LADSPA_HINT_DEFAULT_MAXIMUM: return hi;
    case LADSPA_HINT_DEFAULT_0:       return 0;
    case LADSPA_HINT_DEFAULT_1:       return 1;
    case LADSPA_HINT_DEFAULT_100:     return 100;
    case LADSPA_HINT_DEFAULT_440:     return 440;
    default: return LADSPA_IS_HINT_BOUNDED_BELOW(f) ? lo : 0;
    }
    #undef MIX
}
static void range(const LADSPA_PortRangeHint *r, double *lo, double *hi) {
    int f = r->HintDescriptor;
    *lo = LADSPA_IS_HINT_BOUNDED_BELOW(f) ? r->LowerBound : -1e6;
    *hi = LADSPA_IS_HINT_BOUNDED_ABOVE(f) ? r->UpperBound : 1e6;
    if (LADSPA_IS_HINT_TOGGLED(f)) { *lo = 0; *hi = 1; }
    if (LADSPA_IS_HINT_SAMPLE_RATE(f)) { *lo *= 48000; *hi *= 48000; }   /* ponytail: display range at 48 kHz */
}

/* params */
static uint32_t p_count(const clap_plugin_t *p) { return ((fx_t *)p->plugin_data)->nparams; }
static bool p_info(const clap_plugin_t *p, uint32_t i, clap_param_info_t *info) {
    fx_t *s = p->plugin_data;
    if (i >= s->nparams) return false;
    unsigned long port = s->param_port[i];
    const LADSPA_PortRangeHint *r = &s->d->PortRangeHints[port];
    memset(info, 0, sizeof *info);
    info->id = (clap_id)port;
    info->flags = CLAP_PARAM_IS_AUTOMATABLE |
                  (LADSPA_IS_HINT_INTEGER(r->HintDescriptor) || LADSPA_IS_HINT_TOGGLED(r->HintDescriptor) ? CLAP_PARAM_IS_STEPPED : 0);
    snprintf(info->name, sizeof info->name, "%s", s->d->PortNames[port]);
    range(r, &info->min_value, &info->max_value);
    info->default_value = ladspa_default(r, 48000);
    return true;
}
static bool p_value(const clap_plugin_t *p, clap_id id, double *v) {
    fx_t *s = p->plugin_data;
    if (id >= s->d->PortCount) return false;
    *v = s->ctl[id];
    return true;
}
static bool p_to_text(const clap_plugin_t *p, clap_id id, double v, char *out, uint32_t n) {
    (void)p; (void)id; snprintf(out, n, "%.2f", v); return true;
}
static bool p_from_text(const clap_plugin_t *p, clap_id id, const char *t, double *v) {
    (void)p; (void)id; char *e; *v = strtod(t, &e); return e != t;
}
static void apply_events(fx_t *s, const clap_input_events_t *in) {
    uint32_t n = in ? in->size(in) : 0;
    for (uint32_t i = 0; i < n; ++i) {
        const clap_event_header_t *e = in->get(in, i);
        if (e->space_id != CLAP_CORE_EVENT_SPACE_ID || e->type != CLAP_EVENT_PARAM_VALUE) continue;
        const clap_event_param_value_t *pv = (const clap_event_param_value_t *)e;
        if (pv->param_id < s->d->PortCount && LADSPA_IS_PORT_CONTROL(s->d->PortDescriptors[pv->param_id]))
            s->ctl[pv->param_id] = (float)pv->value;
    }
}
static void p_flush(const clap_plugin_t *p, const clap_input_events_t *in, const clap_output_events_t *out) {
    (void)out; apply_events(p->plugin_data, in);
}
static const clap_plugin_params_t params_ext = {p_count, p_info, p_value, p_to_text, p_from_text, p_flush};

/* audio ports: one stereo in, one stereo out */
static uint32_t a_count(const clap_plugin_t *p, bool is_input) { (void)p; (void)is_input; return 1; }
static bool a_get(const clap_plugin_t *p, uint32_t i, bool is_input, clap_audio_port_info_t *info) {
    (void)p;
    if (i) return false;
    memset(info, 0, sizeof *info);
    info->id = is_input ? 0 : 1;
    snprintf(info->name, sizeof info->name, "%s", is_input ? "In" : "Out");
    info->flags = CLAP_AUDIO_PORT_IS_MAIN;
    info->channel_count = 2;
    info->port_type = CLAP_PORT_STEREO;
    info->in_place_pair = CLAP_INVALID_ID;
    return true;
}
static const clap_plugin_audio_ports_t ports_ext = {a_count, a_get};

/* plugin */
static bool fx_init(const clap_plugin_t *p) {
    fx_t *s = p->plugin_data;
    int ai = 0, ao = 0;
    s->ctl = calloc(s->d->PortCount, sizeof *s->ctl);
    s->param_port = calloc(s->d->PortCount, sizeof *s->param_port);
    if (!s->ctl || !s->param_port) return false;
    for (unsigned long i = 0; i < s->d->PortCount; ++i) {
        LADSPA_PortDescriptor t = s->d->PortDescriptors[i];
        if (LADSPA_IS_PORT_AUDIO(t)) {
            if (LADSPA_IS_PORT_INPUT(t) && ai < 2) s->in[ai++] = (int)i;
            else if (LADSPA_IS_PORT_OUTPUT(t) && ao < 2) s->out[ao++] = (int)i;
        } else if (LADSPA_IS_PORT_INPUT(t)) {
            s->ctl[i] = ladspa_default(&s->d->PortRangeHints[i], 48000);
            s->param_port[s->nparams++] = (uint32_t)i;
        }
    }
    return ai == 2 && ao == 2;
}
static void fx_destroy(const clap_plugin_t *p) {
    fx_t *s = p->plugin_data;
    if (s->h) s->d->cleanup(s->h);
    free(s->ctl); free(s->param_port); free(s); free((void *)p);
}
static bool fx_activate(const clap_plugin_t *p, double rate, uint32_t minf, uint32_t maxf) {
    (void)minf; (void)maxf;
    fx_t *s = p->plugin_data;
    s->h = s->d->instantiate(s->d, (unsigned long)rate);
    if (!s->h) return false;
    for (unsigned long i = 0; i < s->d->PortCount; ++i)
        if (LADSPA_IS_PORT_CONTROL(s->d->PortDescriptors[i])) s->d->connect_port(s->h, i, &s->ctl[i]);
    if (s->d->activate) s->d->activate(s->h);
    return true;
}
static void fx_deactivate(const clap_plugin_t *p) {
    fx_t *s = p->plugin_data;
    if (s->h) { s->d->cleanup(s->h); s->h = NULL; }
}
static bool fx_start(const clap_plugin_t *p) { (void)p; return true; }
static void fx_stop(const clap_plugin_t *p) { (void)p; }
static void fx_reset(const clap_plugin_t *p) {
    fx_t *s = p->plugin_data;
    if (s->h && s->d->activate) s->d->activate(s->h);
}
static clap_process_status fx_process(const clap_plugin_t *p, const clap_process_t *pr) {
    fx_t *s = p->plugin_data;
    apply_events(s, pr->in_events);
    if (!pr->audio_inputs_count || !pr->audio_outputs_count) return CLAP_PROCESS_ERROR;
    float **in = pr->audio_inputs[0].data32, **out = pr->audio_outputs[0].data32;
    if (!in || !out) return CLAP_PROCESS_ERROR;
    for (int c = 0; c < 2; ++c) {
        s->d->connect_port(s->h, (unsigned long)s->in[c], in[c]);
        s->d->connect_port(s->h, (unsigned long)s->out[c], out[c]);
    }
    s->d->run(s->h, pr->frames_count);
    return CLAP_PROCESS_CONTINUE;
}
static const void *fx_ext(const clap_plugin_t *p, const char *id) {
    (void)p;
    if (!strcmp(id, CLAP_EXT_PARAMS)) return &params_ext;
    if (!strcmp(id, CLAP_EXT_AUDIO_PORTS)) return &ports_ext;
    return NULL;
}
static void fx_main_thread(const clap_plugin_t *p) { (void)p; }

/* factory */
static uint32_t f_count(const clap_plugin_factory_t *f) { (void)f; return fx_count; }
static const clap_plugin_descriptor_t *f_desc(const clap_plugin_factory_t *f, uint32_t i) {
    (void)f; return i < fx_count ? &descs[i] : NULL;
}
static const clap_plugin_t *f_create(const clap_plugin_factory_t *f, const clap_host_t *host, const char *id) {
    (void)f;
    if (!clap_version_is_compatible(host->clap_version)) return NULL;
    for (uint32_t i = 0; i < fx_count; ++i) {
        if (strcmp(id, descs[i].id)) continue;
        fx_t *s = calloc(1, sizeof *s);
        clap_plugin_t *p = calloc(1, sizeof *p);
        if (!s || !p) { free(s); free(p); return NULL; }
        s->host = host;
        s->d = ladspa_descriptor(i);
        *p = (clap_plugin_t){&descs[i], s, fx_init, fx_destroy, fx_activate, fx_deactivate, fx_start, fx_stop,
                             fx_reset, fx_process, fx_ext, fx_main_thread};
        return p;
    }
    return NULL;
}
static const clap_plugin_factory_t factory = {f_count, f_desc, f_create};

static bool e_init(const char *path) {
    (void)path;
    fx_count = 0;
    for (const LADSPA_Descriptor *d; fx_count < MAX_FX && (d = ladspa_descriptor(fx_count)); ++fx_count) {
        /* id from the public name ("HS Crush" -> com.henryskillz.hs_crush), never the internal label */
        int n = snprintf(ids[fx_count], sizeof ids[fx_count], "com.henryskillz.");
        for (const char *c = d->Name; *c && n < (int)sizeof ids[fx_count] - 1; ++c)
            ids[fx_count][n++] = *c == ' ' ? '_' : (char)(*c >= 'A' && *c <= 'Z' ? *c + 32 : *c);
        ids[fx_count][n] = 0;
        descs[fx_count] = (clap_plugin_descriptor_t){
            .clap_version = CLAP_VERSION_INIT, .id = ids[fx_count], .name = d->Name, .vendor = "Henry Skillz",
            .url = "https://github.com/hmukhwana/henryskillz-mpc-sample-mod-kit", .version = "0.6.0",
            .description = "Henry Skillz effect (the same DSP as the MPC Sample kit)", .features = features};
    }
    return fx_count > 0;
}
static void e_deinit(void) {}
static const void *e_factory(const char *id) { return strcmp(id, CLAP_PLUGIN_FACTORY_ID) ? NULL : &factory; }

CLAP_EXPORT const clap_plugin_entry_t clap_entry = {CLAP_VERSION_INIT, e_init, e_deinit, e_factory};
