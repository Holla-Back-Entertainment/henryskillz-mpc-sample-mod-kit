#ifndef HS_EQ2_VIEW_H
#define HS_EQ2_VIEW_H
/* Para EQ editor: response curve with band nodes, like the desktop plugin.
 * Included by hs_fx.c after hs_fx_faceplate.h. The curve comes from hs_eq2_response() in
 * hs-pro-fx-ladspa.so, i.e. the same biquads the audio runs, not a redrawn approximation. */
#include <link.h>

typedef int (*eq2_response_fn)(int, int, float *);
typedef const float *(*eq2_values_fn)(unsigned *);
static eq2_response_fn eq2_response;
static eq2_values_fn eq2_values;

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
    return eq2_response && eq2_values;
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

static const uint32_t band_colors[] = {0xe0503c, 0xd7dc5a, 0x45c16f, 0x3d8fe3, 0xec923c, 0xb56fdc};
static uint32_t band_color(int band) { return band_colors[(band - 1) % 6]; }

static void eq2_graph(uint32_t *p, int selected)
{
    const uint32_t bg = 0x0c1215, grid = 0x1f2a2f, zero = 0x3a4950;
    rect(p, GX - 1, GY - 1, GW + 2, GH + 2, bg);
    frame(p, GX - 1, GY - 1, GW + 2, GH + 2, 0x465259);
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
    int bypass = v[EQ_BYPASS] >= .5f;
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
    uint32_t curve = bypass ? 0x66767c : 0xe6eee9;
    for (int pass = stereo_split ? 0 : 1; pass < 2; ++pass) {
        const float *c = pass ? main : side;
        uint32_t color = pass ? curve : 0x5f7d8a;
        for (int k = 1; k < GW; ++k)
            fx_line(p, GX + k - 1, db_y(c[k - 1]), GX + k, db_y(c[k]), color);
    }
    if (stereo_split) text(p, GX + GW - 30, GY + 3, "R/S DIM", 7, 0x5f7d8a);
    if (bypass) text(p, GX + GW / 2 - 12, GY + 3, "BYPASS", 6, 0xc8a46a);

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
