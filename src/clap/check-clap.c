/* check-clap BUNDLE_BINARY: load the CLAP like a host, run every plugin over 2 s of a -12 dBFS
 * 220 Hz sine at defaults, then with every parameter at its max. FAIL on NaN/inf anywhere, or on
 * silence or a peak over +24 dBFS at defaults. Then: every plugin's state must save and load back
 * exactly (and refuse garbage), and every factory preset must be listed, load, and play.
 * Exit 1 on any FAIL. */
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
typedef struct { char buf[16384]; size_t len, pos; } mem_t;
static int64_t m_write(const clap_ostream_t *o, const void *b, uint64_t n) {
    mem_t *m = o->ctx; if (m->len + n > sizeof m->buf) return -1;
    memcpy(m->buf + m->len, b, n); m->len += n; return (int64_t)n;
}
static int64_t m_read(const clap_istream_t *i, void *b, uint64_t n) {
    mem_t *m = i->ctx; size_t k = m->len - m->pos < n ? m->len - m->pos : n;
    if (k > 7) k = 7;                                 /* short reads, like a real host stream */
    memcpy(b, m->buf + m->pos, k); m->pos += k; return (int64_t)k;
}

/* preset discovery: a fake indexer and receiver that record what the provider declares */
static int n_locations, n_presets;
static char preset_key[64][64], preset_plugin[64][96];
static bool ix_location(const clap_preset_discovery_indexer_t *ix, const clap_preset_discovery_location_t *l) {
    (void)ix; n_locations += l->kind == CLAP_PRESET_DISCOVERY_LOCATION_PLUGIN; return true;
}
static bool ix_type(const clap_preset_discovery_indexer_t *ix, const clap_preset_discovery_filetype_t *t) { (void)ix; (void)t; return true; }
static bool ix_pack(const clap_preset_discovery_indexer_t *ix, const clap_preset_discovery_soundpack_t *p) { (void)ix; (void)p; return true; }
static const void *ix_ext(const clap_preset_discovery_indexer_t *ix, const char *id) { (void)ix; (void)id; return NULL; }
static bool rx_begin(const clap_preset_discovery_metadata_receiver_t *r, const char *name, const char *key) {
    (void)r; (void)name;
    if (n_presets >= 64) return false;
    snprintf(preset_key[n_presets++], 64, "%s", key); return true;
}
static void rx_plugin(const clap_preset_discovery_metadata_receiver_t *r, const clap_universal_plugin_id_t *id) {
    (void)r; if (n_presets) snprintf(preset_plugin[n_presets - 1], 96, "%s", id->id);
}
static void rx_err(const clap_preset_discovery_metadata_receiver_t *r, int32_t e, const char *m) {
    (void)r; printf("FAIL preset discovery error %d: %s\n", e, m);
}
static void rx_s(const clap_preset_discovery_metadata_receiver_t *r, const char *s) { (void)r; (void)s; }
static void rx_u(const clap_preset_discovery_metadata_receiver_t *r, uint32_t u) { (void)r; (void)u; }
static void rx_t(const clap_preset_discovery_metadata_receiver_t *r, clap_timestamp a, clap_timestamp b) { (void)r; (void)a; (void)b; }
static void rx_kv(const clap_preset_discovery_metadata_receiver_t *r, const char *k, const char *v) { (void)r; (void)k; (void)v; }

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
    /* state round trip: every parameter to its middle, save, load into a fresh instance, compare */
    for (uint32_t k = 0; k < count; ++k) {
        const clap_plugin_descriptor_t *d = f->get_plugin_descriptor(f, k);
        const clap_plugin_t *a = f->create_plugin(f, &host, d->id), *b = f->create_plugin(f, &host, d->id);
        if (!a || !b || !a->init(a) || !b->init(b)) { printf("FAIL %-28s state: create\n", d->name); ++fails; continue; }
        const clap_plugin_params_t *pp = a->get_extension(a, CLAP_EXT_PARAMS);
        const clap_plugin_state_t *sa = a->get_extension(a, CLAP_EXT_STATE), *sb = b->get_extension(b, CLAP_EXT_STATE);
        static events_t evs; evs.n = 0;
        uint32_t np = pp->count(a);
        for (uint32_t i = 0; i < np && i < 256; ++i) {
            clap_param_info_t info; pp->get_info(a, i, &info);
            evs.ev[evs.n++] = (clap_event_param_value_t){
                .header = {sizeof(clap_event_param_value_t), 0, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_PARAM_VALUE, 0},
                .param_id = info.id, .note_id = -1, .port_index = -1, .channel = -1, .key = -1,
                .value = (info.min_value + info.max_value) / 2};
        }
        clap_input_events_t iev = {&evs, ev_size, ev_get};
        clap_output_events_t oev = {NULL, ev_push};
        pp->flush(a, &iev, &oev);
        static mem_t m; m.len = m.pos = 0;
        clap_ostream_t os = {&m, m_write}; clap_istream_t is = {&m, m_read};
        int ok = sa && sb && sa->save(a, &os) && sb->load(b, &is);
        for (uint32_t i = 0; ok && i < np; ++i) {
            clap_param_info_t info; double va, vb; pp->get_info(a, i, &info);
            ok = pp->get_value(a, info.id, &va) && pp->get_value(b, info.id, &vb) && va == vb &&
                 fabs(va - (info.min_value + info.max_value) / 2) < 1e-3 * (info.max_value - info.min_value + 1);
        }
        static mem_t junk = {"not a state", 11, 0}; junk.pos = 0;
        clap_istream_t js = {&junk, m_read};
        ok = ok && !sb->load(b, &js);
        printf("%s %-28s state round trip (%zu bytes)\n", ok ? "PASS" : "FAIL", d->name, m.len);
        fails += !ok;
        a->destroy(a); b->destroy(b);
    }

    /* factory presets: listed by the preset-discovery factory, then each one loads and plays */
    const clap_preset_discovery_factory_t *pf = e->get_factory(CLAP_PRESET_DISCOVERY_FACTORY_ID);
    clap_preset_discovery_indexer_t ix = {CLAP_VERSION_INIT, "check-clap", "hs", "", "1", NULL, ix_type, ix_location, ix_pack, ix_ext};
    clap_preset_discovery_metadata_receiver_t rx = {NULL, rx_err, rx_begin, rx_plugin, rx_s, rx_u, rx_s, rx_s, rx_t, rx_s, rx_kv};
    const clap_preset_discovery_provider_t *pv = pf && pf->count(pf) == 1 ? pf->create(pf, &ix, pf->get_descriptor(pf, 0)->id) : NULL;
    if (!pv || !pv->init(pv) || n_locations != 1 || !pv->get_metadata(pv, CLAP_PRESET_DISCOVERY_LOCATION_PLUGIN, NULL, &rx) || n_presets < 2) {
        printf("FAIL preset discovery (%d locations, %d presets)\n", n_locations, n_presets); ++fails;
    }
    if (pv) pv->destroy(pv);
    for (int i = 0; i < n_presets; ++i) {
        const clap_plugin_t *p = f->create_plugin(f, &host, preset_plugin[i]);
        if (!p || !p->init(p)) { printf("FAIL preset %s: no plugin %s\n", preset_key[i], preset_plugin[i]); ++fails; continue; }
        const clap_plugin_preset_load_t *pl = p->get_extension(p, CLAP_EXT_PRESET_LOAD);
        const clap_plugin_params_t *pp = p->get_extension(p, CLAP_EXT_PARAMS);
        int ok = pl && pl->from_location(p, CLAP_PRESET_DISCOVERY_LOCATION_PLUGIN, NULL, preset_key[i]) &&
                 !pl->from_location(p, CLAP_PRESET_DISCOVERY_LOCATION_PLUGIN, NULL, "No Such Preset");
        double attack = -1;
        for (uint32_t j = 0; ok && j < pp->count(p); ++j) {
            clap_param_info_t info; pp->get_info(p, j, &info);
            if (!strncmp(info.name, "Attack", 6)) pp->get_value(p, info.id, &attack);
        }
        if (ok && !strcmp(preset_key[i], "Kick")) ok = fabs(attack - 15) < 1e-4;   /* the table says Attack=15 */
        static events_t evs; double ri, ro; float pk; evs.n = 0;
        ok = ok && p->activate(p, RATE, 1, BLOCK) && !run(p, &evs, &ri, &ro, &pk) && ro > 1e-5;
        printf("%s preset %-12s on %s (Attack %.1f, %+.1f dB)\n", ok ? "PASS" : "FAIL", preset_key[i], preset_plugin[i],
               attack, ok ? 20 * log10(ro / ri) : 0.0);
        fails += !ok;
        if (ok) p->deactivate(p);
        p->destroy(p);
    }
    e->deinit();
    printf("%u plugins, %d fails\n", count, fails);
    return fails != 0;
}
