/* check-clap BUNDLE_BINARY: load the CLAP like a host, run every plugin over 2 s of a -12 dBFS
 * 220 Hz sine at defaults, then with every parameter at its max. FAIL on NaN/inf anywhere, or on
 * silence or a peak over +24 dBFS at defaults. Exit 1 on any FAIL. */
#include <dlfcn.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <clap/clap.h>

#define RATE 48000
#define BLOCK 512
static const clap_host_t host = {CLAP_VERSION_INIT, NULL, "check-clap", "hs", "", "1",
                                 NULL, NULL, NULL, NULL};

typedef struct { clap_event_param_value_t ev[256]; uint32_t n; } events_t;
static uint32_t ev_size(const clap_input_events_t *l) { return ((const events_t *)l->ctx)->n; }
static const clap_event_header_t *ev_get(const clap_input_events_t *l, uint32_t i) {
    return &((const events_t *)l->ctx)->ev[i].header;
}
static bool ev_push(const clap_output_events_t *l, const clap_event_header_t *e) { (void)l; (void)e; return true; }

static int run(const clap_plugin_t *p, events_t *evs, double *rms_in, double *rms_out, float *peak) {
    float il[BLOCK], ir[BLOCK], ol[BLOCK], or_[BLOCK], *in[2] = {il, ir}, *out[2] = {ol, or_};
    clap_audio_buffer_t ain = {.data32 = in, .channel_count = 2}, aout = {.data32 = out, .channel_count = 2};
    clap_input_events_t iev = {evs, ev_size, ev_get};
    clap_output_events_t oev = {NULL, ev_push};
    double si = 0, so = 0; long n = 0; int bad = 0; *peak = 0;
    for (long b = 0; b < 2 * RATE / BLOCK; ++b) {
        for (int i = 0; i < BLOCK; ++i) il[i] = ir[i] = .25f * sinf(2 * (float)M_PI * 220 * (float)(b * BLOCK + i) / RATE);
        clap_process_t pr = {.steady_time = b * BLOCK, .frames_count = BLOCK, .audio_inputs = &ain, .audio_outputs = &aout,
                             .audio_inputs_count = 1, .audio_outputs_count = 1, .in_events = &iev, .out_events = &oev};
        if (p->process(p, &pr) == CLAP_PROCESS_ERROR) return -1;
        evs->n = 0;                                   /* events only in the first block */
        for (int i = 0; i < BLOCK; ++i) for (int c = 0; c < 2; ++c) {
            float x = out[c][i];
            if (!isfinite(x)) bad = 1;
            if (fabsf(x) > *peak) *peak = fabsf(x);
            so += (double)x * x; si += (double)in[c][i] * in[c][i]; ++n;
        }
    }
    *rms_in = sqrt(si / n); *rms_out = sqrt(so / n);
    return bad;
}

int main(int argc, char **argv) {
    if (argc != 2) { fprintf(stderr, "usage: check-clap BUNDLE_BINARY\n"); return 2; }
    void *lib = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!lib) { fprintf(stderr, "dlopen: %s\n", dlerror()); return 1; }
    const clap_plugin_entry_t *e = dlsym(lib, "clap_entry");
    if (!e || !e->init(argv[1])) { fprintf(stderr, "no clap_entry or init failed\n"); return 1; }
    const clap_plugin_factory_t *f = e->get_factory(CLAP_PLUGIN_FACTORY_ID);
    int fails = 0;
    uint32_t count = f->get_plugin_count(f);
    for (uint32_t k = 0; k < count; ++k) {
        const clap_plugin_descriptor_t *d = f->get_plugin_descriptor(f, k);
        const clap_plugin_t *p = f->create_plugin(f, &host, d->id);
        if (!p || !p->init(p) || !p->activate(p, RATE, 1, BLOCK) || !p->start_processing(p)) {
            printf("FAIL %-28s create/init/activate\n", d->name); ++fails; continue;
        }
        const clap_plugin_params_t *pp = p->get_extension(p, CLAP_EXT_PARAMS);
        const clap_plugin_audio_ports_t *ap = p->get_extension(p, CLAP_EXT_AUDIO_PORTS);
        uint32_t np = pp ? pp->count(p) : 0;
        static events_t evs; double ri, ro; float pk;
        evs.n = 0;
        int bad = run(p, &evs, &ri, &ro, &pk);
        double gain_db = 20 * log10(ro / ri);
        int ok = !bad && ro > 1e-5 && pk < 15.85f && ap && ap->count(p, true) == 1;
        printf("%s %-28s %2u params  defaults: %+6.1f dB  ", ok ? "PASS" : "FAIL", d->name, np, gain_db);
        fails += !ok;
        /* every parameter at its max */
        p->reset(p);
        evs.n = 0;
        for (uint32_t i = 0; i < np && i < 256; ++i) {
            clap_param_info_t info;
            if (!pp->get_info(p, i, &info)) continue;
            evs.ev[evs.n++] = (clap_event_param_value_t){
                .header = {sizeof(clap_event_param_value_t), 0, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_PARAM_VALUE, 0},
                .param_id = info.id, .cookie = NULL, .note_id = -1, .port_index = -1, .channel = -1, .key = -1,
                .value = info.max_value};
        }
        bad = run(p, &evs, &ri, &ro, &pk);
        ok = !bad;                                    /* every gain/drive knob at max is meant to be loud */
        printf("all-max: %s (peak %+.1f dBFS)\n", ok ? "ok" : "FAIL", 20 * log10(pk + 1e-12));
        fails += !ok;
        p->stop_processing(p); p->deactivate(p); p->destroy(p);
    }
    e->deinit();
    printf("%u plugins, %d fails\n", count, fails);
    return fails != 0;
}
