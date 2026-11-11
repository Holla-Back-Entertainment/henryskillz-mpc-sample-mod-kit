/* hs_stems_ui.c: "Stems" in the sample browser and on the Sample (pad) screen (roadmap #8).
 *
 * Browser: preview a WAV (Preview on, the stock default), hold SHIFT: B1, which only repeats "Back"
 * there, reads "Stems". Sample screen: hold SHIFT: B3 reads "Stems" (stock SHIFT+B1 is Loop Lock,
 * SHIFT+B2 Normalize, SHIFT+B3 nothing). Either press opens the chooser: the wheel (or -/+) moves,
 * a wheel push ticks Vocals / Drums / Bass / Other, B2 switches Separate (one WAV per stem) and Sum
 * (the ticked stems mixed into one WAV), B3 is Go, B1 Cancel. hs-stems then runs as an idle-priority
 * child; a box shows progress and B1 (now "Cancel") stops it.
 * Out: browser -> "<name> Stems/" next to the file; pad -> <card>/MPC-Sample/Samples/Stems/<name> Stems/.
 *
 * Which file, browser: the app opens the highlighted file when it previews it, so open()/fopen() are
 * watched for .wav paths and each is bound to the highlighted row on the glass (a hash of its pixels,
 * as in browser_tag.c). No preview = no path = stock.
 * Which file, pad: the pad label read off the glass + the app's autosave (stems_pad.c). The autosave
 * lags a change by ~10 s, so a job waits until the save is newer than the last panel input, or 13 s.
 *
 * Input: the panel MIDI stream is read as hs_seq.c does. A press we take over has its velocity byte
 * set to 0 (a note-off) so the app never sees it; the release passes as a harmless note-off. A wheel
 * turn we take has its value set to 0, which the app reads as no step (lab-checked).
 * Drawing: drmModePageFlip is interposed. Browser and Sample frames are always copied (read only),
 * but our own two buffers replace the app's frame ONLY while we draw something (SHIFT label,
 * chooser, job box). Any other time the app's frame goes through untouched: earlier hooks in the
 * chain (graphic-mixer padmix, hs-chrome, browser-warp) paint on the buffer they flipped and send
 * DIRTYFB, and a copy of ours on the glass hid that (hardware: screen "froze" while the app ran).
 * While ours is up, a DIRTYFB for the frame it covers is mirrored into ours (ioctl hook), so their
 * still-screen drawing shows, under our drawing.
 * Job updates repaint with DIRTYFB while the screen is still. The lock is never held across the
 * real flip. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <spawn.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#ifndef STEMS_HOST
#include <link.h>
#include <zlib.h>
#endif

#include "font.h"
#include "stems_ui.h"

extern char **environ;
enum { W = 320, H = 240 };
#define ROW   0x2e2e33u
#define WHITE 0xffffffu
#define GREY  0xa8a8b0u
#define BOX   0x141416u
#define ORANGE 0xffb000u
#define DIM   0x6e6e74u
#define SHIFT_NOTE 49
#define B1_NOTE 12
#define B2_NOTE 13
#define B3_NOTE 14
#define PUSH_NOTE 111   /* wheel push */
#define PLUS_NOTE 54
#define MINUS_NOTE 55
#define DATA_CC 100     /* wheel turn: 1..63 up, 65..127 down, 0 = no step */
#define CAPTURE_MS 150

/* ---------- pure parts (host-checked in check_ui.c) ---------- */
static uint32_t at(const uint32_t *px, int stride, int x, int y) { return px[(size_t)y * stride + x] & 0xffffff; }
static int near(uint32_t a, uint32_t b) {
    for (int s = 0; s < 24; s += 8) { int d = (int)(a >> s & 255) - (int)(b >> s & 255); if (d < -4 || d > 4) return 0; }
    return 1;
}
/* The storage pill (Internal / External) and its two page dots (same test as browser_tag.c). */
int stems_is_browser(const uint32_t *px, int stride) {
    uint32_t d1 = at(px, stride, 153, 34), d2 = at(px, stride, 172, 33);
    return at(px, stride, 122, 14) == WHITE && at(px, stride, 204, 14) == WHITE &&
           at(px, stride, 118, 14) == 0 && at(px, stride, 160, 8) == 0 &&
           at(px, stride, 146, 33) == 0 && at(px, stride, 179, 33) == 0 &&
           (d1 == WHITE || near(d1, 0x45454d)) && (d2 == WHITE || near(d2, 0x45454d)) && d1 != d2;
}
/* The same browser with SHIFT held: the pill becomes "Eject" (white text) and B3 a red "Delete". */
int stems_is_shift_browser(const uint32_t *px, int stride) {
    int red = 0, white = 0;
    for (int y = 15; y <= 30; ++y)
        for (int x = 140; x <= 292; ++x) {
            uint32_t c = at(px, stride, x, y);
            if (x >= 240 && (c >> 16 & 255) > 180 && (c >> 8 & 255) < 90 && (c & 255) < 90) ++red;
            if (x <= 182 && c == WHITE) ++white;
        }
    return red >= 40 && white >= 15 && white <= 90 && at(px, stride, 122, 14) == 0 && at(px, stride, 204, 14) == 0;
}
static int any_browser(const uint32_t *px, int stride) { return stems_is_browser(px, stride) || stems_is_shift_browser(px, stride); }
/* Hash of the highlighted row's lit pixels; 0 = no browser or no highlighted row. */
uint32_t stems_row(const uint32_t *px, int stride) {
    if (!any_browser(px, stride)) return 0;
    for (int y = 40; y < 215; ++y)
        if (near(at(px, stride, 6, y), ROW)) {
            int z = y;
            while (z + 1 < 215 && near(at(px, stride, 6, z + 1), ROW)) ++z;
            if (z - y < 16) return 0;
            uint32_t h = 2166136261u;
            for (int yy = y; yy <= z; ++yy)
                for (int x = 8; x < 176; ++x) {
                    uint32_t c = at(px, stride, x, yy), m = c >> 16 & 255;
                    if ((c >> 8 & 255) > m) m = c >> 8 & 255;
                    if ((c & 255) > m) m = c & 255;
                    h = (h ^ (m > 128)) * 16777619u;
                }
            return h | 1;
        }
    return 0;
}
int stems_is_wav(const char *p) {
    size_t n = p ? strlen(p) : 0;
    return n > 4 && p[0] == '/' && (!strcasecmp(p + n - 4, ".wav") || !strcasecmp(p + n - 4, ".mp3"));   /* the engine reads both */
}
/* "/a/b/Loop.wav" -> dir "/a/b/Loop Stems", base "Loop". 0 on success. */
int stems_out(const char *path, char *dir, size_t dn, char *base, size_t bn) {
    const char *slash = strrchr(path, '/'), *name = slash + 1;
    size_t len = strlen(name) - 4;
    if (!len || len >= bn) return -1;
    memcpy(base, name, len); base[len] = 0;
    int r = snprintf(dir, dn, "%.*s/%s Stems", (int)(slash - path), path, base);
    return r < 0 || (size_t)r >= dn ? -1 : 0;
}

static void fill(uint32_t *px, int stride, int x0, int y0, int x1, int y1, uint32_t c) {
    for (int y = y0 < 0 ? 0 : y0; y <= y1 && y < H; ++y)
        for (int x = x0 < 0 ? 0 : x0; x <= x1 && x < W; ++x) px[(size_t)y * stride + x] = c;
}
static uint32_t blend(uint32_t bg, uint32_t fg, unsigned a) {  /* a 0..15 */
    uint32_t r = 0;
    for (int s = 0; s < 24; s += 8) r |= (((bg >> s & 255) * (15 - a) + (fg >> s & 255) * a) / 15) << s;
    return r;
}
/* Text with its top-left at x,y, clipped to max_x. Returns the end x. */
static int text(uint32_t *px, int stride, int x, int y, const char *s, uint32_t c, int max_x) {
    for (; *s && x + FONT_W <= max_x; ++s, x += FONT_W) {
        unsigned ch = (unsigned char)*s; if (ch < 32 || ch > 126) ch = '?';
        for (int ry = 0; ry < FONT_H; ++ry)
            for (int cx = 0; cx < FONT_W; ++cx) {
                unsigned a = FONT[ch - 32][ry][cx / 2] >> (cx & 1 ? 0 : 4) & 15;
                if (a) { uint32_t *p = &px[(size_t)(y + ry) * stride + x + cx]; *p = blend(*p & 0xffffff, c, a); }
            }
    }
    return x;
}
static int text_w(const char *s) { return FONT_W * (int)strlen(s); }

/* B1 label: replace the "< Back" corner. */
void stems_paint_b1(uint32_t *px, int stride, const char *label) {
    fill(px, stride, 0, 6, 104, 34, 0);     /* from y 6: keeps the app's thin top bar */
    text(px, stride, 53 - text_w(label) / 2, 15, label, WHITE, 104);
}
/* The box: title, bar (pct < 0 = no bar), one status line. */
void stems_paint_box(uint32_t *px, int stride, const char *title, int pct, const char *line) {
    fill(px, stride, 30, 64, 290, 176, WHITE);
    fill(px, stride, 31, 65, 289, 175, BOX);
    text(px, stride, 160 - (text_w(title) < 240 ? text_w(title) : 240) / 2, 78, title, WHITE, 282);
    if (pct >= 0) {
        fill(px, stride, 50, 108, 270, 124, WHITE);
        fill(px, stride, 52, 110, 268, 122, BOX);
        if (pct) fill(px, stride, 52, 110, 52 + 216 * (pct > 100 ? 100 : pct) / 100, 122, WHITE);
    }
    text(px, stride, 160 - (text_w(line) < 240 ? text_w(line) : 240) / 2, 140, line, GREY, 282);
}

/* B3 label (the Sample screen's free SHIFT slot): replace the right-hand tab. */
void stems_paint_b3(uint32_t *px, int stride, const char *label) {
    fill(px, stride, 214, 6, W - 1, 36, 0);
    text(px, stride, 267 - text_w(label) / 2, 15, label, WHITE, W);
}
/* Our own three labels over the whole tab strip. */
void stems_paint_tabs(uint32_t *px, int stride, const char *b1, const char *b2, const char *b3) {
    const char *l[3] = {b1, b2, b3};
    fill(px, stride, 0, 6, W - 1, 36, 0);
    for (int i = 0; i < 3; ++i)
        if (l[i] && *l[i]) text(px, stride, 53 + 107 * i - text_w(l[i]) / 2, 15, l[i], WHITE, W);
}
/* A second grey line in the box, where the bar would be. */
void stems_paint_note(uint32_t *px, int stride, const char *line) {
    text(px, stride, 160 - (text_w(line) < 240 ? text_w(line) : 240) / 2, 108, line, GREY, 282);
}
/* The chooser: four ticks (bit i = Vocals, Drums, Bass, Other), the highlighted row, what Go makes. */
void stems_paint_chooser(uint32_t *px, int stride, const char *title, int ticks, int sel, int summed) {
    static const char *N[4] = {"Vocals", "Drums", "Bass", "Other"};
    fill(px, stride, 24, 44, 296, 198, WHITE);
    fill(px, stride, 25, 45, 295, 197, BOX);
    text(px, stride, 160 - (text_w(title) < 260 ? text_w(title) : 260) / 2, 50, title, WHITE, 292);
    char out[48] = ""; int n = 0;
    for (int i = 0; i < 4; ++i) {
        int y = 72 + i * 20, on = ticks >> i & 1;
        if (i == sel) fill(px, stride, 32, y - 2, 288, y + 16, ROW);
        fill(px, stride, 44, y + 1, 57, y + 14, WHITE);
        fill(px, stride, 45, y + 2, 56, y + 13, on ? ORANGE : BOX);
        text(px, stride, 68, y, N[i], on ? WHITE : DIM, 280);
        if (on) { snprintf(out + strlen(out), sizeof out - strlen(out), "%s%s", n ? "+" : "", N[i]); ++n; }
    }
    char line[64];
    if (!n) snprintf(line, sizeof line, "Tick at least one");
    else if (summed) snprintf(line, sizeof line, "1 file: %s", out);
    else snprintf(line, sizeof line, n == 1 ? "1 file" : "%d files", n);
    text(px, stride, 160 - text_w(line) / 2, 156, line, GREY, 292);
    text(px, stride, 160 - text_w("turn: move  push: tick") / 2, 176, "turn: move  push: tick", DIM, 292);
}


#ifndef STEMS_HOST
/* ---------- state (all under lock) ---------- */
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
/* PREEMPT_RT: realtime app threads take this lock, so a normal-priority holder must be boosted. */
__attribute__((constructor)) static void lock_init(void) {
    pthread_mutexattr_t a; pthread_mutexattr_init(&a);
    pthread_mutexattr_setprotocol(&a, PTHREAD_PRIO_INHERIT);
    pthread_mutex_init(&lock, &a); pthread_mutexattr_destroy(&a);
}
enum { IDLE, CHOOSE, RUNNING, DONE, FAILED };
enum { OTHER, BROWSER, PAD };       /* what the last app frame showed */
static int state = IDLE, shift_held;
/* SHIFT+B1 (browser) / SHIFT+B3 (Sample screen) open Stems only when HELD for HOLD_MS, so a quick press
   keeps whatever job other features give it (Load, Rename ...): we hold the press back and replay it on a
   short release. hold_note = the button being held (0 none); hold_gen cancels a stale timer. */
#ifndef HOLD_MS
#define HOLD_MS 1500
#endif
static int hold_note, hold_kind, hold_fired;
static unsigned hold_gen;
static long hold_prev_rt;
static unsigned char replay_note;   /* a short press to hand the app after this read */
static char seen_path[1024];        /* last .wav the app opened */
static char ch_path[1024];          /* the .wav the open chooser names (seen_path can move on) */
static uint32_t seen_row;           /* the highlighted row it belongs to */
static long seen_ms;
static int pad_track, pad_idx;      /* the Sample screen's label: T number (0 = not shown), pad 0..127 */
static long last_input_rt;          /* wall clock ms of the last panel input that may change the project */
/* the chooser; ticks and Sum stay as last used */
static int ch_src, ch_track, ch_pad, ch_sel, ch_ticks = 15, ch_sum;
static long ch_input_rt;
static char ch_title[96];
/* the job */
static int job_src, job_track, job_pad, job_ticks, job_sum;
static long job_input_rt;
static char job_path[1024], job_dir[1200], job_base[256], job_msg[96], job_note[96];
static int job_pct;                 /* -1: waiting for the autosave */
static long job_t0, done_ms;
static pid_t job_pid;
static int job_cancel;

static long now_ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1000 + t.tv_nsec / 1000000; }
static long wall_ms(void) { struct timespec t; clock_gettime(CLOCK_REALTIME, &t); return t.tv_sec * 1000 + t.tv_nsec / 1000000; }
static void ulog(const char *fmt, ...) {
    FILE *f = fopen("/tmp/hs-stems-ui.log", "a"); if (!f) return;
    va_list a; va_start(a, fmt); vfprintf(f, fmt, a); va_end(a); fputc('\n', f); fclose(f);
}

/* ---------- framebuffers (as hs_seq.c / browser_tag.c) ---------- */
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
static struct map *own_fb(int fd) {
    if (own_failed) return NULL;    /* ponytail: give up for the session rather than leak a buffer per flip */
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
static struct map *shown;           /* our buffer on the glass, or NULL */
static struct map *covered;         /* the frame it replaced (app's or an earlier hook's) */
static uint32_t covered_id;         /* covered->id, read without the lock in the ioctl hook */
static int shown_fd, shown_kind;
static uint32_t clean[W * H];       /* the app's last browser / Sample frame, without our drawing */

static int armed(void) {            /* a file is known for the highlighted browser row */
    return *seen_path && seen_row && stems_row(clean, W) == seen_row;
}
/* Our drawing on top of the clean frame, by state. Caller holds the lock. */
static void decorate(uint32_t *px, int stride, int kind) {
    char line[64];
    if (state == RUNNING) {
        long el = (now_ms() - job_t0) / 1000;
        if (job_pct < 0) snprintf(line, sizeof line, "Waiting for the autosave...");
        else if (job_pct > 0 && job_pct < 100) {
            long left = el * (100 - job_pct) / job_pct;
            snprintf(line, sizeof line, "Separating... %d%%  ~%ld:%02ld left", job_pct, left / 60, left % 60);
        } else if (job_pct >= 100) snprintf(line, sizeof line, "Loading onto pads...");
        else snprintf(line, sizeof line, "Separating... %d%%", job_pct);
        char title[96]; snprintf(title, sizeof title, "Stems: %.80s", job_base);
        stems_paint_tabs(px, stride, "Cancel", "", "");
        stems_paint_box(px, stride, title, job_pct < 0 ? 0 : job_pct, line);
    } else if (state == DONE || state == FAILED) {
        char title[96];
        if (state == DONE && job_src == PAD) snprintf(title, sizeof title, "Saved in Samples/Stems");
        else snprintf(title, sizeof title, state == DONE ? "Saved: %.60s Stems" : "Stems: %.60s", job_base);
        stems_paint_box(px, stride, title, -1, job_msg);
        if (*job_note) stems_paint_note(px, stride, job_note);
    } else if (state == CHOOSE) {
        stems_paint_tabs(px, stride, "Cancel", ch_sum ? "Sum" : "Separate", "Go");
        stems_paint_chooser(px, stride, ch_title, ch_ticks, ch_sel, ch_sum);
    } else if (hold_note == B1_NOTE && kind == BROWSER)   /* only while a hold is under way: SHIFT alone shows the app's own labels */
        stems_paint_b1(px, stride, "Stems...");
    else if (hold_note == B3_NOTE && kind == PAD)
        stems_paint_b3(px, stride, "Stems...");
}
/* Repaint our shown buffer from the clean frame (screen still: the app is not flipping). */
static void repaint_locked(void) {
    if (!shown) return;
    for (int y = 0; y < H; ++y) memcpy(shown->px + (size_t)y * shown->stride, clean + y * W, W * 4);
    decorate(shown->px, shown->stride, shown_kind);
    struct cliprect c = {0, 0, W, H};
    struct dirtycmd d = {.fb_id = shown->id, .num_clips = 1, .clips_ptr = (uintptr_t)&c};
    ioctl(shown_fd, DIRTYFB, &d);
}

int drmModePageFlip(int fd, uint32_t crtc, uint32_t fb, uint32_t flags, void *ud) {
    static flip_fn real;
    if (!real) real = (flip_fn)dlsym(RTLD_NEXT, "drmModePageFlip");
    struct map *a = app_fb(fd, fb), *o = NULL;           /* app_fb's cache: only this thread flips */
    int kind = OTHER, tr = 0, pd = 0;
    if (a) kind = any_browser(a->px, a->stride) ? BROWSER : !stems_pad_label(a->px, a->stride, &tr, &pd) ? PAD : OTHER;
    pthread_mutex_lock(&lock);
    if (kind == PAD) { pad_track = tr; pad_idx = pd; }
    /* the chooser belongs to one screen (and one pad): leaving it closes the chooser */
    if (state == CHOOSE && (kind != ch_src || (kind == PAD && (tr != ch_track || pd != ch_pad)))) state = IDLE;
    if (kind != OTHER) {
        for (int y = 0; y < H; ++y) memcpy(clean + y * W, a->px + (size_t)y * a->stride, W * 4);
        if (kind == BROWSER && *seen_path && now_ms() - seen_ms < CAPTURE_MS) seen_row = stems_row(clean, W);
    }
    /* With SHIFT held on a screen Stems serves we show our own copy of the frame (no label until a hold
       starts), so a hold can repaint at once: a swallowed press makes the app send no new frame. */
    int draw = kind != OTHER && (state != IDLE || (shift_held && (kind == PAD || (kind == BROWSER && armed()))));
    if (draw && (o = own_fb(fd))) {
        for (int y = 0; y < H; ++y) memcpy(o->px + (size_t)y * o->stride, clean + y * W, W * 4);
        decorate(o->px, o->stride, kind);
    }
    shown = NULL; covered = NULL; __atomic_store_n(&covered_id, 0, __ATOMIC_RELAXED);
    pthread_mutex_unlock(&lock);
    int rc = real(fd, crtc, o ? o->id : fb, flags, ud);
    if (o && !rc) {
        pthread_mutex_lock(&lock);
        shown = o; shown_fd = fd; shown_kind = kind; covered = a; __atomic_store_n(&covered_id, fb, __ATOMIC_RELAXED);
        pthread_mutex_unlock(&lock);
    }
    return rc;
}
/* Someone repainted the frame ours covers on a still screen: copy it into ours and show it. */
int ioctl(int fd, unsigned long req, ...) {
    static int (*real)(int, unsigned long, ...);
    if (!real) real = (int (*)(int, unsigned long, ...))dlsym(RTLD_NEXT, "ioctl");
    va_list ap; va_start(ap, req); void *arg = va_arg(ap, void *); va_end(ap);
    int rc = real(fd, req, arg);
    uint32_t id = __atomic_load_n(&covered_id, __ATOMIC_RELAXED);
    if (req == DIRTYFB && id && arg && ((struct dirtycmd *)arg)->fb_id == id) {
        pthread_mutex_lock(&lock);  /* repaint_locked's own DIRTYFB names our buffer: no re-entry */
        if (shown && covered && covered->id == id) {
            for (int y = 0; y < H; ++y) memcpy(clean + y * W, covered->px + (size_t)y * covered->stride, W * 4);
            repaint_locked();
        }
        pthread_mutex_unlock(&lock);
    }
    return rc;
}


/* ---------- the job ---------- */
#define XPJ "/data/TempProjects/Temp-Project.xpj"
#define PROJECT_DATA "/data/TempProjects/Temp-Project_[ProjectData]"
#define AUTOSAVE_MS 13000           /* the app autosaves ~10 s after a change (measured in the lab) */

static int cancelled(void) { pthread_mutex_lock(&lock); int c = job_cancel; pthread_mutex_unlock(&lock); return c; }
static void set_pct(int pct) { pthread_mutex_lock(&lock); job_pct = pct; repaint_locked(); pthread_mutex_unlock(&lock); }

/* The autosave, decompressed and NUL-terminated (~11 MB, freed by the caller). */
static char *read_xpj(void) {
    gzFile z = gzopen(XPJ, "rb");
    if (!z) return NULL;
    size_t cap = 16 << 20, len = 0; char *b = malloc(cap);
    int n = 0;
    while (b && (n = gzread(z, b + len, (unsigned)(cap - 1 - len))) > 0) {
        len += n;
        if (cap - 1 - len < 65536) { char *nb = realloc(b, cap *= 2); if (!nb) { free(b); b = NULL; } else b = nb; }
    }
    gzclose(z);
    if (!b || n < 0) { free(b); return NULL; }
    b[len] = 0;
    return b;
}
/* Wait until the autosave can hold every change up to input_rt: saved after it, or AUTOSAVE_MS since
   it, and >= 1 s old so a save in progress isn't read half-written. 0, or a reason. */
static const char *fresh_autosave(long input_rt) {
    for (;;) {
        struct stat st;
        long m = stat(XPJ, &st) ? 0 : st.st_mtim.tv_sec * 1000 + st.st_mtim.tv_nsec / 1000000;
        long w = wall_ms();
        if ((m >= input_rt || w >= input_rt + AUTOSAVE_MS) && w - m >= 1000) return NULL;
        if (cancelled()) return "Cancelled";
        set_pct(-1);
        usleep(250000);
    }
}

/* Pad source: wait until the autosave can hold the last change, then find the pad's file.
   Fills path, base and dir (<card>/MPC-Sample/Samples/Stems/<base> Stems). NULL or a reason. */
static const char *pad_source(int track, int pad, long input_rt, char *path, size_t pn, char *base, size_t bn, char *dir, size_t dn) {
    const char *late = fresh_autosave(input_rt);
    if (late) return late;
    char file[512];
    char *x = read_xpj();
    if (!x) return "Can't read the project";
    const char *why = stems_xpj_sample(x, track, pad, file, sizeof file);
    free(x);
    if (why) return why;
    size_t fl = strlen(file);
    if (fl < 5 || strcasecmp(file + fl - 4, ".wav")) return "The pad's sample isn't a WAV";
    if (fl - 4 >= bn) return "Name too long";
    memcpy(base, file, fl - 4); base[fl - 4] = 0;
    snprintf(path, pn, "%s/%s", PROJECT_DATA, file);
    if (access(path, R_OK)) return "Sample file not found: save the project";
    const char *s = getenv("HS_STEMS_DIR");               /* <card>/autorun/stems */
    char card[1024]; snprintf(card, sizeof card, "%s", s ? s : "");
    for (int i = 0; i < 2; ++i) { char *sl = strrchr(card, '/'); if (!sl) return "No card"; *sl = 0; }
    char stems[1100]; snprintf(stems, sizeof stems, "%s/MPC-Sample/Samples/Stems", card);
    if (mkdir(stems, 0755) && errno != EEXIST) return "Can't write to the card";
    int r = snprintf(dir, dn, "%s/%s Stems", stems, base);
    return r < 0 || (size_t)r >= dn ? "Name too long" : NULL;
}

/* ---------- onto pads ----------
   The firmware's own path for the browser's Load: CommandFactory::loadSample (LoadSampleCommand +
   AssignSampleToProgramCommand, one undo step "Load Sample"), onto the current track's program.
   MPC Sample 1.3.0.12 (40e46ca7991) only: fw_ok() checks the version note and every object's vptr before
   any call. All of it runs on the app's message thread, through JUCE's callFunctionOnMessageThread
   (the job thread waits for it), as the browser's Load does. */
#define FW_NOTE 0x2c0                /* .note.mpc.version text */
static const char FW_VERSION[] = "1.3.0.12 (40e46ca7991)";
#define MM_INSTANCE 0xff3ff0         /* juce::MessageManager::getInstance() */
#define MM_CALL 0xff4e30             /* MessageManager::callFunctionOnMessageThread(mm, fn, arg): returns fn's result */
#define FACTORY_VPTR 0x6d95318       /* CommandFactory (one on the heap, as hs_chains.c finds it) */
#define LOAD_SAMPLE 0x195bd00        /* CommandFactory::loadSample(factory, LoadParams*) */
#define FILE_RES 0x1df1ec0           /* FileResource(this, const File*, bool keep): keep=0 deletes the file when freed */
#define FILE_RES_CB 0x6d8b498        /* vptr of make_shared<FileResource>'s control block */
#define SP_RELEASE 0x123bca0         /* shared_ptr control block: drop one use */
#define REPORTER 0x1df1a20           /* FileProgressReporter(this, MpcProgressHub*) */
#define POOL_FIND 0x292ed50          /* SamplePool::find(pool, const String* name) -> shared_ptr<Sample> via x8 */
#define STRING_CTOR 0xf535c0         /* juce::String(const char*) */
#define STRING_DTOR 0xf0a290
#define VP_ENGINE 0x6d9bc40          /* EngineMode       factory+0x88 */
#define VP_HUB 0x6da24f8             /* MpcProgressHub   factory+0x90 */
#define VP_LOADER 0x6e600a8          /* SampleLoader     factory+0x18 */
#define VP_PROJECT 0x6ecc648         /* Project          factory+0x28 */
#define VP_POOL 0x6dbd2f0            /* SamplePool       project+0x1278 */
#define PROJECT_TRACK 0x140          /* project: the current Track* (loadSample assigns to its program) */

static uintptr_t fw_base, fw_factory;
static int fw_base_cb(struct dl_phdr_info *i, size_t s, void *d) { (void)s; (void)d; if (*i->dlpi_name) return 0; fw_base = i->dlpi_addr; return 1; }
static int readable(uintptr_t p) {   /* plausible, 8-aligned, mapped */
    return p > 0x10000 && p < 0xffffffffffffull && !(p & 7) && !msync((void *)(p & ~0xfffUL), 4096, MS_ASYNC);
}
static int is_a(uintptr_t obj, uintptr_t vptr) { return readable(obj) && *(uintptr_t *)obj == fw_base + vptr; }
static uintptr_t word(uintptr_t p, unsigned off) { return *(uintptr_t *)(p + off); }
/* The factory, checked; one heap scan the first time (on the message thread, as hs_chains.c). */
static uintptr_t fw_ok(void) {
    if (!fw_base) dl_iterate_phdr(fw_base_cb, 0);
    if (!fw_base || memcmp((const void *)(fw_base + FW_NOTE), FW_VERSION, sizeof FW_VERSION)) return 0;
    if (!is_a(fw_factory, FACTORY_VPTR)) {
        fw_factory = 0;
        FILE *m = fopen("/proc/self/maps", "r"); char line[512];
        while (m && !fw_factory && fgets(line, sizeof line, m)) {
            unsigned long a, b; char perm[5];
            if (!strstr(line, "[heap]") || sscanf(line, "%lx-%lx %4s", &a, &b, perm) != 3 || perm[1] != 'w') continue;
            for (uintptr_t *w = (uintptr_t *)a; w < (uintptr_t *)b; ++w) if (*w == fw_base + FACTORY_VPTR) { fw_factory = (uintptr_t)w; break; }
        }
        if (m) fclose(m);
    }
    uintptr_t f = fw_factory, pr = f ? word(f, 0x28) : 0;
    return f && is_a(word(f, 0x88), VP_ENGINE) && is_a(word(f, 0x90), VP_HUB) && is_a(word(f, 0x18), VP_LOADER) &&
           is_a(pr, VP_PROJECT) && is_a(word(pr, 0x1278), VP_POOL) ? f : 0;
}
/* fn(x0, x1) with x8 = out: C++ returns a shared_ptr through x8. */
__attribute__((visibility("hidden"))) void hs_call_x8(uintptr_t fn, uintptr_t a, uintptr_t b, void *out);
__asm__(".text\n.type hs_call_x8,%function\nhs_call_x8:\n"
        "stp x29, x30, [sp, #-16]!\nmov x29, sp\nmov x16, x0\nmov x0, x1\nmov x1, x2\nmov x8, x3\nblr x16\n"
        "ldp x29, x30, [sp], #16\nret\n");

struct onto {                        /* one request to the message thread */
    int load;                        /* 0: just read the current track */
    uintptr_t track;                 /* in: the track it must still be (0 any); out: the current one */
    int pad;                         /* load: the target pad */
    const char *path, *name;         /* load: the WAV, its name in the project (made unique here) */
    const char *why;                 /* out: NULL = done */
};
static void *on_message_thread(void *arg) {
    struct onto *o = arg;
    uintptr_t f = fw_ok(), pr = f ? word(f, 0x28) : 0;
    if (!f) { o->why = "Not on this firmware"; return NULL; }
    uintptr_t track = word(pr, PROJECT_TRACK);
    if (!o->load) { o->track = track; o->why = NULL; return NULL; }
    if (!track || (o->track && track != o->track)) { o->why = "Track changed"; return NULL; }
    void (*str)(void *, const char *) = (void *)(fw_base + STRING_CTOR), (*unstr)(void *) = (void *)(fw_base + STRING_DTOR);
    void (*sp_release)(uintptr_t) = (void *)(fw_base + SP_RELEASE);
    uintptr_t pool = word(pr, 0x1278);
    /* a free name: the pool finds samples by name, and loadSample assigns the one it finds */
    char name[300]; void *jname = 0; uintptr_t found[2];
    for (int k = 1; k < 100; ++k) {
        snprintf(name, sizeof name, k == 1 ? "%s" : "%s-%d", o->name, k);
        str(&jname, name);
        found[0] = found[1] = 0;
        hs_call_x8(fw_base + POOL_FIND, pool, (uintptr_t)&jname, found);
        if (found[1]) sp_release(found[1]);
        if (!found[0]) break;
        unstr(&jname); jname = 0;
    }
    if (!jname) { o->why = "Name taken"; return NULL; }
    static _Alignas(16) uintptr_t reporter[4];     /* commands keep it for undo/redo: never freed */
    if (!reporter[0]) ((void (*)(void *, uintptr_t))(fw_base + REPORTER))(reporter, word(f, 0x90));
    /* make_shared<FileResource>(File(path), keep=true): control block + the object (0x38 bytes, as the app) */
    uintptr_t *cb = malloc(0x38);
    void *file = 0;
    if (!cb) { unstr(&jname); o->why = "No memory"; return NULL; }
    cb[0] = fw_base + FILE_RES_CB; cb[1] = 0x100000001ull;
    str(&file, o->path);
    ((void (*)(void *, void *, int))(fw_base + FILE_RES))(cb + 2, &file, 1);
    unstr(&file);
    struct { uintptr_t engine, hub, reporter, pool, loader, res, cb; void *name; int pad, layer, flags; } p = {
        word(f, 0x88), word(f, 0x90), (uintptr_t)reporter, pool, word(f, 0x18), (uintptr_t)(cb + 2), (uintptr_t)cb, jname, o->pad, -1, 0 };
    _Static_assert(sizeof p == 0x50, "the firmware's LoadParams");
    ((void (*)(uintptr_t, void *))(fw_base + LOAD_SAMPLE))(f, &p);
    sp_release((uintptr_t)cb);       /* the command keeps its own reference */
    found[0] = found[1] = 0;
    hs_call_x8(fw_base + POOL_FIND, pool, (uintptr_t)&jname, found);   /* loaded = in the pool under its name */
    if (found[1]) sp_release(found[1]);
    unstr(&jname);
    o->why = found[0] ? NULL : "Not loaded";
    return NULL;
}
/* Run on the message thread and wait for it. The caller holds no lock (the message thread takes ours). */
static const char *onto(struct onto *o) {
    if (!fw_base) dl_iterate_phdr(fw_base_cb, 0);
    if (!fw_base || memcmp((const void *)(fw_base + FW_NOTE), FW_VERSION, sizeof FW_VERSION)) return "Not on this firmware";
    o->why = "App busy";             /* stays if the app is quitting and never runs it */
    uintptr_t mm = ((uintptr_t (*)(void))(fw_base + MM_INSTANCE))();
    if (mm) ((void *(*)(uintptr_t, void *(*)(void *), void *))(fw_base + MM_CALL))(mm, on_message_thread, o);
    return o->why;
}

/* Plain C copy through <dst>.new + rename (no system(): its shell would inherit LD_PRELOAD). 0 = done. */
static int copy_file(const char *src, const char *dst, mode_t mode) {
    char tmp[1700]; snprintf(tmp, sizeof tmp, "%s.new", dst);
    int in = open(src, O_RDONLY), out = in < 0 ? -1 : open(tmp, O_WRONLY | O_CREAT | O_TRUNC, mode);
    char b[65536]; ssize_t n = 0;
    while (out >= 0 && (n = read(in, b, sizeof b)) > 0 && write(out, b, n) == n) {}
    int cl = out >= 0 ? close(out) : -1;
    int bad = in < 0 || out < 0 || n != 0 || cl || rename(tmp, dst);
    if (in >= 0) close(in);
    if (bad) unlink(tmp);
    return bad;
}

/* After a good pad job: each stem onto the next free pad in the source pad's bank, on the track the
   job started on. The note says where they went. */
static void place_stems(int track, int src_pad, uintptr_t start_track, const char *dir, const char *base,
                        int ticks, int sum, char *note, size_t nn) {
    char names[4][300]; int want = 0, pads[4], got = 0;
    while (want < 4 && !stems_out_name(base, ticks, sum, want, names[want], sizeof names[want])) ++want;
    struct onto o = {0};
    const char *why = fresh_autosave(__atomic_load_n(&last_input_rt, __ATOMIC_RELAXED));
    if (!why) why = onto(&o);                          /* the current track, now */
    if (!why && o.track != start_track) why = "Track changed";
    char *x = why ? NULL : read_xpj();
    if (!why && !x) why = "Can't read the project";
    int n = why ? -1 : stems_free_pads(x, track, src_pad, src_pad, pads, want, &why);
    free(x);
    for (int i = 0; i < n && !why; ++i) {
        /* as the stock browser: the project gets its own copy in its data folder (else the next start
           says "Sample not found"); a free file name there, the pool name is made free on load */
        char src[1600], path[1700], name[300]; struct stat st;
        snprintf(src, sizeof src, "%s/%s", dir, names[i]);
        for (int k = 1; k < 100; ++k) {
            snprintf(name, sizeof name, k == 1 ? "%.*s" : "%.*s-%d", (int)strlen(names[i]) - 4, names[i], k);
            snprintf(path, sizeof path, "%s/%s.wav", PROJECT_DATA, name);
            if (stat(path, &st)) break;
        }
        if (!stat(path, &st) || copy_file(src, path, 0644)) { why = "Can't copy to the project"; break; }
        struct onto l = {.load = 1, .track = o.track, .pad = pads[i], .path = path, .name = name};
        if ((why = onto(&l))) { unlink(path); break; }
        ++got;
    }
    ulog("pads: %d of %d placed%s%s", got, want, why ? ": " : "", why ? why : "");
    if (!why || got) stems_pads_note(pads, got, want, note, nn);
    else snprintf(note, nn, "Not on pads: %.40s", why);
}

static void *job(void *unused) {
    (void)unused;
    struct sched_param sp = {0};    /* not the MIDI thread's RT policy it was created from */
    sched_setscheduler(0, SCHED_IDLE, &sp);
    char path[sizeof job_path], dir[sizeof job_dir], base[sizeof job_base], pick[8];
    pthread_mutex_lock(&lock);
    int src = job_src, track = job_track, pad = job_pad, ticks = job_ticks, sum = job_sum; long input_rt = job_input_rt;
    memcpy(path, job_path, sizeof path); memcpy(dir, job_dir, sizeof dir); memcpy(base, job_base, sizeof base);
    pthread_mutex_unlock(&lock);
    stems_pick(ticks, sum, pick, sizeof pick);

    const char *src_dir = getenv("HS_STEMS_DIR");
    char bin_src[1024], weights[1024], msg[96] = "", note[96] = "";
    int ok = 0, made = 0, fds[2] = {-1, -1};
    pid_t pid = 0;
    if (src == PAD) {
        const char *why = pad_source(track, pad, input_rt, path, sizeof path, base, sizeof base, dir, sizeof dir);
        if (why) { snprintf(msg, sizeof msg, "%s", why); goto out; }
        pthread_mutex_lock(&lock); memcpy(job_base, base, sizeof job_base); job_pct = 0; job_t0 = now_ms(); repaint_locked(); pthread_mutex_unlock(&lock);
        snprintf(note, sizeof note, "%.80s Stems", base);
    }
    struct onto now = {0};          /* the track the job starts on: its stems go back to it */
    /* ponytail: pad jobs only; a browser job's track isn't on its screen, so its stems stay on disk */
    const char *unplaceable = src == PAD ? onto(&now) : "browser job";
    if (unplaceable && src == PAD) ulog("pads: %s", unplaceable);
    snprintf(bin_src, sizeof bin_src, "%s/hs-stems", src_dir ? src_dir : "");
    snprintf(weights, sizeof weights, "%s/spl4h.bin", src_dir ? src_dir : "");
    /* The card is noexec: run a copy from /tmp (made again when the card's copy changes). Plain C
       copy: system() would fork a shell that inherits the app's LD_PRELOAD. */
    struct stat st, ss;
    if (stat("/tmp/hs-stems", &st) || stat(bin_src, &ss) || st.st_size != ss.st_size || ss.st_mtime > st.st_mtime) {
        if (!src_dir || copy_file(bin_src, "/tmp/hs-stems", 0755)) { snprintf(msg, sizeof msg, "hs-stems is missing"); goto out; }
    }
    made = !mkdir(dir, 0755);       /* only a folder this run made may be emptied on failure */
    if (!made && errno != EEXIST) { snprintf(msg, sizeof msg, "Cannot make the folder"); goto out; }
    if (pipe2(fds, O_CLOEXEC)) { snprintf(msg, sizeof msg, "No pipe"); goto out; }
    /* environment without LD_PRELOAD: the child must not load the app's hooks */
    char *env[64]; int ne = 0;
    for (char **e = environ; *e && ne < 60; ++e) if (strncmp(*e, "LD_PRELOAD=", 11)) env[ne++] = *e;
    env[ne++] = "OPENBLAS_NUM_THREADS=2"; env[ne] = NULL;
    char *argv[] = {"/tmp/hs-stems", weights, path, dir, base, pick, NULL};
    posix_spawn_file_actions_t fa; posix_spawn_file_actions_init(&fa);
    posix_spawn_file_actions_adddup2(&fa, fds[1], 1); posix_spawn_file_actions_adddup2(&fa, fds[1], 2);
    int rc = posix_spawn(&pid, argv[0], &fa, NULL, argv, env);
    posix_spawn_file_actions_destroy(&fa);
    close(fds[1]);
    if (rc) { close(fds[0]); pid = 0; snprintf(msg, sizeof msg, "Cannot start hs-stems"); goto out; }
    pthread_mutex_lock(&lock); job_pid = pid; if (job_cancel) kill(pid, SIGTERM); pthread_mutex_unlock(&lock);

    FILE *f = fdopen(fds[0], "r"); char l[512], last[512] = "";
    while (f && fgets(l, sizeof l, f)) {
        long a, b;
        if (sscanf(l, "progress %ld/%ld", &a, &b) == 2 && b > 0) set_pct(a * 100 / b > 99 ? 99 : (int)(a * 100 / b));
        else if (!strncmp(l, "hs-stems: ", 10)) snprintf(last, sizeof last, "%s", l + 10);
    }
    if (f) fclose(f);
    int ws = 0; waitpid(pid, &ws, 0);
    pthread_mutex_lock(&lock); int stopped = job_cancel; job_pid = 0; pthread_mutex_unlock(&lock);
    ok = WIFEXITED(ws) && !WEXITSTATUS(ws);
    if (ok) {
        for (int s = 0; s < 4; ++s)
            if (ticks >> s & 1) snprintf(msg + strlen(msg), sizeof msg - strlen(msg), "%s%s", !*msg ? "" : sum ? "+" : ", ", (const char *[]){"Vocals", "Drums", "Bass", "Other"}[s]);
        if (sum) snprintf(msg + strlen(msg), sizeof msg - strlen(msg), " in one file");
        if (!unplaceable) { set_pct(100); place_stems(track, pad, now.track, dir, base, ticks, sum, note, sizeof note); }
        else if (src == PAD) snprintf(note, sizeof note, "Not on pads: %.40s", unplaceable);
    } else {
        last[strcspn(last, "\n")] = 0;
        snprintf(msg, sizeof msg, "%s", stopped ? "Cancelled" : *last ? last : "Failed");
        *note = 0;
        /* remove this run's .part files; finals and the folder only if this run made the folder
           (an earlier good result in an existing folder stays) */
        char name[600], p[1900];
        for (int i = 0; !stems_out_name(base, ticks, sum, i, name, sizeof name); ++i) {
            snprintf(p, sizeof p, "%s/%s.part", dir, name); unlink(p);
            if (made) { snprintf(p, sizeof p, "%s/%s", dir, name); unlink(p); }
        }
        if (made) rmdir(dir);
    }
out:
    if (!ok && !pid && made) rmdir(dir);
    ulog("job %s -> %s (%s)", *path ? path : "?", ok ? "ok" : "failed", msg);
    pthread_mutex_lock(&lock);
    state = ok ? DONE : FAILED; snprintf(job_msg, sizeof job_msg, "%s", msg); snprintf(job_note, sizeof job_note, "%s", ok ? note : "");
    done_ms = now_ms();
    repaint_locked();
    long mine = done_ms;
    pthread_mutex_unlock(&lock);
    if (ok) {                       /* the "Saved" box goes by itself; a failure stays until a press */
        sleep(4);
        pthread_mutex_lock(&lock);
        if (state == DONE && done_ms == mine) { state = IDLE; repaint_locked(); }
        pthread_mutex_unlock(&lock);
    }
    return NULL;
}

/* Caller holds the lock. */
static void open_chooser(int src) {
    ch_src = src; ch_sel = 0; state = CHOOSE;
    if (src == PAD) {
        ch_track = pad_track; ch_pad = pad_idx;
        snprintf(ch_title, sizeof ch_title, "Stems: pad %c%02d", 'A' + pad_idx / 16, pad_idx % 16 + 1);
    } else {
        snprintf(ch_path, sizeof ch_path, "%s", seen_path);
        const char *s = strrchr(ch_path, '/'); s = s ? s + 1 : ch_path;
        snprintf(ch_title, sizeof ch_title, "Stems: %.*s", (int)(strlen(s) > 4 ? strlen(s) - 4 : strlen(s)), s);
    }
}
/* Caller holds the lock. Go from the chooser. */
static void start_job(void) {
    job_src = ch_src; job_track = ch_track; job_pad = ch_pad; job_ticks = ch_ticks; job_sum = ch_sum; job_input_rt = ch_input_rt;
    *job_path = *job_dir = *job_note = 0;
    if (ch_src == BROWSER) {
        snprintf(job_path, sizeof job_path, "%s", ch_path);
        if (stems_out(job_path, job_dir, sizeof job_dir, job_base, sizeof job_base)) {
            state = FAILED; snprintf(job_base, sizeof job_base, "?"); snprintf(job_msg, sizeof job_msg, "Name too long"); return;
        }
    } else snprintf(job_base, sizeof job_base, "pad %c%02d", 'A' + ch_pad / 16, ch_pad % 16 + 1);
    state = RUNNING; job_pct = 0; job_t0 = now_ms(); job_cancel = 0; job_pid = 0;
    pthread_t t;
    if (pthread_create(&t, NULL, job, NULL)) { state = FAILED; snprintf(job_msg, sizeof job_msg, "No thread"); return; }
    pthread_detach(t);
    ulog("start %s", ch_src == PAD ? job_base : job_path);
}

/* ---------- panel input ---------- */
typedef struct _snd_rawmidi snd_rawmidi_t;
static snd_rawmidi_t *panel_midi;
static unsigned char st, d1;
static int nd, drop_msg;           /* drop_msg: midi_filter removes the message just read */

static int surface_card(void) {
    FILE *f = fopen("/proc/asound/cards", "r"); char l[256]; int n, card = -1;
    if (!f) return -1;
    while (fgets(l, sizeof l, f)) if (sscanf(l, " %d [", &n) == 1 && strstr(l, "Control Surface")) { card = n; break; }
    fclose(f);
    return card;
}
static int panel_name(const char *name) {
    if (!name) return 0;
    if (strstr(name, "Surface")) return 1;
    int card; return sscanf(name, "hw:%d", &card) == 1 && card == surface_card();
}
int snd_rawmidi_open(snd_rawmidi_t **in, snd_rawmidi_t **out, const char *name, int mode) {
    static int (*real)(snd_rawmidi_t **, snd_rawmidi_t **, const char *, int);
    if (!real) real = (int (*)(snd_rawmidi_t **, snd_rawmidi_t **, const char *, int))dlsym(RTLD_NEXT, "snd_rawmidi_open");
    int r = real(in, out, name, mode);
    if (!r && in && *in && panel_name(name)) panel_midi = *in;
    return r;
}
static void *hold_wait(void *arg) {
    unsigned gen = (unsigned)(uintptr_t)arg;
    struct timespec t = {HOLD_MS / 1000, (HOLD_MS % 1000) * 1000000L};
    while (nanosleep(&t, &t) && errno == EINTR) {}
    pthread_mutex_lock(&lock);
    if (gen == hold_gen && hold_note && state == IDLE && shown && shown_kind == hold_kind) {
        open_chooser(hold_kind);
        if (hold_kind == PAD) ch_input_rt = hold_prev_rt;
        hold_fired = hold_note; hold_note = 0;
        repaint_locked();
    }
    pthread_mutex_unlock(&lock);
    return NULL;
}
static void start_hold(int note, int kind, long prev_rt) {     /* caller holds the lock */
    hold_note = note; hold_kind = kind; hold_prev_rt = prev_rt; hold_fired = 0;
    pthread_t t; pthread_attr_t a;
    pthread_attr_init(&a); pthread_attr_setdetachstate(&a, PTHREAD_CREATE_DETACHED);
    if (pthread_create(&t, &a, hold_wait, (void *)(uintptr_t)++hold_gen)) { hold_note = 0; replay_note = (unsigned char)note; }
    pthread_attr_destroy(&a);
    repaint_locked();
}
static void end_hold(void) { hold_note = 0; ++hold_gen; repaint_locked(); }   /* caller holds the lock */

/* One byte; returns the byte the app should see. */
static unsigned char midi_byte(unsigned char c) {
    if (c >= 0xf8) return c;
    if (c & 0x80) { st = c; nd = 0; return c; }
    if (st < 0x80 || st >= 0xf0 || (st & 0xe0) == 0xc0) return c;
    if (!nd) { d1 = c; nd = 1; return c; }
    nd = 0;
    if ((st & 0xf0) == 0xb0) {                /* knob, encoder or fader */
        if (st == 0xb0 && d1 == DATA_CC && __atomic_load_n(&state, __ATOMIC_RELAXED) == CHOOSE) {
            pthread_mutex_lock(&lock);
            if (state == CHOOSE && c) { ch_sel = (ch_sel + (c < 64 ? 1 : 3)) % 4; repaint_locked(); c = 0; drop_msg = 1; }  /* 0 = no step */
            pthread_mutex_unlock(&lock);
            return c;
        }
        __atomic_store_n(&last_input_rt, wall_ms(), __ATOMIC_RELAXED);
        if (__atomic_load_n(&state, __ATOMIC_RELAXED) < DONE) return c;   /* no lock on every CC */
        pthread_mutex_lock(&lock);
        if (state == DONE || state == FAILED) { state = IDLE; repaint_locked(); }   /* also clears a result box */
        pthread_mutex_unlock(&lock);
        return c;
    }
    if (st != 0x90 && st != 0x80) return c;   /* buttons are notes on channel 1 (pads: channel 10, untouched) */
    int on = st == 0x90 && c;
    pthread_mutex_lock(&lock);
    if (d1 == SHIFT_NOTE) {
        shift_held = on;
        if (!on && hold_note) end_hold();   /* SHIFT let go first: no Stems, and no replay (it was SHIFT+B) */
        repaint_locked();
    } else if (!on && hold_note && d1 == hold_note) {
        replay_note = (unsigned char)d1;    /* short press: the app gets press + release after this read */
        end_hold();
    } else if (!on && hold_fired && d1 == hold_fired) {
        hold_fired = 0;                     /* the release of a hold that opened Stems: the app never saw its press */
    } else if (on) {
        long prev_rt = last_input_rt;           /* the press that opens the chooser isn't an edit */
        if (state != CHOOSE) last_input_rt = wall_ms();
        if (state == DONE || state == FAILED) { state = IDLE; repaint_locked(); }   /* any press clears the result box */
        if (state == CHOOSE) {                  /* the chooser takes B1-B3, the wheel push and -/+ */
            switch (d1) {
            case B1_NOTE: state = IDLE; c = 0; break;
            case B2_NOTE: ch_sum ^= 1; c = 0; break;
            case B3_NOTE: if (ch_ticks) start_job(); c = 0; break;
            case PUSH_NOTE: ch_ticks ^= 1 << ch_sel; c = 0; break;
            case PLUS_NOTE: ch_sel = (ch_sel + 1) % 4; c = 0; break;
            case MINUS_NOTE: ch_sel = (ch_sel + 3) % 4; c = 0; break;
            }
            repaint_locked();
        } else if (state == RUNNING && d1 == B1_NOTE && shown) {   /* Cancel, only while our box is on the glass */
            job_cancel = 1; if (job_pid > 0) kill(job_pid, SIGTERM);
            c = 0;
        } else if (state == IDLE && shift_held && !hold_note && d1 == B1_NOTE && shown_kind == BROWSER && shown && armed()) {
            start_hold(B1_NOTE, BROWSER, prev_rt); c = 0;
        } else if (state == IDLE && shift_held && !hold_note && d1 == B3_NOTE && shown_kind == PAD && shown) {
            start_hold(B3_NOTE, PAD, prev_rt); c = 0;
        }
    }
    pthread_mutex_unlock(&lock);
    return c;
}

/* Filters in place; a pending short press is appended as note on + off (the panel always sends status
   bytes, so appending whole messages can't confuse running status). */
static ssize_t midi_filter(void *buf, ssize_t n, size_t size) {
    unsigned char *b = buf;
    ssize_t w = 0, sw = -1;                    /* sw: where the current message's status byte was written */
    for (ssize_t i = 0; i < n; ++i) {
        if (b[i] >= 0x80 && b[i] < 0xf8) sw = w;
        b[w++] = midi_byte(b[i]);
        /* A wheel step the chooser took: drop the whole message when it all came in this read, because
           even a value-0 step leaves an empty "Set Start" in the app's UNDO. Split across reads, the
           value-0 step stays. */
        if (drop_msg) { drop_msg = 0; if (sw >= 0 && w - sw == 3) w = sw; }
    }
    n = w;
    pthread_mutex_lock(&lock);
    unsigned char r = replay_note; replay_note = 0;
    pthread_mutex_unlock(&lock);
    if (r) {
        if ((size_t)n + 6 <= size) {
            unsigned char m[6] = {0x90, r, 0x7f, 0x80, r, 0x00};
            memcpy(b + n, m, 6); n += 6;
        } else ulog("hold: no room to replay note %d", r);
    }
    return n;
}
ssize_t snd_rawmidi_read(snd_rawmidi_t *rm, void *buf, size_t size) {
    static ssize_t (*real)(snd_rawmidi_t *, void *, size_t);
    if (!real) real = (ssize_t (*)(snd_rawmidi_t *, void *, size_t))dlsym(RTLD_NEXT, "snd_rawmidi_read");
    ssize_t n = real(rm, buf, size);
    return rm == panel_midi && n > 0 ? midi_filter(buf, n, size) : n;
}
ssize_t snd_rawmidi_tread(snd_rawmidi_t *rm, struct timespec *ts, void *buf, size_t size) {
    static ssize_t (*real)(snd_rawmidi_t *, struct timespec *, void *, size_t);
    if (!real) real = (ssize_t (*)(snd_rawmidi_t *, struct timespec *, void *, size_t))dlsym(RTLD_NEXT, "snd_rawmidi_tread");
    ssize_t n = real(rm, ts, buf, size);
    return rm == panel_midi && n > 0 ? midi_filter(buf, n, size) : n;
}

/* ---------- which file the browser previews ---------- */
static void saw(const char *path) {
    if (!stems_is_wav(path) || strlen(path) >= sizeof seen_path) return;
    pthread_mutex_lock(&lock);
    if (state != RUNNING || strcmp(path, job_path)) {
        snprintf(seen_path, sizeof seen_path, "%s", path);
        seen_ms = now_ms();
        if (shown) seen_row = stems_row(clean, W);   /* a later flip within CAPTURE_MS corrects it */
    }
    pthread_mutex_unlock(&lock);
}
#define OPEN_HOOK(name)                                                                   \
    int name(const char *p, int fl, ...) {                                                \
        static int (*real)(const char *, int, ...);                                       \
        if (!real) real = (int (*)(const char *, int, ...))dlsym(RTLD_NEXT, #name);       \
        mode_t m = 0;                                                                     \
        if (__OPEN_NEEDS_MODE(fl)) { va_list a; va_start(a, fl); m = va_arg(a, mode_t); va_end(a); } \
        int r = real(p, fl, m);                                                           \
        if (r >= 0 && !(fl & (O_WRONLY | O_RDWR))) saw(p);                                \
        return r;                                                                         \
    }
OPEN_HOOK(open)
OPEN_HOOK(open64)
int openat(int dfd, const char *p, int fl, ...) {
    static int (*real)(int, const char *, int, ...);
    if (!real) real = (int (*)(int, const char *, int, ...))dlsym(RTLD_NEXT, "openat");
    mode_t m = 0;
    if (__OPEN_NEEDS_MODE(fl)) { va_list a; va_start(a, fl); m = va_arg(a, mode_t); va_end(a); }
    int r = real(dfd, p, fl, m);
    if (r >= 0 && !(fl & (O_WRONLY | O_RDWR))) saw(p);
    return r;
}
FILE *fopen(const char *p, const char *mode) {
    static FILE *(*real)(const char *, const char *);
    if (!real) real = (FILE *(*)(const char *, const char *))dlsym(RTLD_NEXT, "fopen");
    FILE *f = real(p, mode);
    if (f && mode && mode[0] == 'r' && !strchr(mode, '+')) saw(p);
    return f;
}
FILE *fopen64(const char *p, const char *mode) {
    static FILE *(*real)(const char *, const char *);
    if (!real) real = (FILE *(*)(const char *, const char *))dlsym(RTLD_NEXT, "fopen64");
    FILE *f = real(p, mode);
    if (f && mode && mode[0] == 'r' && !strchr(mode, '+')) saw(p);
    return f;
}
#endif
