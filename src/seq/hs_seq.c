/* hs_seq.c: the stock SEQ page redrawn as a mixing-desk panel (Henry's mock v5, 12 Nov 2026).
 *
 * How: interpose drmModePageFlip (like warp_guide / hs-stock). When the frame about to be shown is
 * the stock SEQ page, its own pixels are cut out region by region and pasted into the desk layout,
 * so every value, SHIFT label and transport state stays exactly what the stock app drew.
 *
 * The restyled frame goes into buffers of our own; the app's buffers are only read.
 * The VU and OUT LEDs read the app's own audio output (snd_pcm_writei/writen, hooked below) and a
 * small thread repaints just those two meters on the shown buffer, so they move between app redraws. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "dsb_logo.h"

enum { W = 320, H = 240, N = W * H };

/* ---------- colours (0xRRGGBB) ---------- */
#define PANEL  0x3a4046u
#define PANEL2 0x2c3136u
#define LCD    0x080b0du
#define EDGE   0x586066u
#define DARK   0x14161au
#define CREAM  0xeee4c4u
#define INK    0x22201cu
#define TXT    0xe8eae6u
#define DIM    0x98a0a4u
#define RED    0xd83a30u
#define GRN    0x3cc058u
#define YEL    0xe8c83au
#define AMB    0xffb030u
#define BLUE   0x3a78d0u
#define STOCK_GREY 0x2e2e33u

static uint32_t mix(uint32_t a, uint32_t b, unsigned t) /* t 0..255: a -> b */
{
    uint32_t r = 0;
    for (int sh = 0; sh <= 16; sh += 8) {
        unsigned x = (a >> sh) & 255, y = (b >> sh) & 255;
        r |= ((x * (255 - t) + y * t) / 255) << sh;
    }
    return r;
}
static unsigned lum(uint32_t c) { unsigned r = c >> 16 & 255, g = c >> 8 & 255, b = c & 255; return r > g ? (r > b ? r : b) : (g > b ? g : b); }

static void rect(uint32_t *d, int x0, int y0, int x1, int y1, uint32_t c)
{
    for (int y = y0 < 0 ? 0 : y0; y <= y1 && y < H; ++y)
        for (int x = x0 < 0 ? 0 : x0; x <= x1 && x < W; ++x) d[y * W + x] = c;
}
static void box(uint32_t *d, int x0, int y0, int x1, int y1, uint32_t fill)
{
    rect(d, x0, y0, x1, y1, EDGE);
    rect(d, x0 + 1, y0 + 1, x1 - 1, y1 - 1, fill);
}
static void plot(uint32_t *d, int x, int y, uint32_t c) { if (x >= 0 && x < W && y >= 0 && y < H) d[y * W + x] = c; }
static void line(uint32_t *d, float x0, float y0, float x1, float y1, uint32_t c)
{
    int n = (int)(fmaxf(fabsf(x1 - x0), fabsf(y1 - y0)) + 1);
    for (int i = 0; i <= n; ++i) plot(d, (int)lrintf(x0 + (x1 - x0) * i / n), (int)lrintf(y0 + (y1 - y0) * i / n), c);
}
static void disc(uint32_t *d, float cx, float cy, float r, uint32_t c)
{
    for (int y = (int)(cy - r); y <= (int)(cy + r) + 1; ++y)
        for (int x = (int)(cx - r); x <= (int)(cx + r) + 1; ++x)
            if ((x - cx) * (x - cx) + (y - cy) * (y - cy) <= r * r) plot(d, x, y, c);
}

/* copy a stock region. key: background colour to skip (or -1 for none). tint: recolour by
   brightness over the destination (or 0 to keep the stock colours). */
static void paste(uint32_t *d, const uint32_t *s, int sx0, int sy0, int sx1, int sy1, int dx, int dy, long key, uint32_t tint)
{
    for (int y = sy0; y <= sy1; ++y)
        for (int x = sx0; x <= sx1; ++x) {
            uint32_t c = s[y * W + x] & 0xffffff;
            if (key >= 0 && c == (uint32_t)key) continue;
            int X = dx + x - sx0, Y = dy + y - sy0;
            if (X < 0 || X >= W || Y < 0 || Y >= H) continue;
            d[Y * W + X] = tint ? mix(d[Y * W + X], tint, lum(c)) : c;
        }
}

/* paste a stock value centred in a window [wx0,wx1]; squeeze it sideways (keeping the brightest
   pixel of each squeezed span, so thin strokes survive) when the stock text is wider. */
static void paste_fit(uint32_t *d, const uint32_t *s, int sx0, int sx1, int sy0, int sy1, int wx0, int wx1, int dy)
{
    int bx0 = sx1, bx1 = sx0;
    for (int y = sy0; y <= sy1; ++y) for (int x = sx0; x <= sx1; ++x)
        if (lum(s[y * W + x]) > 40) { if (x < bx0) bx0 = x; if (x > bx1) bx1 = x; }
    if (bx1 < bx0) return;
    int w = bx1 - bx0 + 1, room = wx1 - wx0 + 1, out = w < room ? w : room, x0 = wx0 + (room - out) / 2;
    for (int y = sy0; y <= sy1; ++y)
        for (int j = 0; j < out; ++j) {
            int a = bx0 + j * w / out, b = bx0 + (j + 1) * w / out;
            uint32_t best = 0;
            for (int x = a; x < (b > a ? b : a + 1); ++x) if (lum(s[y * W + x]) > lum(best)) best = s[y * W + x] & 0xffffff;
            if (best) d[(dy + y - sy0) * W + x0 + j] = best;
        }
}

/* ---------- 5x7 font for the meter print (column-major, bit 0 = top) ---------- */
static const unsigned char *glyph(char c)
{
    static const struct { char c; unsigned char b[5]; } T[] = {
        {'0',{0x3E,0x51,0x49,0x45,0x3E}},{'1',{0x00,0x42,0x7F,0x40,0x00}},{'2',{0x42,0x61,0x51,0x49,0x46}},
        {'3',{0x21,0x41,0x45,0x4B,0x31}},{'5',{0x27,0x45,0x45,0x45,0x39}},{'+',{0x08,0x08,0x3E,0x08,0x08}},
        {'V',{0x1F,0x20,0x40,0x20,0x1F}},{'U',{0x3F,0x40,0x40,0x40,0x3F}},{'L',{0x7F,0x40,0x40,0x40,0x40}},
        {'R',{0x7F,0x09,0x19,0x29,0x46}},{'O',{0x3E,0x41,0x41,0x41,0x3E}},{'T',{0x01,0x01,0x7F,0x01,0x01}},
    };
    static const unsigned char sp[5];
    for (unsigned i = 0; i < sizeof T / sizeof *T; ++i) if (T[i].c == c) return T[i].b;
    return sp;
}
static void text(uint32_t *d, int x, int y, const char *s, uint32_t c, int sc)
{
    for (; *s; ++s, x += 6 * sc) {
        const unsigned char *g = glyph(*s);
        for (int cx = 0; cx < 5; ++cx) for (int ry = 0; ry < 7; ++ry)
            if (g[cx] >> ry & 1) rect(d, x + cx * sc, y + ry * sc, x + cx * sc + sc - 1, y + ry * sc + sc - 1, c);
    }
}
static int text_w(const char *s, int sc) { return (int)strlen(s) * 6 * sc - sc; }

/* ---------- desk parts ---------- */
static void vu(uint32_t *d, int x0, int y0, int x1, int y1, float level) /* level 0..1 across the scale */
{
    box(d, x0, y0, x1, y1, DARK);
    int fx0 = x0 + 3, fy0 = y0 + 3, fx1 = x1 - 3, fy1 = y1 - 3;
    rect(d, fx0, fy0, fx1, fy1, CREAM);
    float w = fx1 - fx0, cx = fx0 + w / 2, cy = fy1 + w * .32f, r = w * .62f;
    #define ANG(f) ((-90.f - 38.f + 76.f * (f)) * 3.14159265f / 180.f)
    for (int i = 0; i < 120; ++i) {
        float f = i / 120.f, a = ANG(f), red = f >= .82f;
        for (float k = 0; k < (red ? 3 : 1); ++k) plot(d, (int)(cx + (r - k) * cosf(a)), (int)(cy + (r - k) * sinf(a)), red ? RED : INK);
    }
    static const struct { float f; const char *l; } m[] = {
        {0,"20"},{.2f,"10"},{.32f,0},{.44f,"5"},{.58f,"3"},{.66f,0},{.74f,0},{.82f,"0"},{.88f,0},{.94f,0},{1,"+3"}};
    for (unsigned i = 0; i < sizeof m / sizeof *m; ++i) {
        float a = ANG(m[i].f); uint32_t c = m[i].f > .82f ? RED : INK;
        line(d, cx + r * cosf(a), cy + r * sinf(a), cx + (r + 4) * cosf(a), cy + (r + 4) * sinf(a), c);
        if (m[i].l) text(d, (int)(cx + (r + 9) * cosf(a)) - text_w(m[i].l, 1) / 2, (int)(cy + (r + 9) * sinf(a)) - 4, m[i].l, c, 1);
    }
    text(d, (int)cx - text_w("VU", 2) / 2, fy0 + (int)((fy1 - fy0) * .45f), "VU", INK, 2);
    float a = ANG(level);
    rect(d, fx0, fy1 - 4, fx1, fy1, 0x1c1c1c);
    for (float t = 0; t <= 1; t += .004f) {          /* needle, clipped to the face */
        float px = cx + t * (r + 3) * cosf(a), py = cy + t * (r + 3) * sinf(a);
        if (px >= fx0 && px <= fx1 && py >= fy0 && py < fy1 - 4) plot(d, (int)px, (int)py, INK);
    }
    #undef ANG
}

static void leds(uint32_t *d, int x0, int y0, int x1, int y1, float l, float r)
{
    box(d, x0, y0, x1, y1, DARK);
    text(d, x0 + 4, y0 + 4, "L", DIM, 1); text(d, x0 + 4, y0 + 16, "R", DIM, 1);
    int n = (x1 - x0 - 16) / 3;
    for (int ch = 0; ch < 2; ++ch)
        for (int s = 0; s < n; ++s) {
            float f = (float)s / (n - 1);
            uint32_t c = f > .88f ? RED : f > .7f ? YEL : GRN;
            if (f > (ch ? r : l)) c = mix(c, 0, 200);
            int x = x0 + 12 + s * 3, y = y0 + 4 + ch * 12;
            rect(d, x, y, x + 1, y + 7, c);
        }
}

static void knob(uint32_t *d, int cx, int cy, uint32_t cap)
{
    for (int k = 0; k < 11; ++k) {
        float a = (135 + 27 * k) * 3.14159265f / 180;
        plot(d, (int)lrintf(cx + 10 * cosf(a)), (int)lrintf(cy + 10 * sinf(a)), DIM);
    }
    disc(d, cx, cy, 8, 0x181a1c);
    disc(d, cx, cy, 6.5f, 0x2a2d30);
    disc(d, cx, cy, 3.5f, cap);
    line(d, cx, cy - 3, cx, cy - 6, TXT);               /* endless encoder: pointer up */
}

static void logo(uint32_t *d, int x0, int y0)
{
    for (int y = 0; y < LOGO_H; ++y) for (int x = 0; x < LOGO_W; ++x)
        if (logo_a[y][x]) { int i = (y0 + y) * W + x0 + x; d[i] = mix(d[i], TXT, logo_a[y][x]); }
}

/* ---------- levels (written by the audio thread, read by the UI) ----------
   One writer per value, aligned 32-bit floats: plain stores and loads are enough on aarch64. */
volatile float hs_seq_ms[2], hs_seq_pk[2];      /* mean square (~80 ms), peak with a 20 dB/s fall */
#define VU_REF_DBFS (-18.f)                     /* ponytail: 0 VU = -18 dBFS RMS, the usual digital line-up; tune here */

static float vu_pos(void)                       /* needle position 0..1 across the printed scale */
{
    static const float db[] = {-20,-10,-7,-5,-3,-2,-1,0,1,2,3}, f[] = {0,.2f,.32f,.44f,.58f,.66f,.74f,.82f,.88f,.94f,1};
    float ms = .5f * (hs_seq_ms[0] + hs_seq_ms[1]), v = 10 * log10f(ms + 1e-12f) - VU_REF_DBFS;
    if (v <= db[0]) return 0;
    for (int i = 1; i < 11; ++i) if (v <= db[i]) return f[i-1] + (f[i] - f[i-1]) * (v - db[i-1]) / (db[i] - db[i-1]);
    return 1.04f;                               /* pinned just past +3 */
}
static float led_pos(int ch) { float d = 20 * log10f(hs_seq_pk[ch] + 1e-9f); return fminf(1, fmaxf(0, (d + 48) / 48)); }

static void meters(uint32_t *d)
{
    vu(d, 6, 36, 150, 100, vu_pos());
    leds(d, 252, 72, 314, 102, led_pos(0), led_pos(1));
}

/* ---------- the page ---------- */
int hs_seq_is_seq(const uint32_t *s)
{
    static const int grey[][2] = {{18,134},{301,134},{18,182},{301,182},{100,140},{100,178}};
    static const int black[][2] = {{8,120},{310,120},{160,45},{5,45},{160,124},{5,200},{160,200},{316,150},{3,150}};
    for (unsigned i = 0; i < sizeof grey / sizeof *grey; ++i) if ((s[grey[i][1] * W + grey[i][0]] & 0xffffff) != STOCK_GREY) return 0;
    for (unsigned i = 0; i < sizeof black / sizeof *black; ++i) if (s[black[i][1] * W + black[i][0]] & 0xffffff) return 0;
    for (int y = 54; y <= 76; ++y) for (int x = 17; x <= 57; ++x) {      /* the yellow "Seq" */
        uint32_t c = s[y * W + x];
        if ((c >> 16 & 255) > 200 && (c >> 8 & 255) > 140 && (c & 255) < 60) return 1;
    }
    return 0;
}

void hs_seq_draw(const uint32_t *s, uint32_t *d)
{
    rect(d, 0, 0, W - 1, H - 1, PANEL);
    memcpy(d, s, 4 * W * sizeof *d);                                  /* stock position bar, rows 0-3 */

    rect(d, 0, 4, W - 1, 31, PANEL2); rect(d, 0, 32, W - 1, 32, 0x1a1d20);  /* header */
    logo(d, 4, 6);
    box(d, 70, 5, 314, 30, CREAM);
    paste(d, s, 17, 54, 248, 76, 76, 6, 0, INK);                      /* Seq A01  Sequence 01 */

    box(d, 156, 36, 314, 68, LCD);
    paste(d, s, 28, 9, 78, 36, 162, 39, 0, GRN);                      /* tempo */
    paste(d, s, 117, 12, 200, 33, 228, 42, 0, AMB);                   /* BPM: SEQ (box border left out) */
    box(d, 156, 72, 248, 102, DARK);
    paste(d, s, 221, 9, 310, 36, 158, 74, -1, 0);                     /* Rec Quant / Rename, as stock */
    meters(d);

    box(d, 6, 105, 314, 131, LCD);
    paste(d, s, 12, 90, 308, 114, 12, 106, 0, 0);                     /* 4/4, metronome, count-in */

    box(d, 6, 135, 314, 187, LCD);
    paste(d, s, 16, 132, 303, 184, 16, 135, STOCK_GREY, 0);           /* transport + counter */

    /* the stock values sit in thirds of the bottom row; long ones ("16 Bars", "1/16T") get squeezed */
    static const struct { int x0, x1, sx0, sx1; uint32_t cap; } k[3] = {
        {4, 104, 2, 106, BLUE}, {108, 208, 108, 212, GRN}, {212, 316, 214, 306, RED}};
    for (int i = 0; i < 3; ++i) {
        box(d, k[i].x0, 192, k[i].x1, 236, PANEL);
        knob(d, k[i].x0 + 11, 214, k[i].cap);
        box(d, k[i].x0 + 22, 199, k[i].x1 - 3, 230, LCD);
        paste_fit(d, s, k[i].sx0, k[i].sx1, 207, 236, k[i].x0 + 24, k[i].x1 - 5, 200);
    }
    paste(d, s, 309, 226, 316, 237, 309, 226, 0, 0);                  /* SHIFT arrow */
}

#ifndef HS_SEQ_HOST
/* ---------- drmModePageFlip ----------
   The app's buffers are never written: on the SEQ page we draw into two buffers of our own and
   flip those instead, so the app (and its partial redraws) always works on pristine stock frames.
   SampQOL's flip, next in line, still draws its overlays on top of whichever buffer is shown. */
struct fbcmd { uint32_t fb_id, width, height, pitch, bpp, depth, handle; };
struct mapdumb { uint32_t handle, pad; uint64_t offset; };
struct mkdumb { uint32_t height, width, bpp, flags, handle, pitch; uint64_t size; };
#define GETFB       _IOWR('d', 0xAD, struct fbcmd)
#define ADDFB       _IOWR('d', 0xAE, struct fbcmd)
#define CREATE_DUMB _IOWR('d', 0xB2, struct mkdumb)
#define MAP_DUMB    _IOWR('d', 0xB3, struct mapdumb)
/* The panel is SPI (st7796s): pixels written to a shown buffer reach the glass only on a flip or a
   dirty call, so the meter thread must send one (SampQOL does the same for its moving screens). */
struct cliprect { uint16_t x1, y1, x2, y2; };
struct dirtycmd { uint32_t fb_id, flags, color, num_clips; uint64_t clips_ptr; };
#define DIRTYFB     _IOWR('d', 0xB1, struct dirtycmd)
typedef int (*flip_fn)(int, uint32_t, uint32_t, uint32_t, void *);

struct map { uint32_t id, stride; uint32_t *px; };

static uint32_t *map_handle(int fd, uint32_t handle, size_t len)
{
    struct mapdumb m = { .handle = handle };
    if (ioctl(fd, MAP_DUMB, &m)) return NULL;
    void *p = mmap(0, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, m.offset);
    return p == MAP_FAILED ? NULL : p;
}

static struct map *app_fb(int fd, uint32_t id)
{
    static struct map cache[8];
    for (int i = 0; i < 8; ++i) if (cache[i].id == id && cache[i].px) return &cache[i];
    struct fbcmd f = { .fb_id = id };
    if (ioctl(fd, GETFB, &f) || f.bpp != 32 || f.width != W || f.height != H || !f.handle) return NULL;
    uint32_t *p = map_handle(fd, f.handle, (size_t)f.pitch * H);
    if (!p) return NULL;
    for (int i = 0; i < 8; ++i)
        if (!cache[i].px) { cache[i] = (struct map){ id, f.pitch / 4, p }; return &cache[i]; }
    munmap(p, (size_t)f.pitch * H);
    return NULL;
}

static struct map *own_fb(int fd)                      /* two buffers of ours, used in turn */
{
    static struct map own[2];
    static int ready, next;
    if (!ready) {
        for (int i = 0; i < 2; ++i) {
            struct mkdumb c = { .height = H, .width = W, .bpp = 32 };
            if (ioctl(fd, CREATE_DUMB, &c)) return NULL;
            struct fbcmd f = { .width = W, .height = H, .pitch = c.pitch, .bpp = 32, .depth = 24, .handle = c.handle };
            if (ioctl(fd, ADDFB, &f)) return NULL;
            uint32_t *p = map_handle(fd, c.handle, c.size);
            if (!p) return NULL;
            own[i] = (struct map){ f.fb_id, c.pitch / 4, p };
        }
        ready = 1;
    }
    next ^= 1;
    return &own[next];
}

/* ---------- audio tap ---------- */
enum { FMT_S16 = 2, FMT_S24 = 6, FMT_S32 = 10, FMT_FLOAT = 14, ACC_NONINTERLEAVED = 4 };
/* The app writes two outputs (the I2S codec and the USB audio gadget); each keeps its own levels
   and the meters show the louder, so a silent USB stream cannot pull them to zero. */
static struct pcm { void *pcm; int fmt, ch, access; float ms[2], pk[2]; } pcms[4];
static unsigned long tap_calls;
static int tap_fmt = -1, tap_ch = -1;

static struct pcm *pcm_of(void *pcm)
{
    for (int i = 0; i < 4; ++i) if (pcms[i].pcm == pcm) return &pcms[i];
    for (int i = 0; i < 4; ++i) if (!pcms[i].pcm) { pcms[i] = (struct pcm){ .pcm = pcm, .fmt = -1, .ch = 2, .access = 3 }; return &pcms[i]; }
    return NULL;
}
#define NEXT(name, ret, args) static ret (*real)args; if (!real) real = (ret (*)args)dlsym(RTLD_NEXT, name)
int snd_pcm_hw_params_set_format(void *pcm, void *hw, int fmt)
{ NEXT("snd_pcm_hw_params_set_format", int, (void *, void *, int)); struct pcm *p = pcm_of(pcm); if (p) p->fmt = fmt; return real(pcm, hw, fmt); }
int snd_pcm_hw_params_set_access(void *pcm, void *hw, int acc)
{ NEXT("snd_pcm_hw_params_set_access", int, (void *, void *, int)); struct pcm *p = pcm_of(pcm); if (p) p->access = acc; return real(pcm, hw, acc); }
int snd_pcm_hw_params_set_channels(void *pcm, void *hw, unsigned ch)
{ NEXT("snd_pcm_hw_params_set_channels", int, (void *, void *, unsigned)); struct pcm *p = pcm_of(pcm); if (p) p->ch = (int)ch; return real(pcm, hw, ch); }
int snd_pcm_hw_params_set_channels_near(void *pcm, void *hw, unsigned *ch)
{ NEXT("snd_pcm_hw_params_set_channels_near", int, (void *, void *, unsigned *)); int rc = real(pcm, hw, ch); struct pcm *p = pcm_of(pcm); if (p && ch) p->ch = (int)*ch; return rc; }

static float sample(int fmt, const void *b, unsigned long i)
{
    switch (fmt) {
    case FMT_S16:   return ((const int16_t *)b)[i] * (1.f / 32768);
    case FMT_S24:   return (float)(((int32_t)(((const uint32_t *)b)[i] << 8)) >> 8) * (1.f / 8388608);
    case FMT_S32:   return ((const int32_t *)b)[i] * (1.f / 2147483648.f);
    case FMT_FLOAT: return ((const float *)b)[i];
    }
    return 0;
}

static void measure(struct pcm *p, const void *const *bufs, unsigned long frames)
{
    if (!p || frames == 0 || p->ch < 1 || (p->fmt != FMT_S16 && p->fmt != FMT_S24 && p->fmt != FMT_S32 && p->fmt != FMT_FLOAT)) return;
    ++tap_calls; tap_fmt = p->fmt; tap_ch = p->ch;
    int inter = p->access != ACC_NONINTERLEAVED;
    for (int c = 0; c < 2; ++c) {
        int src = c < p->ch ? c : 0;
        float pk = 0; double sq = 0;
        for (unsigned long i = 0; i < frames; ++i) {
            float x = inter ? sample(p->fmt, bufs[0], i * p->ch + src) : sample(p->fmt, bufs[src], i);
            if (!isfinite(x)) x = 0;
            float ax = fabsf(x); if (ax > pk) pk = ax;
            sq += (double)x * x;
        }
        float dt = frames / 48000.f;            /* ponytail: assumes 48 kHz; only shifts the ballistics a little at 44.1 */
        p->ms[c] += (float)(sq / frames - p->ms[c]) * (1 - expf(-dt / .08f));
        float fall = p->pk[c] * powf(10, -20 * dt / 20);
        p->pk[c] = pk > fall ? pk : fall;
        float ms = 0, mp = 0;
        for (int i = 0; i < 4; ++i) { if (pcms[i].ms[c] > ms) ms = pcms[i].ms[c]; if (pcms[i].pk[c] > mp) mp = pcms[i].pk[c]; }
        hs_seq_ms[c] = ms; hs_seq_pk[c] = mp;
    }
}

long snd_pcm_writei(void *pcm, const void *buf, unsigned long frames)
{
    NEXT("snd_pcm_writei", long, (void *, const void *, unsigned long));
    measure(pcm_of(pcm), &buf, frames);
    return real(pcm, buf, frames);
}
long snd_pcm_writen(void *pcm, void **bufs, unsigned long frames)
{
    NEXT("snd_pcm_writen", long, (void *, void **, unsigned long));
    if (bufs) measure(pcm_of(pcm), (const void *const *)bufs, frames);
    return real(pcm, bufs, frames);
}

/* ---------- meter animation ----------
   Between app redraws, a thread repaints only the VU and LED rects on the buffer being shown.
   It stops when the app shows another page, and holds off when a SampQOL overlay covers them. */
static pthread_mutex_t show_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t draw_lock = PTHREAD_MUTEX_INITIALIZER;   /* our buffers and caches; never held across real() */
static struct map *front;                       /* our buffer on screen, or NULL */
static int front_fd = -1;                       /* the DRM fd it was flipped on */
static int dirty_rc = 1;                        /* last dirty-call result, for the log */
static int meters_clear;                        /* no overlay over the meter rects */
static const int mrect[2][4] = {{6, 36, 150, 100}, {252, 72, 314, 102}};

static int rects_match(const uint32_t *px, uint32_t stride, const uint32_t *ref)
{
    for (int r = 0; r < 2; ++r) for (int y = mrect[r][1]; y <= mrect[r][3]; ++y)
        if (memcmp(px + (size_t)y * stride + mrect[r][0], ref + y * W + mrect[r][0], (mrect[r][2] - mrect[r][0] + 1) * 4)) return 0;
    return 1;
}

static void *animate(void *arg)
{
    (void)arg;
    static uint32_t m[N];
    for (int tick = 0;; ++tick) {
        usleep(40000);
        if (tick % 250 == 0 && tick <= 3000) {  /* a line every 10 s for 2 min, to check the tap on a device */
            FILE *f = fopen("/tmp/hs-seq.log", "a");
            if (f) {
                fprintf(f, "t=%ds tap calls=%lu fmt=%d ch=%d vu=%.2f front=%d clear=%d dirty=%d", tick / 25, tap_calls, tap_fmt, tap_ch, vu_pos(), front != NULL, meters_clear, dirty_rc);
                for (int i = 0; i < 4; ++i) if (pcms[i].pcm) fprintf(f, " | pcm%d pk=%.1f/%.1f dB", i, 20 * log10f(pcms[i].pk[0] + 1e-9f), 20 * log10f(pcms[i].pk[1] + 1e-9f));
                fputc('\n', f); fclose(f);
            }
        }
        uint32_t id = 0; int fd = -1;
        pthread_mutex_lock(&show_lock);
        if (front && meters_clear) {
            meters(m);
            for (int r = 0; r < 2; ++r) for (int y = mrect[r][1]; y <= mrect[r][3]; ++y)
                memcpy(front->px + (size_t)y * front->stride + mrect[r][0], m + y * W + mrect[r][0], (mrect[r][2] - mrect[r][0] + 1) * 4);
            id = front->id; fd = front_fd;
        }
        pthread_mutex_unlock(&show_lock);
        if (id && fd >= 0) {                    /* push the two rects to the panel; no lock held */
            struct cliprect c[2];
            for (int r = 0; r < 2; ++r) c[r] = (struct cliprect){mrect[r][0], mrect[r][1], mrect[r][2] + 1, mrect[r][3] + 1};
            struct dirtycmd d = {.fb_id = id, .num_clips = 2, .clips_ptr = (uintptr_t)c};
            dirty_rc = ioctl(fd, DIRTYFB, &d) ? -errno : 0;
        }
    }
    return NULL;
}
static void animate_start(void) { pthread_t t; if (!pthread_create(&t, NULL, animate, NULL)) pthread_detach(t); }

int drmModePageFlip(int fd, uint32_t crtc, uint32_t fb, uint32_t flags, void *ud)
{
    static flip_fn real;
    static uint32_t stock[N], out[N];
    static pthread_once_t once = PTHREAD_ONCE_INIT;
    if (!real) real = (flip_fn)dlsym(RTLD_NEXT, "drmModePageFlip");
    pthread_once(&once, animate_start);
    /* Never hold a lock across real(): SampQOL's overlay thread flips through this hook too, while
       holding its own lock, so waiting in here deadlocked the app (killed ~130 s later, 11 Nov 2026). */
    pthread_mutex_lock(&draw_lock);
    struct map *a = app_fb(fd, fb), *o = NULL;
    if (a) {
        for (int y = 0; y < H; ++y) memcpy(stock + y * W, a->px + (size_t)y * a->stride, W * 4);
        if (hs_seq_is_seq(stock) && (o = own_fb(fd))) {
            pthread_mutex_lock(&show_lock);
            front = NULL;                                       /* animate keeps off while o changes */
            pthread_mutex_unlock(&show_lock);
            hs_seq_draw(stock, out);
            for (int y = 0; y < H; ++y) memcpy(o->px + (size_t)y * o->stride, out + y * W, W * 4);
        }
    }
    pthread_mutex_unlock(&draw_lock);
    if (o) {
        int rc = real(fd, crtc, o->id, flags, ud);          /* SampQOL may draw overlays in here */
        pthread_mutex_lock(&draw_lock);
        int clear = !rc && rects_match(o->px, o->stride, out);
        pthread_mutex_unlock(&draw_lock);
        pthread_mutex_lock(&show_lock);
        front = rc ? NULL : o;
        front_fd = fd;
        meters_clear = clear;
        pthread_mutex_unlock(&show_lock);
        return rc;
    }
    pthread_mutex_lock(&show_lock);
    front = NULL;
    pthread_mutex_unlock(&show_lock);
    return real(fd, crtc, fb, flags, ud);
}
#endif
