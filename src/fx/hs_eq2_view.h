#ifndef HS_EQ2_VIEW_H
#define HS_EQ2_VIEW_H
/* Para EQ editor: response curve with band nodes, like the desktop plugin.
 * Included by hs_fx.c after hs_fx_faceplate.h. The curve comes from hs_eq2_response() in
 * hs-pro-fx-ladspa.so, i.e. the same biquads the audio runs, not a redrawn approximation. */
#include <complex.h>
#include <link.h>
#include <time.h>

typedef int (*eq2_response_fn)(int, int, float *);
typedef const float *(*eq2_values_fn)(unsigned *);
static eq2_response_fn eq2_response;
static eq2_values_fn eq2_values;
static const void *eq2_descriptor, *dist_descriptor;
static float (*dist_gr)(const void *);
static const void *tube_descriptor;
static float (*tube_gr)(const void *);
static unsigned (*eq2_tap)(const void *, float *, unsigned, unsigned long *);
typedef int (*mod_knob_fn)(const void *, int *);
/* MOD effects: descriptor and hs_<name>_mod_knob (hs_mod.h), by enum effect; NULL for the rest. */
static const char *const mod_names[HS_TUBE_STRIP + 1] = {
    [HS_CHANNEL] = "hs_channel", [HS_XSAT] = "hs_xsat", [HS_BUS] = "hs_bus", [HS_PUMP] = "hs_pump",
    [HS_HEAT] = "hs_heat", [HS_DISTRESSOR] = "hs_distressor", [HS_TUBE_STRIP] = "hs_tube_strip"
};
static const void *mod_descriptor[HS_TUBE_STRIP + 1];
static mod_knob_fn mod_knob[HS_TUBE_STRIP + 1];
static int (*live_lock)(const void *);      /* hs_registry.c: holds a live instance while it is read */
static void (*live_unlock)(void);

static int find_ladspa(struct dl_phdr_info *info, size_t size, void *out)
{
    (void)size;
    if (!strstr(info->dlpi_name, "hs-pro-fx-ladspa")) return 0;
    /* The firmware dlopens the library privately, so RTLD_DEFAULT cannot see it. NOLOAD hands
     * back that same copy and pins it, so it cannot be unloaded under the editor. */
    void *lib = dlopen(info->dlpi_name, RTLD_LAZY | RTLD_NOLOAD);
    if (lib) *(void **)out = lib;
    return lib != NULL;
}

static int eq2_link(void)
{
    if (eq2_response && eq2_values) return 1;
    void *lib = NULL;
    dl_iterate_phdr(find_ladspa, &lib);
    if (!lib) return 0;
    eq2_response = (eq2_response_fn)dlsym(lib, "hs_eq2_response");
    eq2_values = (eq2_values_fn)dlsym(lib, "hs_eq2_values");
    eq2_descriptor = dlsym(lib, "hs_eq2_descriptor");
    eq2_tap = (unsigned (*)(const void *, float *, unsigned, unsigned long *))dlsym(lib, "hs_eq2_tap");
    live_lock = (int (*)(const void *))dlsym(lib, "hs_ladspa_lock_live");
    live_unlock = (void (*)(void))dlsym(lib, "hs_ladspa_unlock");
    dist_descriptor = dlsym(lib, "hs_distressor_descriptor");
    dist_gr = (float (*)(const void *))dlsym(lib, "hs_distressor_gr");
    tube_descriptor = dlsym(lib, "hs_tube_strip_descriptor");
    tube_gr = (float (*)(const void *))dlsym(lib, "hs_tube_strip_gr");
    for (int k = 0; k <= HS_TUBE_STRIP; ++k) {
        if (!mod_names[k]) continue;
        char name[48];
        snprintf(name, sizeof name, "%s_descriptor", mod_names[k]);
        mod_descriptor[k] = dlsym(lib, name);
        snprintf(name, sizeof name, "%s_mod_knob", mod_names[k]);
        mod_knob[k] = (mod_knob_fn)dlsym(lib, name);
    }
    return eq2_response && eq2_values;
}

/* SampQOL 0.2.0 (pinned build): the insert editor object at qol+0x16333a0 holds the open insert at
 * +0xd0; the insert keeps its LADSPA descriptor at +0x128 and handle at +0x130 (read live on the
 * device 11 Nov 2026 with two Para EQs open). The descriptor check keeps a wrong read harmless. */
static void *open_handle(const void *descriptor)
{
    if (!qol_base || !descriptor) return NULL;
    const char *editor = *(char *const *)(qol_base + 0x16333a0);
    const char *insert = editor ? *(char *const *)(editor + 0xd0) : NULL;
    return insert && *(const void *const *)(insert + 0x128) == descriptor ? *(void *const *)(insert + 0x130) : NULL;
}

/* The meters are read from the UI thread and from the 40 ms animation thread, so the instance is
   held in the registry while its meter is read: its cleanup cannot free it mid-read. */
static float live_gr(float (*gr)(const void *), const void *descriptor)
{
    void *h = eq2_link() && gr && live_lock && live_unlock ? open_handle(descriptor) : NULL;
    if (!h || !live_lock(h)) return -1;
    float db = gr(h);
    live_unlock();
    return db;
}

/* Gain reduction of the open HS Crush, or -1 when it cannot be read. */
static float dist_open_gr(void)
{
    return live_gr(dist_gr, dist_descriptor);
}

/* Gain reduction of the open HS Tube Strip, or -1 when it cannot be read. */
static float tube_open_gr(void)
{
    return live_gr(tube_gr, tube_descriptor);
}

/* The open MOD effect's live target (-1: none, or it cannot be read) and *delta, the knob steps
   from its set value to the value the audio hears. Held in the registry while read, as live_gr. */
static int mod_open_knob(enum effect kind, int *delta)
{
    *delta = 0;
    if (kind > HS_TUBE_STRIP || !mod_names[kind]) return -1;
    void *h = eq2_link() && mod_knob[kind] && live_lock && live_unlock ? open_handle(mod_descriptor[kind]) : NULL;
    if (!h || !live_lock(h)) return -1;
    int target = mod_knob[kind](h, delta);
    live_unlock();
    return target;
}

static unsigned eq2_seq(void)
{
    unsigned seq = 0;
    if (eq2_link()) eq2_values(&seq);
    return seq;
}

enum { GX = 9, GY = 59, GW = 302, GH = 63, G_DB = 20 };

static uint32_t mix(uint32_t a, uint32_t b, int t)  /* t/256 of b over a */
{
    unsigned r = (((a >> 16) & 255) * (256 - t) + ((b >> 16) & 255) * t) >> 8;
    unsigned g = (((a >> 8) & 255) * (256 - t) + ((b >> 8) & 255) * t) >> 8;
    unsigned c = ((a & 255) * (256 - t) + (b & 255) * t) >> 8;
    return rgb(r, g, c);
}

static int db_y(float db)
{
    if (db > G_DB) db = G_DB;
    if (db < -G_DB) db = -G_DB;
    return GY + GH / 2 - (int)lroundf(db * (GH / 2 - 1) / G_DB);
}

static int hz_x(double hz) { return GX + (int)lround(log(hz / 20) / log(1000) * (GW - 1)); }

/* Live spectrum of the open Para EQ's output: a 2048-point Hann FFT of hs_eq2_tap, one value per
   graph column on the same log axis as the curve, falling 1.5 dB per frame. -72..0 dBFS fills the
   graph bottom to top. The maths runs here on the UI thread; the audio side only stores samples. */
enum { SPEC_N = 2048, SPEC_FLOOR = -72 };
static float eq2_spec[GW];
static int eq2_spec_live;

static void spec_fft(float complex *x)
{
    for (int i = 1, j = 0; i < SPEC_N; ++i) {
        int bit = SPEC_N >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j |= bit;
        if (i < j) { float complex t = x[i]; x[i] = x[j]; x[j] = t; }
    }
    for (int len = 2; len <= SPEC_N; len <<= 1) {
        float complex w = cexpf(-2 * 3.14159265f * I / len);
        for (int i = 0; i < SPEC_N; i += len) {
            float complex u = 1;
            for (int k = 0; k < len / 2; ++k, u *= w) {
                float complex a = x[i + k], b = x[i + k + len / 2] * u;
                x[i + k] = a + b; x[i + k + len / 2] = a - b;
            }
        }
    }
}

/* Called every UI poll; returns 1 when the spectrum moved enough to redraw (at most every 40 ms). */
static int eq2_spectrum_tick(void)
{
    static struct timespec last;
    static unsigned seen;
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    if ((now.tv_sec - last.tv_sec) * 1000 + (now.tv_nsec - last.tv_nsec) / 1000000 < 40) return 0;
    last = now;
    void *h = eq2_link() && eq2_tap ? open_handle(eq2_descriptor) : NULL;
    static float buf[SPEC_N];
    static float complex x[SPEC_N];
    unsigned long rate = 0;
    unsigned w = h ? eq2_tap(h, buf, SPEC_N, &rate) : seen;
    int fresh = h && rate && w != seen;
    seen = w;
    float mag[SPEC_N / 2];
    if (fresh) {
        for (int i = 0; i < SPEC_N; ++i) x[i] = buf[i] * (.5f - .5f * cosf(2 * 3.14159265f * i / (SPEC_N - 1)));
        spec_fft(x);
        for (int i = 0; i < SPEC_N / 2; ++i) mag[i] = cabsf(x[i]) * (4.0f / SPEC_N);   /* sine -> its peak level */
    }
    int moved = 0;
    if (!eq2_spec_live) for (int k = 0; k < GW; ++k) eq2_spec[k] = SPEC_FLOOR;   /* fresh open: start empty */
    for (int k = 0; k < GW; ++k) {
        float db = SPEC_FLOOR;
        if (fresh) {
            double lo = 20 * pow(1000, (k - .5) / (GW - 1)), hi = 20 * pow(1000, (k + .5) / (GW - 1));
            double b0 = lo * SPEC_N / rate, b1 = hi * SPEC_N / rate;
            float m = 0;
            if (b1 - b0 < 1) {                              /* narrower than a bin: interpolate */
                double c = .5 * (b0 + b1);
                int i = (int)c;
                if (i + 1 < SPEC_N / 2) m = mag[i] + (float)(c - i) * (mag[i + 1] - mag[i]);
            } else
                for (int i = (int)b0; i <= (int)b1 && i < SPEC_N / 2; ++i) m = fmaxf(m, mag[i]);
            db = m > 0 ? 20 * log10f(m) : SPEC_FLOOR;
        }
        float fall = eq2_spec[k] - 1.5f, v = fmaxf(fmaxf(db, fall), SPEC_FLOOR);
        if (fabsf(v - eq2_spec[k]) > .5f) moved = 1;
        eq2_spec[k] = v;
    }
    eq2_spec_live = h != NULL;
    return moved;
}

static const uint32_t band_colors[] = {0xe0503c, 0xd7dc5a, 0x45c16f, 0x3d8fe3, 0xec923c, 0xb56fdc};
static uint32_t band_color(int band) { return band_colors[(band - 1) % 6]; }

static void eq2_graph(uint32_t *p, int selected)
{
    const uint32_t bg = 0x0c1215, grid = 0x1f2a2f, zero = 0x3a4950;
    rect(p, GX - 1, GY - 1, GW + 2, GH + 2, bg);
    frame(p, GX - 1, GY - 1, GW + 2, GH + 2, 0x465259);
    /* live spectrum underneath everything else */
    for (int k = 0; eq2_spec_live && k < GW; ++k) {
        if (eq2_spec[k] <= SPEC_FLOOR + .5f) continue;
        int top = GY + GH - 1 - (int)lroundf((eq2_spec[k] - SPEC_FLOOR) * (GH - 1) / -SPEC_FLOOR);
        rect(p, GX + k, top, 1, GY + GH - top, mix(bg, 0x77bcb0, 46));
        rect(p, GX + k, top, 1, 1, mix(bg, 0x77bcb0, 110));
    }

    static const int lines[] = {50, 100, 200, 500, 1000, 2000, 5000, 10000};
    for (int i = 0; i < 8; ++i) rect(p, hz_x(lines[i]), GY, 1, GH, grid);
    for (int db = -15; db <= 15; db += 5) rect(p, GX, db_y(db), GW, 1, db ? grid : zero);
    text(p, hz_x(100) - 5, GY + GH - 6, "100", 3, 0x5e6f76);
    text(p, hz_x(1000) - 3, GY + GH - 6, "1K", 2, 0x5e6f76);
    text(p, hz_x(10000) - 5, GY + GH - 6, "10K", 3, 0x5e6f76);
    text(p, GX + 2, db_y(10) - 2, "+10", 3, 0x5e6f76);
    text(p, GX + 2, db_y(-10) - 2, "-10", 3, 0x5e6f76);

    float main[GW], side[GW], own[GW];
    const float *v = eq2_link() ? eq2_values(NULL) : NULL;
    if (!v || !eq2_response(0, GW, main) || !eq2_response(1, GW, side)) {
        rect(p, GX, db_y(0), GW, 1, 0x8fa3a8);
        text(p, GX + GW / 2 - 30, GY + 8, "PLAY TO SHOW CURVE", 18, 0x6f8288);
        return;
    }
    const float *sb = selected ? v + EQ_FIRST + (selected - 1) * EB_COUNT : NULL;
    int sel_live = sb && sb[EB_ON] >= .5f && sb[EB_BYPASS] < .5f;

    /* Selected band's own contribution, shaded between 0 dB and its curve. */
    if (sel_live && eq2_response(1 + selected, GW, own)) {
        uint32_t fill = mix(bg, band_color(selected), 70);
        int z = db_y(0);
        for (int k = 0; k < GW; ++k) {
            int y = db_y(own[k]);
            rect(p, GX + k, y < z ? y : z + 1, 1, y < z ? z - y : y - z, fill);
        }
    }
    int stereo_split = 0;
    for (int k = 0; k < GW; ++k) stereo_split |= fabsf(main[k] - side[k]) > .05f;
    uint32_t curve = 0xe6eee9;
    for (int pass = stereo_split ? 0 : 1; pass < 2; ++pass) {
        const float *c = pass ? main : side;
        uint32_t color = pass ? curve : 0x5f7d8a;
        for (int k = 1; k < GW; ++k)
            fx_line(p, GX + k - 1, db_y(c[k - 1]), GX + k, db_y(c[k]), color);
    }
    if (stereo_split) text(p, GX + GW - 30, GY + 3, "R/S DIM", 7, 0x5f7d8a);

    /* Nodes sit on the curve of the channel they act on; the selected one draws last. */
    for (int pass = 0; pass < 2; ++pass)
        for (int band = 1; band <= EQ_BANDS; ++band) {
            const float *b = v + EQ_FIRST + (band - 1) * EB_COUNT;
            int on = b[EB_ON] >= .5f, is_sel = band == selected;
            if (is_sel != pass || (!on && !is_sel)) continue;
            int k = hz_x(b[EB_FREQ]) - GX;
            k = k < 0 ? 0 : k >= GW ? GW - 1 : k;
            int x = GX + k, y = db_y((b[EB_STEREO] < .5f || b[EB_STEREO] > 1.5f ? main : side)[k]);
            uint32_t c = !on || b[EB_BYPASS] >= .5f ? 0x6b7a80 : band_color(band);
            if (is_sel) fx_disc(p, x, y, 5, 0xf2f6f4);
            fx_disc(p, x, y, is_sel ? 4 : 3, on ? c : bg);
            if (!on) fx_disc(p, x, y, 2, bg);
        }
}

static void eq2_header(uint32_t *p, int band, int page)
{
    static const char *const band_tabs[] = {"SHAPE", "MODE", "FILTER", "STEREO"};
    static const char *const global_tabs[] = {"LEVELS", "ROUTING"};
    static const char *const types[] = {"HPF", "LPF", "LO SHELF", "HI SHELF", "BELL"};
    uint32_t color = band ? band_color(band) : 0x77bcb0;
    char title[] = "BAND 01";
    title[5] = '0' + band / 10; title[6] = '0' + band % 10;
    rect(p, 8, 34, 304, 23, 0x101719);
    text(p, 12, 38, band ? title : "GLOBAL", 7, color);
    const float *v = eq2_link() ? eq2_values(NULL) : NULL;
    if (v && band) {
        int type = (int)lroundf(v[EQ_FIRST + (band - 1) * EB_COUNT + EB_TYPE]);
        if (type >= 0 && type <= 4) text(p, 44, 38, types[type], 8, 0x9fb0b5);
    }
    /* All 24 bands at a glance (plus G = Global): lit in their colour when on, outlined when selected. */
    for (int i = 0; i <= EQ_BANDS; ++i) {
        int x = 10 + 12 * i, on = 0;
        if (i && v) {
            const float *b = v + EQ_FIRST + (i - 1) * EB_COUNT;
            on = b[EB_ON] >= .5f && b[EB_BYPASS] < .5f;
        }
        uint32_t fill = i == 0 ? 0x2b3a40 : on ? band_color(i) : 0x1a2328;
        rect(p, x, 47, 11, 9, fill);
        if (i == band) frame(p, x - 1, 46, 13, 11, 0xf2f6f4);
        char num[3] = {i ? '0' + i / 10 : 'G', i ? '0' + i % 10 : 0, 0};
        text(p, x + (i ? 2 : 4), 49, num, 2, on ? 0x111a17 : 0x8a9aa0);
    }
    int tabs = band ? 4 : 2, w = 40, x0 = 312 - tabs * w;
    for (int i = 0; i < tabs; ++i) {
        const char *label = band ? band_tabs[i] : global_tabs[i];
        int x = x0 + i * w, active = i == page;
        rect(p, x, 35, w - 2, 10, active ? color : 0x242c33);
        text(p, x + (w - 2 - text_width(label, 8)) / 2, 38, label, 8, active ? 0x111a17 : 0xc9d6d6);
    }
}

static const uint32_t *draw_eq2(struct effect_page *page)
{
    const uint32_t *src = page->stock;
    uint32_t *p = page->rendered;
    const struct faceplate *face = &faceplates[HS_EQ2];
    const int rows[] = {57, 73, 89, 109, 125, 141};
    int bank = page->shifted ? 3 : 0;
    int sel_page, band = eq2_position(&sel_page);

    rect(p, 0, 0, W, H, face->metal);
    rack(p);
    for (int y = 4; y < 32; ++y)
        memcpy(p + y * W + 4, src + y * W + 4, 312 * sizeof *p);
    rect(p, 4, 3, 312, 1, face->accent);
    stock_name_display(p, src);
    eq2_header(p, band, sel_page);
    eq2_graph(p, band);

    /* The knobs are already on the hardware bar below; show their names and values only. */
    for (int i = 0; i < KNOBS; ++i) {
        int x = 8 + 103 * i, cx = x + 49;
        rect(p, x, 125, 98, 30, 0x192227);
        frame(p, x, 125, 98, 30, 0x465157);
        if (!row_has_label(src, rows[bank + i])) continue;
        rect(p, x + 1, 126, 96, 1, band ? band_color(band) : face->accent);
        center_ink(p, src, 50, 165, rows[bank + i], cx, 127);
        const char *marked = eq2_marking(i, page->values[i]);
        if (marked)
            text(p, cx - 2 * (int)strlen(marked), 146, marked, 13, 0xb7e0ce);
        else
            center_ink(p, src, 205, 309, rows[bank + i], cx, 140);
    }
    return p;
}
#endif
