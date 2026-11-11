/* browser_tag.c: key + tempo on the highlighted row of the stock sample browser.
 *
 * When the browser previews a file (the audition hook in browser_warp.c), the tempo the preview
 * uses is known at once and the key is worked out on an idle-priority thread. The tag sits at the
 * right end of the highlighted row: "C# MIN  75 BPM". It belongs to the row that was highlighted
 * when the preview started (a hash of the row's pixels), so scrolling away hides it and coming back
 * shows it again. No buttons, nothing to learn: it is just there while you browse.
 *
 * Drawing (same method as hs_seq.c): drmModePageFlip is interposed. On a browser frame the app's
 * buffer is copied into one of two buffers of our own, the tag drawn there, and ours is flipped,
 * so the app's buffers are never written. The key thread paints into our shown buffer and sends a
 * DIRTYFB when the result lands on a still screen. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <time.h>

#include "key_detect.h"
#include "browser_tag.h"

enum { W = 320, H = 240 };
#define ROW   0x2e2e33u   /* stock highlight bar */
#define WHITE 0xffffffu
#define GREY  0xa8a8b0u

/* ---------- pure parts (host-checked in check_tag.c) ---------- */
static uint32_t at(const uint32_t *px, int stride, int x, int y) { return px[(size_t)y * stride + x] & 0xffffff; }
static int near(uint32_t a, uint32_t b) {
    for (int s = 0; s < 24; s += 8) { int d = (int)(a >> s & 255) - (int)(b >> s & 255); if (d < -4 || d > 4) return 0; }
    return 1;
}

/* The storage pill (Internal / External) and the two page dots under it. */
int browser_tag_is_browser(const uint32_t *px, int stride) {
    uint32_t d1 = at(px, stride, 153, 34), d2 = at(px, stride, 172, 33);
    return at(px, stride, 122, 14) == WHITE && at(px, stride, 204, 14) == WHITE &&
           at(px, stride, 118, 14) == 0 && at(px, stride, 160, 8) == 0 &&
           at(px, stride, 146, 33) == 0 && at(px, stride, 179, 33) == 0 &&
           (d1 == WHITE || near(d1, 0x45454d)) && (d2 == WHITE || near(d2, 0x45454d)) && d1 != d2;
}

/* Highlighted row: the grey bar's top and bottom at x = 6, under the header. */
static int highlight(const uint32_t *px, int stride, int *y0, int *y1) {
    for (int y = 40; y < 215; ++y)
        if (near(at(px, stride, 6, y), ROW)) {
            int z = y;
            while (z + 1 < 215 && near(at(px, stride, 6, z + 1), ROW)) ++z;
            if (z - y < 16) return 0;
            *y0 = y; *y1 = z;
            return 1;
        }
    return 0;
}

/* Which row: lit pixels of the icon and name, left of where the tag goes. 0 = no row. */
uint32_t browser_tag_row(const uint32_t *px, int stride) {
    int y0, y1;
    if (!browser_tag_is_browser(px, stride) || !highlight(px, stride, &y0, &y1)) return 0;
    uint32_t h = 2166136261u;
    for (int y = y0; y <= y1; ++y)
        for (int x = 8; x < 176; ++x) {
            uint32_t c = at(px, stride, x, y), r = c >> 16 & 255, g = c >> 8 & 255, b = c & 255;
            uint32_t peak = r > g ? (r > b ? r : b) : (g > b ? g : b);
            h = (h ^ (peak > 128)) * 16777619u;   /* any colour: a loaded file's name turns blue */
        }
    return h | 1;
}

/* 5x7 font, column-major, bit 0 = top */
static const unsigned char *glyph(char c) {
    static const struct { char c; unsigned char b[5]; } T[] = {
        {'0',{0x3E,0x51,0x49,0x45,0x3E}},{'1',{0x00,0x42,0x7F,0x40,0x00}},{'2',{0x42,0x61,0x51,0x49,0x46}},
        {'3',{0x21,0x41,0x45,0x4B,0x31}},{'4',{0x18,0x14,0x12,0x7F,0x10}},{'5',{0x27,0x45,0x45,0x45,0x39}},
        {'6',{0x3C,0x4A,0x49,0x49,0x30}},{'7',{0x01,0x71,0x09,0x05,0x03}},{'8',{0x36,0x49,0x49,0x49,0x36}},
        {'9',{0x06,0x49,0x49,0x29,0x1E}},{'#',{0x14,0x7F,0x14,0x7F,0x14}},{'.',{0x00,0x60,0x60,0x00,0x00}},
        {'A',{0x7E,0x11,0x11,0x11,0x7E}},{'B',{0x7F,0x49,0x49,0x49,0x36}},{'C',{0x3E,0x41,0x41,0x41,0x22}},
        {'D',{0x7F,0x41,0x41,0x22,0x1C}},{'E',{0x7F,0x49,0x49,0x49,0x41}},{'F',{0x7F,0x09,0x09,0x09,0x01}},
        {'G',{0x3E,0x41,0x49,0x49,0x7A}},{'H',{0x7F,0x08,0x08,0x08,0x7F}},{'I',{0x00,0x41,0x7F,0x41,0x00}},
        {'J',{0x20,0x40,0x41,0x3F,0x01}},{'L',{0x7F,0x40,0x40,0x40,0x40}},{'M',{0x7F,0x02,0x0C,0x02,0x7F}},
        {'N',{0x7F,0x04,0x08,0x10,0x7F}},{'O',{0x3E,0x41,0x41,0x41,0x3E}},{'P',{0x7F,0x09,0x09,0x09,0x06}},
        {'R',{0x7F,0x09,0x19,0x29,0x46}},{'S',{0x46,0x49,0x49,0x49,0x31}},{'T',{0x01,0x01,0x7F,0x01,0x01}},
        {'X',{0x63,0x14,0x08,0x14,0x63}},{'Y',{0x07,0x08,0x70,0x08,0x07}},
    };
    static const unsigned char space[5];
    for (unsigned i = 0; i < sizeof T / sizeof *T; ++i) if (T[i].c == c) return T[i].b;
    return space;
}
static int text(uint32_t *px, int stride, int x, int y, const char *s, uint32_t colour) {
    for (; *s; ++s, x += 6) {
        const unsigned char *g = glyph(*s);
        for (int cx = 0; cx < 5; ++cx) for (int ry = 0; ry < 7; ++ry)
            if (g[cx] >> ry & 1) px[(size_t)(y + ry) * stride + x + cx] = colour;
    }
    return x;
}

/* key ("" = none yet) in white, tempo (0 = none) in grey, right-aligned in the highlighted row */
int browser_tag_paint(uint32_t *px, int stride, const char *key, float bpm) {
    int y0, y1;
    if (!browser_tag_is_browser(px, stride) || !highlight(px, stride, &y0, &y1)) return 0;
    char tempo[24] = "";
    if (bpm >= 40 && bpm <= 300) {
        int tenths = (int)(bpm * 10 + 0.5f);
        if (tenths % 10) snprintf(tempo, sizeof tempo, "%d.%d BPM", tenths / 10, tenths % 10);
        else snprintf(tempo, sizeof tempo, "%d BPM", tenths / 10);
    }
    if (!*key && !*tempo) return 0;
    int kw = *key ? 6 * (int)strlen(key) : 0, tw = *tempo ? 6 * (int)strlen(tempo) : 0;
    int width = kw + tw + (kw && tw ? 6 : 0) - 1, x = 312 - width, y = (y0 + y1) / 2 - 3;
    for (int yy = y0; yy <= y1; ++yy)                     /* clear room over a long file name */
        for (int xx = x - 8; xx < 316; ++xx) px[(size_t)yy * stride + xx] = ROW;
    if (*key) x = text(px, stride, x, y, key, WHITE) + 6;
    if (*tempo) text(px, stride, x, y, tempo, GREY);
    return 1;
}

#ifndef BROWSER_TAG_HOST
/* ---------- state ---------- */
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t wake = PTHREAD_COND_INITIALIZER;
static char want_path[1024];        /* the file being previewed */
static float tag_bpm;               /* the tempo the preview uses */
static char tag_key[24];            /* "" until the key thread is done */
static uint32_t tag_row;            /* the highlighted row it belongs to; 0 = not seen yet */
static unsigned generation;         /* bumped by every audition */
static long audition_ms;
#define CAPTURE_MS 150              /* rows flipped this soon after an audition are its row */

static long now_ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1000 + t.tv_nsec / 1000000; }

/* ---------- framebuffers (as hs_seq.c) ---------- */
struct fbcmd { uint32_t fb_id, width, height, pitch, bpp, depth, handle; };
struct mapdumb { uint32_t handle, pad; uint64_t offset; };
struct mkdumb { uint32_t height, width, bpp, flags, handle, pitch; uint64_t size; };
struct cliprect { uint16_t x1, y1, x2, y2; };
struct dirtycmd { uint32_t fb_id, flags, color, num_clips; uint64_t clips_ptr; };
#define GETFB       _IOWR('d', 0xAD, struct fbcmd)
#define ADDFB       _IOWR('d', 0xAE, struct fbcmd)
#define DIRTYFB     _IOWR('d', 0xB1, struct dirtycmd)
#define CREATE_DUMB _IOWR('d', 0xB2, struct mkdumb)
#define MAP_DUMB    _IOWR('d', 0xB3, struct mapdumb)
typedef int (*flip_fn)(int, uint32_t, uint32_t, uint32_t, void *);
struct map { uint32_t id, stride; uint32_t *px; };

static uint32_t *map_handle(int fd, uint32_t handle, size_t len) {
    struct mapdumb m = { .handle = handle };
    if (ioctl(fd, MAP_DUMB, &m)) return NULL;
    void *p = mmap(0, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, m.offset);
    return p == MAP_FAILED ? NULL : p;
}
static struct map *app_fb(int fd, uint32_t id) {
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
static struct map own[2];
static int own_ready, own_next;
static struct map *own_fb(int fd) {
    if (!own_ready) {
        for (int i = 0; i < 2; ++i) {
            struct mkdumb c = { .height = H, .width = W, .bpp = 32 };
            if (ioctl(fd, CREATE_DUMB, &c)) return NULL;
            struct fbcmd f = { .width = W, .height = H, .pitch = c.pitch, .bpp = 32, .depth = 24, .handle = c.handle };
            if (ioctl(fd, ADDFB, &f)) return NULL;
            uint32_t *p = map_handle(fd, c.handle, c.size);
            if (!p) return NULL;
            own[i] = (struct map){ f.fb_id, c.pitch / 4, p };
        }
        own_ready = 1;
    }
    own_next ^= 1;
    return &own[own_next];
}
static struct map *shown;           /* our buffer on the glass, or NULL */
static int shown_fd;
static uint32_t shown_clean[W * H]; /* its pixels without the tag, for the row check */

/* ---------- key thread ---------- */
static void *keys(void *unused) {
    (void)unused;
    struct sched_param idle = {0};
    sched_setscheduler(0, SCHED_IDLE, &idle);   /* never compete with the audio engine */
    setpriority(PRIO_PROCESS, 0, 19);
    unsigned done = 0;
    for (;;) {
        char path[sizeof want_path]; float bpm; unsigned gen;
        pthread_mutex_lock(&lock);
        while (generation == done) pthread_cond_wait(&wake, &lock);
        gen = done = generation;
        memcpy(path, want_path, sizeof path); bpm = tag_bpm;
        pthread_mutex_unlock(&lock);

        char key[24] = "";
        if (!browser_is_drums(path)) {
            unsigned n = 0, sr = 0; int root, scale;
            float *mono = browser_decode(path, &n, &sr);
            if (mono && !browser_detect_key(mono, n, (int)sr, bpm, &root, &scale)) browser_key_text(root, scale, key, sizeof key);
            free(mono);
        }
        uint32_t dirty_id = 0; int dirty_fd = -1;
        pthread_mutex_lock(&lock);
        if (gen == generation && *key) {
            memcpy(tag_key, key, sizeof key);
            if (shown && tag_row && browser_tag_row(shown_clean, W) == tag_row) {   /* still screen: paint now */
                browser_tag_paint(shown->px, shown->stride, tag_key, tag_bpm);
                dirty_id = shown->id; dirty_fd = shown_fd;
            }
        }
        pthread_mutex_unlock(&lock);
        if (dirty_id) {   /* push to the panel with no lock held: flips and auditions never wait on the SPI send */
            struct cliprect c = {0, 0, W, H};
            struct dirtycmd d = {.fb_id = dirty_id, .num_clips = 1, .clips_ptr = (uintptr_t)&c};
            ioctl(dirty_fd, DIRTYFB, &d);
        }
    }
    return NULL;
}

void browser_tag_audition(const char *path, float bpm) {
    if (!path) return;
    pthread_mutex_lock(&lock);
    if (strcmp(path, want_path) || tag_bpm != bpm) {   /* a replay of the same file keeps its key */
        snprintf(want_path, sizeof want_path, "%s", path);
        tag_bpm = bpm; tag_key[0] = 0; tag_row = 0;
        ++generation;
        pthread_cond_signal(&wake);
    }
    audition_ms = now_ms();
    /* The row on the glass now (a tempo detection can run longer than CAPTURE_MS after the
       highlight moved); a flip in the next CAPTURE_MS still corrects it. */
    if (shown) tag_row = browser_tag_row(shown_clean, W);
    pthread_mutex_unlock(&lock);
}

static void start_keys(void) { pthread_t t; if (!pthread_create(&t, NULL, keys, NULL)) pthread_detach(t); }

int drmModePageFlip(int fd, uint32_t crtc, uint32_t fb, uint32_t flags, void *ud) {
    static flip_fn real;
    static pthread_once_t once = PTHREAD_ONCE_INIT;
    if (!real) real = (flip_fn)dlsym(RTLD_NEXT, "drmModePageFlip");
    pthread_once(&once, start_keys);
    /* Never hold the lock across real(): SampQOL flips through here while holding its own lock. */
    pthread_mutex_lock(&lock);
    struct map *a = app_fb(fd, fb), *o = NULL;
    if (a && browser_tag_is_browser(a->px, a->stride) && (o = own_fb(fd))) {
        for (int y = 0; y < H; ++y) memcpy(shown_clean + y * W, a->px + (size_t)y * a->stride, W * 4);
        uint32_t row = browser_tag_row(shown_clean, W);
        if (*want_path && now_ms() - audition_ms < CAPTURE_MS) tag_row = row;
        for (int y = 0; y < H; ++y) memcpy(o->px + (size_t)y * o->stride, shown_clean + y * W, W * 4);
        if (row && row == tag_row) browser_tag_paint(o->px, o->stride, tag_key, tag_bpm);
    }
    shown = NULL;
    pthread_mutex_unlock(&lock);
    int rc = real(fd, crtc, o ? o->id : fb, flags, ud);
    if (o && !rc) {
        pthread_mutex_lock(&lock);
        shown = o; shown_fd = fd;
        pthread_mutex_unlock(&lock);
    }
    return rc;
}
#endif
