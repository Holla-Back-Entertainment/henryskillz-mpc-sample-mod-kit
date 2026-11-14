// hs_fx_flip.c: keeps an HS effect editor (and the HS mixer console) on the glass while the app
// redraws underneath it.
//
// Every pixel of an HS editor is SampQOL overlay (header 26, body 27, knobs 28), and SampQOL paints
// those only onto the app buffer already on the glass, after the fact. So during playback each new
// app frame went out bare, the stock Trim waveform showing under the faceplate until the next push:
// the flicker (14 of 30 grabs on HS Para EQ with a pad playing, 14 Nov 2026).
//
// While an HS editor is up, the page-flip ioctl is pointed at a copy of what is on the glass now,
// in one of two buffers of our own, so the faceplate never leaves the screen; every DIRTYFB SampQOL
// sends for an app buffer is mirrored into ours (the clip rects only). The swap is done at the ioctl,
// below every drmModePageFlip hook: SampQOL must keep seeing the app's buffer ids, or it falls back
// to a full redraw path that drops the editor (tried first, 14 Nov). The app's buffers are never
// written, and no lock is held across the real flip or a DIRTYFB.
#define _GNU_SOURCE
#include <dlfcn.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

int hs_fx_editor_up(void);          /* hs_fx.c */

enum { SW = 320, SH = 240, MAX_CLIPS = 16 };
struct fbcmd { uint32_t fb_id, width, height, pitch, bpp, depth, handle; };
struct mapdumb { uint32_t handle, pad; uint64_t offset; };
struct mkdumb { uint32_t height, width, bpp, flags, handle, pitch; uint64_t size; };
struct cliprect { uint16_t x1, y1, x2, y2; };
struct dirtycmd { uint32_t fb_id, flags, color, num_clips; uint64_t clips_ptr; };
#define GETFB       _IOWR('d', 0xAD, struct fbcmd)
#define ADDFB       _IOWR('d', 0xAE, struct fbcmd)
#define CREATE_DUMB _IOWR('d', 0xB2, struct mkdumb)
#define MAP_DUMB    _IOWR('d', 0xB3, struct mapdumb)
#define DIRTYFB     _IOWR('d', 0xB1, struct dirtycmd)
struct flipcmd { uint32_t crtc_id, fb_id, flags, reserved; uint64_t user_data; };
#define PAGE_FLIP   _IOWR('d', 0xB0, struct flipcmd)
struct gemclose { uint32_t handle, pad; };
#define GEM_CLOSE   _IOW('d', 0x09, struct gemclose)
typedef int (*ioctl_fn)(int, unsigned long, ...);

struct map { uint32_t id, stride; uint32_t *px; };

static ioctl_fn real_ioctl;
static int sys_ioctl(int fd, unsigned long req, void *arg)
{
    if (!real_ioctl) real_ioctl = (ioctl_fn)dlsym(RTLD_NEXT, "ioctl");
    return real_ioctl(fd, req, arg);
}

static uint32_t *map_handle(int fd, uint32_t handle, size_t len)
{
    struct mapdumb m = { .handle = handle };
    if (sys_ioctl(fd, MAP_DUMB, &m)) return NULL;
    void *p = mmap(0, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, m.offset);
    return p == MAP_FAILED ? NULL : p;
}

/* Maps and the two buffers are touched only by the flipping thread, and by the ioctl hook under lock. */
static struct map own[2];
static struct map cache[16];       /* ponytail: never evicted; a reused fb id would map stale pixels */

static struct map *fb_map(int fd, uint32_t id)
{
    for (int i = 0; i < 2; ++i) if (own[i].px && own[i].id == id) return &own[i];
    for (int i = 0; i < 8; ++i) if (cache[i].px && cache[i].id == id) return &cache[i];
    struct fbcmd f = { .fb_id = id };
    if (sys_ioctl(fd, GETFB, &f) || f.bpp != 32 || f.width != SW || f.height != SH || !f.handle) return NULL;
    uint32_t *p = map_handle(fd, f.handle, (size_t)f.pitch * SH);
    struct gemclose g = { .handle = f.handle };
    sys_ioctl(fd, GEM_CLOSE, &g);       /* GETFB hands us a new handle each time; the mmap keeps its own ref */
    if (!p) return NULL;
    for (int i = 0; i < 16; ++i)
        if (!cache[i].px) { cache[i] = (struct map){ id, f.pitch / 4, p }; return &cache[i]; }
    munmap(p, (size_t)f.pitch * SH);
    return NULL;
}

static struct map *own_fb(int fd, uint32_t avoid)     /* ours, never the one on the glass */
{
    static int ready, failed;
    if (failed) return NULL;            /* tried once: never retry (and leak) at frame rate */
    if (!ready) {
        for (int i = 0; i < 2; ++i) {
            struct mkdumb c = { .height = SH, .width = SW, .bpp = 32 };
            struct fbcmd f = { .width = SW, .height = SH, .bpp = 32, .depth = 24 };
            uint32_t *p = NULL;
            if (!sys_ioctl(fd, CREATE_DUMB, &c)) {
                f.pitch = c.pitch; f.handle = c.handle;
                if (!sys_ioctl(fd, ADDFB, &f)) p = map_handle(fd, c.handle, c.size);
            }
            if (!p) { failed = 1; return NULL; }   /* ponytail: the half-made buffer stays; one-off */
            own[i] = (struct map){ f.fb_id, c.pitch / 4, p };
        }
        ready = 1;
    }
    return own[0].id == avoid ? &own[1] : &own[0];
}

static void copy_rect(struct map *to, const struct map *from, int x0, int y0, int x1, int y1)
{
    x0 = x0 < SW ? x0 : SW; x1 = x1 < SW ? x1 : SW; y1 = y1 < SH ? y1 : SH;
    for (int y = y0; y < y1 && x0 < x1; ++y)
        memcpy(to->px + (size_t)y * to->stride + x0, from->px + (size_t)y * from->stride + x0, (size_t)(x1 - x0) * 4);
}

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;   /* guards everything below and the maps */
static uint32_t glass_id;           /* last buffer flipped, any owner */
static struct map *shown;           /* ours, on the glass (or about to be) */
static int shown_fd;

static int page_flip(int fd, struct flipcmd *f)
{
    uint32_t app = f->fb_id;
    struct map *o = NULL, *before = NULL;
    static int (*mixer_up)(void);       /* hs-mixer.so's graphic console, if loaded */
    if (!mixer_up) mixer_up = (int (*)(void))dlsym(RTLD_DEFAULT, "hs_mixer_up");
    int up = hs_fx_editor_up() || (mixer_up && mixer_up());   /* calls into SampQOL: never under our lock */
    pthread_mutex_lock(&lock);
    before = shown;
    uint32_t was = glass_id;
    if (glass_id && up) {
        struct map *from = fb_map(fd, glass_id);
        if (from && (o = own_fb(fd, glass_id))) {
            copy_rect(o, from, 0, 0, SW, SH);
            __atomic_store_n(&shown, o, __ATOMIC_RELAXED);   /* mirrored paints land in o from here */
            shown_fd = fd;
            glass_id = o->id;           /* a second flipper copies from o, never over it */
        }
    }
    pthread_mutex_unlock(&lock);
    if (o) f->fb_id = o->id;
    int rc = sys_ioctl(fd, PAGE_FLIP, f);
    f->fb_id = app;
    pthread_mutex_lock(&lock);
    uint32_t redo = 0; int rfd = shown_fd;
    if (!rc) glass_id = o ? o->id : app;
    else if (o) {                       /* failed: the old frame stays up; give it what landed in o */
        glass_id = was;
        if (before) { copy_rect(before, o, 0, 0, SW, SH); redo = before->id; }
    }
    __atomic_store_n(&shown, rc ? before : o, __ATOMIC_RELAXED);
    pthread_mutex_unlock(&lock);
    if (redo) { struct dirtycmd d = { .fb_id = redo }; sys_ioctl(rfd, DIRTYFB, &d); }
    return rc;
}

/* A paint landed on an app buffer while ours is on the glass: copy its clips into ours, show them. */
static void mirror(const struct dirtycmd *d)
{
    struct cliprect clips[MAX_CLIPS];
    unsigned n = d->num_clips <= MAX_CLIPS && d->clips_ptr ? d->num_clips : 0;
    if (n) memcpy(clips, (const void *)(uintptr_t)d->clips_ptr, n * sizeof *clips);
    pthread_mutex_lock(&lock);
    uint32_t mine = 0; int fd = shown_fd;
    struct map *from = shown && d->fb_id != shown->id && d->fb_id != own[0].id && d->fb_id != own[1].id
                       ? fb_map(fd, d->fb_id) : NULL;
    if (from) {
        if (!n) copy_rect(shown, from, 0, 0, SW, SH);
        for (unsigned i = 0; i < n; ++i) copy_rect(shown, from, clips[i].x1, clips[i].y1, clips[i].x2, clips[i].y2);
        mine = shown->id;
    }
    pthread_mutex_unlock(&lock);
    if (mine) {
        struct dirtycmd ours = { .fb_id = mine, .num_clips = n, .clips_ptr = (uintptr_t)clips };
        sys_ioctl(fd, DIRTYFB, &ours);
    }
}

int ioctl(int fd, unsigned long req, ...)
{
    va_list ap; va_start(ap, req); void *arg = va_arg(ap, void *); va_end(ap);
    if (req == PAGE_FLIP && arg) return page_flip(fd, arg);
    int rc = sys_ioctl(fd, req, arg);
    if (req == DIRTYFB && arg && !rc && __atomic_load_n(&shown, __ATOMIC_RELAXED)) mirror(arg);
    return rc;
}
