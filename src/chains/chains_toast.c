// chains_toast.c: a short box on the screen when a track chain loads, saves or is skipped.
//
// Drawing (as hs_stems_ui.c): drmModePageFlip is interposed, and ONLY while the box is
// up (TOAST_MS) the frame is copied into one of two buffers of our own, the box drawn there and ours
// flipped. Any other time every frame goes through untouched. While ours is on the glass, a DIRTYFB
// for the frame it covers (other hooks repainting a still screen) is mirrored into ours (ioctl hook),
// so their drawing still shows. When the box times out on a still screen, a small thread puts the
// covered frame back into ours and sends DIRTYFB. No lock is ever held across the real flip or a
// DIRTYFB. hs-chains.so goes after SampQOL in LD_PRELOAD so the box sits on top of SampQOL's screens.
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../stems/font.h"
#include "chains_toast.h"

enum { W = 320, H = 240 };
#define WHITE 0xffffffu
#define GREY  0xa8a8b0u
#define BOX   0x141416u

/* ---------- pure parts (host-checked in check_toast.c) ---------- */
static void fill(uint32_t *px, int stride, int x0, int y0, int x1, int y1, uint32_t c)
{
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x) px[(size_t)y * stride + x] = c;
}
static uint32_t blend(uint32_t bg, uint32_t fg, unsigned a)  // a 0..15
{
    uint32_t r = 0;
    for (int s = 0; s < 24; s += 8) r |= (((bg >> s & 255) * (15 - a) + (fg >> s & 255) * a) / 15) << s;
    return r;
}
// One centred line, cut to max_w pixels.
static void line(uint32_t *px, int stride, int y, const char *s, uint32_t c, int max_w)
{
    int n = (int)strlen(s);
    if (n * FONT_W > max_w) n = max_w / FONT_W;
    for (int i = 0, x = 160 - n * FONT_W / 2; i < n; ++i, x += FONT_W) {
        unsigned ch = (unsigned char)s[i];
        if (ch < 32 || ch > 126) ch = '?';
        for (int ry = 0; ry < FONT_H; ++ry)
            for (int cx = 0; cx < FONT_W; ++cx) {
                unsigned a = FONT[ch - 32][ry][cx / 2] >> (cx & 1 ? 0 : 4) & 15;
                if (a) { uint32_t *p = &px[(size_t)(y + ry) * stride + x + cx]; *p = blend(*p & 0xffffff, c, a); }
            }
    }
}
// The stems box style: white frame, dark fill, title in white, one grey line.
void chains_toast_paint(uint32_t *px, int stride, const char *title, const char *detail)
{
    fill(px, stride, 40, 88, 280, 152, WHITE);
    fill(px, stride, 41, 89, 279, 151, BOX);
    line(px, stride, 98, title, WHITE, 224);
    line(px, stride, 124, detail, GREY, 224);
}

#ifndef CHAINS_TOAST_HOST
#include <dlfcn.h>
#include <pthread.h>
#include <stdarg.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <time.h>

#define TOAST_MS 1500

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t wake = PTHREAD_COND_INITIALIZER;
static char toast_title[48], toast_detail[48];
static long toast_until;            // monotonic ms; 0 = no box

static long now_ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1000 + t.tv_nsec / 1000000; }

/* ---------- framebuffers (as hs_stems_ui.c) ---------- */
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

static uint32_t *map_handle(int fd, uint32_t handle, size_t len)
{
    struct mapdumb m = { .handle = handle };
    if (ioctl(fd, MAP_DUMB, &m)) return NULL;
    void *p = mmap(0, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, m.offset);
    return p == MAP_FAILED ? NULL : p;
}
static struct map *app_fb(int fd, uint32_t id)
{
    static struct map cache[16];
    for (int i = 0; i < 16; ++i) if (cache[i].id == id && cache[i].px) return &cache[i];
    struct fbcmd f = { .fb_id = id };
    if (ioctl(fd, GETFB, &f) || f.bpp != 32 || f.width != W || f.height != H || !f.handle) return NULL;
    uint32_t *p = map_handle(fd, f.handle, (size_t)f.pitch * H);
    if (!p) return NULL;
    for (int i = 0; i < 16; ++i)
        if (!cache[i].px) { cache[i] = (struct map){ id, f.pitch / 4, p }; return &cache[i]; }
    munmap(p, (size_t)f.pitch * H);
    return NULL;
}
static struct map own[2];
static int own_ready, own_next, own_failed;
static struct map *own_fb(int fd)
{
    if (own_failed) return NULL;    // ponytail: give up for the session rather than leak a buffer per flip
    if (!own_ready) {
        own_failed = 1;
        for (int i = 0; i < 2; ++i) {
            struct mkdumb c = { .height = H, .width = W, .bpp = 32 };
            if (ioctl(fd, CREATE_DUMB, &c)) return NULL;
            struct fbcmd f = { .width = W, .height = H, .pitch = c.pitch, .bpp = 32, .depth = 24, .handle = c.handle };
            if (ioctl(fd, ADDFB, &f)) return NULL;
            uint32_t *p = map_handle(fd, c.handle, c.size);
            if (!p) return NULL;
            own[i] = (struct map){ f.fb_id, c.pitch / 4, p };
        }
        own_ready = 1; own_failed = 0;
    }
    own_next ^= 1;
    return &own[own_next];
}
static struct map *shown;           // our buffer on the glass, or NULL
static struct map *covered;         // the frame it replaced
static uint32_t covered_id;         // covered->id, read without the lock in the ioctl hook
static int shown_fd;

// Ours = the covered frame (+ the box while it is up). Caller holds the lock; returns the fb to DIRTYFB.
static uint32_t repaint_locked(void)
{
    if (!shown || !covered) return 0;
    for (int y = 0; y < H; ++y) memcpy(shown->px + (size_t)y * shown->stride, covered->px + (size_t)y * covered->stride, W * 4);
    if (toast_until && now_ms() < toast_until) chains_toast_paint(shown->px, shown->stride, toast_title, toast_detail);
    return shown->id;
}
static void dirty(int fd, uint32_t id)
{
    struct cliprect c = { 0, 0, W, H };
    struct dirtycmd d = { .fb_id = id, .num_clips = 1, .clips_ptr = (uintptr_t)&c };
    ioctl(fd, DIRTYFB, &d);
}

// Shows the box on a still screen and takes it off again when it times out.
static void *timer(void *unused)
{
    (void)unused;
    pthread_mutex_lock(&lock);
    for (;;) {
        while (!toast_until) pthread_cond_wait(&wake, &lock);
        long left = toast_until - now_ms();
        if (left > 0) {
            struct timespec t;
            clock_gettime(CLOCK_REALTIME, &t);
            t.tv_sec += left / 1000; t.tv_nsec += left % 1000 * 1000000;
            if (t.tv_nsec >= 1000000000) { ++t.tv_sec; t.tv_nsec -= 1000000000; }
            pthread_cond_timedwait(&wake, &lock, &t);
            continue;   // a new box or the time is up: look again
        }
        toast_until = 0;
        uint32_t id = repaint_locked();
        int fd = shown_fd;
        pthread_mutex_unlock(&lock);
        if (id) dirty(fd, id);
        pthread_mutex_lock(&lock);
    }
    return NULL;
}
static void start_timer(void) { pthread_t t; if (!pthread_create(&t, NULL, timer, NULL)) pthread_detach(t); }

void chains_toast(const char *title, const char *detail)
{
    static pthread_once_t once = PTHREAD_ONCE_INIT;
    pthread_once(&once, start_timer);
    pthread_mutex_lock(&lock);
    snprintf(toast_title, sizeof toast_title, "%s", title);
    snprintf(toast_detail, sizeof toast_detail, "%s", detail);
    toast_until = now_ms() + TOAST_MS;
    uint32_t id = repaint_locked();     // ours already on the glass (a still screen): show it now
    int fd = shown_fd;
    pthread_cond_signal(&wake);
    pthread_mutex_unlock(&lock);
    if (id) dirty(fd, id);
}

int drmModePageFlip(int fd, uint32_t crtc, uint32_t fb, uint32_t flags, void *ud)
{
    static flip_fn real;
    if (!real) real = (flip_fn)dlsym(RTLD_NEXT, "drmModePageFlip");
    char title[sizeof toast_title], detail[sizeof toast_detail];
    pthread_mutex_lock(&lock);
    int up = toast_until && now_ms() < toast_until;
    if (up) { memcpy(title, toast_title, sizeof title); memcpy(detail, toast_detail, sizeof detail); }
    shown = NULL; covered = NULL; __atomic_store_n(&covered_id, 0, __ATOMIC_RELAXED);
    pthread_mutex_unlock(&lock);
    // app_fb's cache and our two buffers: only this thread flips, and neither buffer is shown now
    struct map *a = NULL, *o = NULL;
    if (up && (a = app_fb(fd, fb)) && (o = own_fb(fd))) {
        for (int y = 0; y < H; ++y) memcpy(o->px + (size_t)y * o->stride, a->px + (size_t)y * a->stride, W * 4);
        chains_toast_paint(o->px, o->stride, title, detail);
    }
    int rc = real(fd, crtc, o ? o->id : fb, flags, ud);
    if (o && !rc) {
        pthread_mutex_lock(&lock);
        shown = o; shown_fd = fd; covered = a; __atomic_store_n(&covered_id, fb, __ATOMIC_RELAXED);
        pthread_mutex_unlock(&lock);
    }
    return rc;
}

// Someone repainted the frame ours covers on a still screen: copy it into ours and show it.
int ioctl(int fd, unsigned long req, ...)
{
    static int (*real)(int, unsigned long, ...);
    if (!real) real = (int (*)(int, unsigned long, ...))dlsym(RTLD_NEXT, "ioctl");
    va_list ap; va_start(ap, req); void *arg = va_arg(ap, void *); va_end(ap);
    int rc = real(fd, req, arg);
    uint32_t id = __atomic_load_n(&covered_id, __ATOMIC_RELAXED);
    if (req == DIRTYFB && id && arg && ((struct dirtycmd *)arg)->fb_id == id) {
        pthread_mutex_lock(&lock);  // our own DIRTYFB names our buffer: no re-entry
        uint32_t mine = covered && covered->id == id ? repaint_locked() : 0;
        int sfd = shown_fd;
        pthread_mutex_unlock(&lock);
        if (mine) dirty(sfd, mine);
    }
    return rc;
}
#endif
