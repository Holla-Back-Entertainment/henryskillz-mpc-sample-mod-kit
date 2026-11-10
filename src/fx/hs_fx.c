#define _GNU_SOURCE
#include <dlfcn.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum { W = 320, H = 160 };
#include "hs_console.h"
#include "hs_ducker_gain.h"
#include "hs_channel.h"
#include "hs_xsat.h"
#include "hs_bus.h"
#include "hs_eq2.h"

typedef void (*overlay_fn)(int, const void *, int);
typedef void (*overlay_rect_fn)(int, const void *, int, int, int, int);

static overlay_rect_fn next_rect;
static uintptr_t qol_base;

__attribute__((constructor)) static void locate_effect_state(void)
{
    void *map = dlsym(RTLD_NEXT, "fxmap_get_row");
    Dl_info info;
    if (map && dladdr(map, &info) && (uintptr_t)map - (uintptr_t)info.dli_fbase == 0x6d31c)
        qol_base = (uintptr_t)info.dli_fbase;
}

static const uint32_t orbit_red = 0xc8061c;
static const uint32_t cyan = 0x4cb8e7;
static const uint32_t rail = 0x555e62;

/* Return sites in the SHA-pinned SampQOL 0.2.0 overlay renderer. The shared
 * mode numbers alone also match menus and unrelated knob overlays. */
static uintptr_t return_site(const void *caller)
{
    Dl_info info;
    if (!dladdr(caller, &info) || !info.dli_fname) return 0;
    const char *name = strrchr(info.dli_fname, '/');
    if (strcmp(name ? name + 1 : info.dli_fname, "mpcqol.so")) return 0;
    return (uintptr_t)caller - (uintptr_t)info.dli_fbase;
}

static void shade(uint32_t *p, int x, int y, int w, int h, uint32_t color)
{
    for (int yy = y; yy < y + h; ++yy)
        for (int xx = x; xx < x + w; ++xx)
            if (!(p[yy * W + xx] & 0xffffff))
                p[yy * W + xx] = color;
}

static void fastener(uint32_t *p, int x, int y)
{
    rect(p, x, y, 3, 3, 0x373d3e);
    rect(p, x + 1, y, 1, 2, 0xa6a69b);
}

static void rack(uint32_t *p)
{
    frame(p, 0, 0, W, H, rail);
    rect(p, 1, 1, 2, H - 2, 0x252d30);
    rect(p, W - 3, 1, 2, H - 2, 0x252d30);
    fastener(p, 5, H - 7);
    fastener(p, W - 8, H - 7);
}

enum effect {
    OTHER, DUCKER, COMPRESSOR, WIDTH, TUBE_DRIVE, SOFT_CLIPPER, HS_SATURATOR, HS_AIR_EXCITER,
    DELAY, DIFF_DELAY, TAPE_DELAY, SAMPLE_DELAY, REVERB, SPRING_REVERB, FILTER, BUS_COMP,
    DUCKER_INPUT, LIMITER, PUMPER, TRANSIENT, GATE, AMP_SIM, ENSEMBLE, CHORUS, PHASER,
    FLANGER, WAH, AUTO_PAN, VINTAGE, VINYL, TAPE, AIR_FLAVOR, HS_CHANNEL, HS_XSAT, HS_BUS, HS_EQ2
};
enum { KNOBS = 3 };

struct effect_page {
    uint32_t stock[W * H];
    uint32_t rendered[W * H];
    enum effect kind;
    int shifted;
    int values[KNOBS];
    int main_values[KNOBS];
    int bypassed[KNOBS];
};

#include "hs_fx_faceplate.h"

static pthread_once_t effect_page_once = PTHREAD_ONCE_INIT;
static pthread_key_t effect_page_key;
static int effect_page_key_ready;

static void init_effect_page_key(void)
{
    effect_page_key_ready = pthread_key_create(&effect_page_key, free) == 0;
}

static struct effect_page *get_effect_page(int create)
{
    /* Static TLS would reserve both framebuffers on the stock sample-loader
     * thread's small stack. Allocate them only on threads drawing an FX editor. */
    pthread_once(&effect_page_once, init_effect_page_key);
    if (!effect_page_key_ready) return NULL;
    struct effect_page *page = pthread_getspecific(effect_page_key);
    if (!page && create) {
        page = calloc(1, sizeof *page);
        if (page && pthread_setspecific(effect_page_key, page)) {
            free(page);
            return NULL;
        }
    }
    return page;
}

static enum effect identify_effect(const uint32_t *p)
{
    /* Fingerprint only the orange title glyphs. Track and slot text is gray. */
    uint32_t hash = 2166136261u;
    for (int y = 8; y < 25; ++y)
        for (int x = 12; x < 152; ++x) {
            uint32_t c = p[y * W + x];
            int r = (c >> 16) & 255, g = (c >> 8) & 255, b = c & 255;
            hash = (hash ^ (r > 70 && r > b + 32 && r > g + 22)) * 16777619u;
        }
    switch (hash) {
    case 0xe111d189: return DUCKER;
    case 0x13a7a0b7: return COMPRESSOR;
    case 0xc7e2e20a: return WIDTH;
    case 0xaa0fcd4d: return TUBE_DRIVE;
    case 0xe82c045f: return SOFT_CLIPPER;
    case 0x671b315c: return HS_SATURATOR;
    case 0x7d9fd763: return HS_AIR_EXCITER;
    default: break;
    }
    if (!qol_base) return OTHER;
    /* Current insert UID in the SHA-pinned 0.2.0 renderer. Presets that
     * share an engine, including the three room sizes, keep their drawn title. */
    switch (__atomic_load_n((const uint32_t *)(qol_base + 0x16328f4), __ATOMIC_ACQUIRE)) {
    case 0x6146446c: return DELAY;
    case 0x61464444: return DIFF_DELAY;
    case 0x41435658: return TAPE_DELAY;
    case 0x534d444c: return SAMPLE_DELAY;
    case 0x61465276: return REVERB;
    case 0x61465370: return SPRING_REVERB;
    case 0x41435663: case 0x41435662: case 0x61465346: return FILTER;
    case 0x4143564a: return BUS_COMP;
    case 0x41435654: return DUCKER_INPUT;
    case 0x61464c6d: return LIMITER;
    case 0x61465075: return PUMPER;
    case 0x61465472: return TRANSIENT;
    case 0x61464e47: return GATE;
    case 0x61464153: return AMP_SIM;
    case 0x6146456e: return ENSEMBLE;
    case 0x61464d43: return CHORUS;
    case 0x61465073: return PHASER;
    case 0x6146466c: return FLANGER;
    case 0x41435679: return WAH;
    case 0x41435671: return AUTO_PAN;
    case 0x56494e54: return VINTAGE;
    case 0x56456678: return VINYL;
    case 0x54456678: return TAPE;
    case 0x485303: return AIR_FLAVOR;
    case HS_CHANNEL_ID: return HS_CHANNEL;
    case HS_XSAT_ID: return HS_XSAT;
    case HS_BUS_ID: return HS_BUS;
    case HS_EQ2_ID: return HS_EQ2;
    default: return OTHER;
    }
}

static int ink_peak(const uint32_t *p, int y)
{
    int peak = 0;
    for (int j = y; j < y + 13; ++j)
        for (int x = 50; x < 160; ++x) {
            int red = (p[j * W + x] >> 16) & 255;
            if (red > peak) peak = red;
        }
    return peak;
}

static void center_ink(uint32_t *dst, const uint32_t *src, int x0, int x1,
                       int y, int cx, int dy)
{
    enum { TEXT_HEIGHT = 13 };
    int first = x1, last = x0;
    for (int j = 0; j < TEXT_HEIGHT; ++j)
        for (int x = x0; x < x1; ++x)
            if (src[(y + j) * W + x] & 0xffffff) {
                if (x < first) first = x;
                if (x > last) last = x;
            }
    if (first > last || last - first + 1 > 94) return;
    int dx = cx - (last - first + 1) / 2;
    for (int j = 0; j < TEXT_HEIGHT; ++j)
        for (int x = first; x <= last; ++x) {
            uint32_t c = src[(y + j) * W + x];
            if (c & 0xffffff) dst[(dy + j) * W + dx + x - first] = c;
        }
}

static void large_knob(uint32_t *p, int cx, int cy, int value, int bypassed)
{
    for (int y = -17; y <= 17; ++y)
        for (int x = -17; x <= 17; ++x) {
            int radius = x * x + y * y;
            if (radius > 289) continue;
            uint32_t c = radius > 256 ? 0x080d10 :
                         radius > 196 ? (y < 0 ? 0x858d8b : 0x404a4d) :
                         radius > 144 ? 0x151c1f :
                         y < 0 ? 0x333d40 : 0x20282c;
            p[(cy + y) * W + cx + x] = c;
        }
    for (int i = 0; i < 9; ++i) {
        float angle = -2.3561945f + i * 0.5890486f;
        int dx = (int)lroundf(sinf(angle) * 20);
        int dy = (int)lroundf(cosf(angle) * 20);
        rect(p, cx + dx, cy - dy, 1, 1, i == 4 ? 0xabb5b6 : 0x616d71);
    }
    if (value < 0) return;
    float angle = -2.3561945f + (float)value * (4.712389f / 73);
    float dx = sinf(angle), dy = cosf(angle);
    for (int i = 6; i <= 13; ++i)
        rect(p, cx + (int)lroundf(dx * i), cy - (int)lroundf(dy * i),
             2, 2, bypassed ? 0x687579 : cyan);
}

static void name_display(uint32_t *p, enum effect kind)
{
    static const char *const titles[] = {
        [DUCKER] = "MOTHER DUCKER",
        [COMPRESSOR] = "AIR COMPRESSOR",
        [WIDTH] = "AIR STEREO WIDTH",
        [TUBE_DRIVE] = "TUBE DRIVE",
        [SOFT_CLIPPER] = "SOFT CLIPPER",
        [HS_SATURATOR] = "HS SATURATOR",
        [HS_AIR_EXCITER] = "HS AIR EXCITER"
    };
    const char *name = titles[kind];
    int inset = kind == TUBE_DRIVE || kind == SOFT_CLIPPER ? 10 : 7;
    int width = (int)strlen(name) * 8 + inset;
    rect(p, 8, 7, width, 22, 0x07160b);
    frame(p, 8, 7, width, 22, 0x586450);
    frame(p, 9, 8, width - 2, 20, 0x0b100b);
    rect(p, 10, 9, width - 4, 18, 0x0e2412);

    for (int i = 0; name[i]; ++i) {
        uint16_t bits = glyph(name[i]);
        for (int y = 0; y < 5; ++y) {
            unsigned row = (bits >> (3 * y)) & 7u;
            for (int x = 0; x < 3; ++x) {
                int px = 13 + i * 8 + x * 2;
                int py = 13 + y * 2;
                if (row & (4u >> x)) {
                    rect(p, px, py, 2, 2, 0x427a33);
                    rect(p, px, py, 1, 1, 0xa6ee75);
                } else {
                    rect(p, px, py, 1, 1, 0x1a3519);
                }
            }
        }
    }
}

static int is_title_pixel(uint32_t c)
{
    int red = (c >> 16) & 255, green = (c >> 8) & 255, blue = c & 255;
    return red > 70 && red > blue + 32 && red > green + 22;
}

static void stock_name_display(uint32_t *p, const uint32_t *src)
{
    int end = 0;
    for (int y = 8; y < 25; ++y)
        for (int x = 12; x < 304; ++x)
            if (is_title_pixel(src[y * W + x]) && x > end) end = x;
    if (end < 13) return;
    for (int y = 6; y < 30; ++y) {
        for (int x = 8; x <= end + 4; ++x) {
            uint32_t pixel = src[y * W + x];
            int glyph = y >= 8 && y < 25 && x >= 12 && x <= end && is_title_pixel(pixel);
            unsigned level = (pixel >> 16) & 255;
            uint32_t color = level > 150 ? ((x+y)%2 ? 0x88d361 : 0xa9ed7b) : 0x4a7942;
            p[y * W + x] = glyph ? color : 0x112519;
        }
    }
    frame(p, 8, 6, end - 3, 24, 0x5a6855);
    frame(p, 9, 7, end - 5, 22, 0x0b110c);
}

static int row_has_label(const uint32_t *src, int y)
{
    for (int j = y; j < y + 13; ++j)
        for (int x = 48; x < 166; ++x)
            if (src[j * W + x] & 0xffffff) return 1;
    return 0;
}

static int is_graph_editor(enum effect kind)
{
    return kind == FILTER || kind == LIMITER;
}

#include "hs_eq2_view.h"

static const uint32_t *draw_stock_rack(struct effect_page *page)
{
    if (page->kind == HS_EQ2) return draw_eq2(page);
    const uint32_t *src = page->stock;
    uint32_t *p = page->rendered;
    const struct faceplate *face = &faceplates[page->kind];
    const int rows[] = {57, 73, 89, 109, 125, 141};
    int bank = page->shifted ? 3 : 0;

    if (is_graph_editor(page->kind)) {
        /* Stock owns the analyzer and Limiter meters, including idle state. */
        memcpy(p, src, sizeof page->stock);
        stock_name_display(p, src);
        rect(p, 4, 3, 312, 1, face->accent);
        return p;
    }

    rect(p, 0, 0, W, H, face->metal);
    rack(p);
    for (int y = 4; y < 32; ++y)
        memcpy(p + y * W + 4, src + y * W + 4, 312 * sizeof *p);
    rect(p, 4, 3, 312, 1, face->accent);
    stock_name_display(p, src);
    if (page->kind == AIR_FLAVOR)
        rect(p, 8, 30, 304, 2, 0x4caba9);

    if (page->kind == DUCKER_INPUT) {
        rect(p, 8, 34, 304, 42, 0x141719);
        frame(p, 8, 34, 304, 42, 0x6d5641);
        for (int y = 36; y < 49; ++y)
            for (int x = 12; x < 306; ++x)
                if (src[y * W + x] & 0xffffff) p[y * W + x] = src[y * W + x];
        for (int x = 22; x < 291; x += 9) rect(p, x, 62, 5, 1, 0x584431);
        fx_disc(p, 22, 62, 4, face->accent);
        fx_disc(p, 295, 62, 4, face->accent);
    } else {
        fx_faceplate_art(p, page);
    }

    for (int i = 0; i < KNOBS; ++i) {
        int x = 8 + 103 * i, cx = x + 49;
        int flavor = page->kind == AIR_FLAVOR;
        rect(p, x, 81, 98, 72, flavor ? 0x444749 : 0x192227);
        frame(p, x, 81, 98, 72, flavor ? 0x9e9984 : 0x465157);
        if (!row_has_label(src, rows[bank + i])) {
            rect(p, x+15, 116, 68, 1, 0x344147);
            continue;
        }
        rect(p, x+1, 82, 96, 1, face->accent);
        fastener(p, x+4, 86);
        center_ink(p, src, 50, 165, rows[bank+i], cx, 84);
        if (flavor) {
            static int (*power_module)(void);
            if (!power_module) power_module = (int (*)(void))dlsym(RTLD_DEFAULT, "hs_flavor_power_module");
            int module = i == 0 && !page->shifted && power_module ? power_module() : -1;
            flavor_control(p, cx, page->values[i], module, page->bypassed[i]);
        } else if (page->kind == HS_EQ2) {
            rack_knob(p,cx,116,page->values[i],page->bypassed[i],eq2_color());
        } else if (page->kind == HS_BUS) {
            static const uint32_t accents[] = {0xb5bcc2,0x7ba5c1,0xa6bf9e};
            rack_knob(p, cx, 116, page->values[i], page->bypassed[i], accents[bus_section(NULL)]);
        } else if (page->kind == HS_XSAT) {
            rack_knob(p, cx, 116, page->values[i], page->bypassed[i], 0xd3a164);
        } else if (page->kind == HS_CHANNEL) {
            channel_knob(p, cx, 116, page->values[i], page->bypassed[i]);
        } else {
            large_knob(p, cx, 115, page->values[i], page->bypassed[i]);
        }
        rect(p, x+5, 137, 88, 14, 0x0a0f12);
        frame(p, x+5, 137, 88, 14, 0x323d43);
        const char *marked = page->kind == HS_BUS ? bus_marking(i, page->values[i])
            : page->kind == HS_EQ2 ? eq2_marking(i,page->values[i]) : NULL;
        if (marked)
            text(p, cx - 2*(int)strlen(marked), 142, marked, 13, 0xb7e0ce);
        else
            center_ink(p, src, 205, 309, rows[bank+i], cx, 137);
    }
    return p;
}

static int has_shift_bank(enum effect kind)
{
    return kind != TUBE_DRIVE && kind != HS_SATURATOR && kind != HS_AIR_EXCITER;
}

static const uint32_t *draw_gear(struct effect_page *page)
{
    if (page->kind >= DELAY) return draw_stock_rack(page);
    const uint32_t *src = page->stock;
    uint32_t *p = page->rendered;
    const int rows[] = {57, 73, 89, 109, 125, 141};
    int bank = page->shifted ? 3 : 0;
    int alt = page->shifted ? 0 : 3;
    static const uint32_t accents[] = {
        [DUCKER] = 0xd58d39, [COMPRESSOR] = 0x6fada6, [WIDTH] = 0x648db8,
        [TUBE_DRIVE] = 0xbc8a52, [SOFT_CLIPPER] = 0xa7839c,
        [HS_SATURATOR] = 0xc99054, [HS_AIR_EXCITER] = 0x71bfb3
    };
    uint32_t accent = accents[page->kind];
    int has_shift = has_shift_bank(page->kind);
    int panel_height = has_shift ? 77 : 103;
    int readout_y = has_shift ? 112 : 127;

    rect(p, 0, 0, W, H, 0x11171b);
    rack(p);
    for (int y = 4; y < 32; ++y)
        memcpy(p + y * W + 4, src + y * W + 4, 312 * sizeof *p);
    rect(p, 4, 3, 312, 1, orbit_red);
    name_display(p, page->kind);
    if (page->kind == DUCKER) {
        for (int y = 36; y < 49; ++y)
            memcpy(p + y * W + 12, src + y * W + 12, 296 * sizeof *p);
        frame(p, 203, 37, 48, 12, 0x88735b);
        for (int x = 211; x < 247; x += 5) rect(p, x, 40, 1, 6, 0x0d1012);
    } else {
        rect(p, 8, 39, 304, 3, 0x343f45);
        rect(p, 8, 39, 34, 3, accent);
        fastener(p, 303, 39);
    }
    for (int i = 0; i < KNOBS; ++i) {
        int x = 8 + 103 * i;
        int cx = x + 49;
        rect(p, x, 53, 98, panel_height, 0x192227);
        frame(p, x, 53, 98, panel_height, 0x465157);
        rect(p, x + 1, 54, 96, 1, accent);
        fastener(p, x + 4, 58);
        fastener(p, x + 91, 58);
        if (page->kind == HS_AIR_EXCITER && i == 0)
            text(p, cx - 13, 61, "FREQ HZ", 7, 0xc6d4db);
        else
            center_ink(p, src, 50, 165, rows[bank + i], cx, 57);
        large_knob(p, cx, has_shift ? 89 : 97, page->values[i], page->bypassed[i]);
        rect(p, x + 12, readout_y, 74, 14, 0x0a0f12);
        frame(p, x + 12, readout_y, 74, 14, 0x323d43);
        center_ink(p, src, 207, 309, rows[bank + i], cx, readout_y);
        if (!has_shift) continue;

        rect(p, x, 134, 98, 22, 0x0d151a);
        frame(p, x, 134, 98, 22, 0x344147);
        center_ink(p, src, 50, 165, rows[alt + i], cx, 132);
        center_ink(p, src, 207, 309, rows[alt + i], cx, 143);
    }
    return p;
}

static const void *effect_overlay(const uint32_t *p)
{
    enum effect kind = identify_effect(p);
    struct effect_page *page = get_effect_page(kind != OTHER);
    if (!page) return p;
    if (kind == OTHER) {
        page->kind = OTHER;
        return p;
    }
    int main = 0, alternate = 0;
    for (int i = 0; i < KNOBS; ++i) {
        int main_ink = ink_peak(p, 57 + 16*i);
        int alt_ink = ink_peak(p, 109 + 16*i);
        if (main_ink > main) main = main_ink;
        if (alt_ink > alternate) alternate = alt_ink;
    }
    int shifted = has_shift_bank(kind) && !is_graph_editor(kind) && alternate > main;
    if (page->kind != kind)
        for (int i = 0; i < KNOBS; ++i) page->main_values[i] = -1;
    if (page->kind != kind || page->shifted != shifted)
        for (int i = 0; i < KNOBS; ++i) page->values[i] = -1;
    page->kind = kind;
    page->shifted = shifted;
    memcpy(page->stock, p, sizeof page->stock);
    return draw_gear(page);
}

static void insert_body(uint32_t *p)
{
    rack(p);
    for (int i = 0; i < 4; ++i) {
        int y = 10 + i * 26;
        shade(p, 5, y, 310, 24, (i % 2) ? 0x162024 : 0x20282c);
        rect(p, 5, y + 23, 310, 1, 0x465256);
        fastener(p, 306, y + 4);
    }
}

static void selector_rows(uint32_t *p, int offset, int height)
{
    rect(p, 0, 0, 3, height, rail);
    rect(p, W - 3, 0, 3, height, rail);
    for (int row = 0; row < 7; ++row) {
        int top = 4 + 26 * row - offset;
        int end = top + 26;
        if (end <= 0 || top >= height) continue;
        int start = top < 0 ? 0 : top;
        int stop = end < height ? end : height;
        shade(p, 4, start, 312, stop - start, (row % 2) ? 0x131d23 : 0x1b252a);
        if (end <= height) rect(p, 4, end - 1, 312, 1, 0x38464d);
        if (top >= 0 && top + 7 < height) fastener(p, 306, top + 5);
    }
}

static void header(uint32_t *p)
{
    shade(p, 0, 2, W, 29, 0x141b1f);
    rect(p, 0, 0, W, 2, orbit_red);
    rect(p, 0, 30, W, 1, rail);
}

static void effect_knobs(uint32_t *p)
{
    struct effect_page *page = get_effect_page(0);
    if (!page || page->kind == OTHER || is_graph_editor(page->kind)) return;
    const int bars[] = {15, 121, 228};
    int changed = 0;
    for (int i = 0; i < KNOBS; ++i) {
        int left = bars[i];
        int filled = 0, width = 0, bypassed = 0;
        for (int x = left; x < left + 74; ++x) {
            uint32_t c = p[31 * W + x] & 0xffffff;
            if (c == 0xffffff || c == 0x808080) ++filled;
            if (c == 0x808080) bypassed = 1;
            if (c == 0xffffff || c == 0x808080 || c == 0x45454d) ++width;
        }
        int value = width == 74 ? (filled > 0 ? filled - 1 : 0) : -1;
        if (page->values[i] != value || page->bypassed[i] != bypassed) {
            changed = 1;
            page->values[i] = value;
            if (!page->shifted) page->main_values[i] = value;
            page->bypassed[i] = bypassed;
        }
        if (value < 0) continue;
        shade(p, left - 3, 9, 83, 28, 0x151d21);
        rect(p, left, 30, 74, 4, 0x151d21);
        rect(p, left, 30, 74, 1, 0x434e55);
        rect(p, left, 30, filled, 1, bypassed ? 0x687579 : cyan);
    }
    if (page->kind == HS_EQ2) {
        /* The audio thread publishes new settings after the bar moves; redraw when it lands. */
        static unsigned drawn;
        unsigned seq = eq2_seq();
        if (seq != drawn) { drawn = seq; changed = 1; }
    }
    if (changed) {
        if (!next_rect)
            next_rect = (overlay_rect_fn)dlsym(RTLD_NEXT, "overlay_update_rect");
        if (next_rect) next_rect(27, draw_gear(page), 0, 40, W, H);
    }
}

void overlay_update(int mode, const void *buf, int flag)
{
    static overlay_fn next;
    if (!next) next = (overlay_fn)dlsym(RTLD_NEXT, "overlay_update");
    if (buf && flag && (mode >= 26 && mode <= 28)) {
        uintptr_t site = return_site(__builtin_return_address(0));
        if (mode == 27 && (site == 0xac2f0 || site == 0xad460)) {
            struct effect_page *page = get_effect_page(0);
            if (page) page->kind = OTHER;
        }
        if (mode == 26 && (site == 0xac684 || site == 0xad644 || site == 0xabc60)) {
            uint32_t *p = (uint32_t *)buf;
            header(p);
            int gain = site == 0xad644 ? hs_ducker_gain_page() : -1;
            if (gain >= 0) {
                rect(p, 124, 4, 71, 20, gain ? 0x15424d : 0x1d272b);
                frame(p, 124, 4, 71, 20, gain ? cyan : rail);
                text(p, 146, 12, "MAKE-UP", 7, gain ? 0xc9f5ff : 0xa6b6bb);
            }
        } else if (mode == 27 && (site == 0xad9e8 || site == 0xab89c)) {
            uint32_t *p = (uint32_t *)buf;
            stock_name_display(p, p);
            if (site == 0xab89c) {
                frame(p, 2, 0, 316, 159, rail);
                fastener(p, 5, 150);
                fastener(p, 312, 150);
            }
        } else if (mode == 27 && site == 0xac2f0)
            insert_body((uint32_t *)buf);
        else if (mode == 27 && site == 0xada58)
            buf = effect_overlay(buf);
        else if (mode == 27 && site == 0xad460)
            selector_rows((uint32_t *)buf, 0, H);
        else if (mode == 28 && site == 0xad4ac)
            selector_rows((uint32_t *)buf, H, 40);
        else if (mode == 28 && site == 0xad698)
            effect_knobs((uint32_t *)buf);
    }
    if (next) next(mode, buf, flag);
}

void overlay_update_rect(int mode, const void *buf, int x, int y, int w, int h)
{
    if (!next_rect) next_rect = (overlay_rect_fn)dlsym(RTLD_NEXT, "overlay_update_rect");
    if (buf && mode == 27 && x == 0 && y == 40 && w == W && h == H) {
        uintptr_t site = return_site(__builtin_return_address(0));
        if (site == 0xaca6c)
            buf = effect_overlay(buf);
        else if (site == 0xabb98)
            stock_name_display((uint32_t *)buf, buf);
    }
    if (buf && mode == 28 && x == 0 && y == 200 && w == W && h == 40 &&
        return_site(__builtin_return_address(0)) == 0xad93c)
        effect_knobs((uint32_t *)buf);
    if (next_rect) next_rect(mode, buf, x, y, w, h);
}
