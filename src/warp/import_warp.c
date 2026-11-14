// import_warp.c: warp on import. When a loop is loaded onto a pad, its pad warp is switched on, so it
// plays at the project tempo straight away.
//
// Stock firmware 1.3.0 tags every loaded sample with a tempo (its own BPM detection) but leaves the pad's
// warp off: its Preferences > "Audio Tracks Auto Warp" (WarpDefault) covers audio tracks only (measured on
// the unit, 15 Nov 2026).
// So after stock's LoadSampleCommand runs, this finds the pad that now holds the new Sample and sets its
// warp, the same byte stock's Warp control on the Tune page writes.
//
// Only loops: the sample must run at least a bar at its tempo and end on a whole beat. One-shots, short
// hits and stems whose tempo was misdetected (a fraction of a beat over) are left alone. Turning "Audio
// Tracks Auto Warp" off (WarpDefault 0 in the MPC settings file) turns this off too.
#define _GNU_SOURCE
#include <elf.h>
#include <fcntl.h>
#include <link.h>
#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

// link-time addresses in the stock app (build id below); add the load base
#define LOAD_VTABLE 0x6e40fa0                       // LoadSampleCommand
#define LOAD_SLOT (LOAD_VTABLE + 16 + 3 * 8)        // runs once per load, after the sample is on the pad
#define LOAD_SLOT_FN 0x199c110                      // what that slot holds on a stock app
#define SAMPLE_VPTR 0x6dbd868                       // Sample
#define SAMPLE_TEMPO 0x2941fa0                      // float (const Sample*)
// LoadSampleCommand: +0xa0 the new Sample*. Sample: +0x130 its audio buffer (int channels, frames, rate).
// A pad's tempo parameter T: float value, 0, 20.0, 999.0 (its range); the pad's Sample* at T+0x2e38;
// its warp (one byte, 0 or 1) at T-0xb0.
#define CMD_SAMPLE 0xa0
#define SAMPLE_BUFFER 0x130
#define PAD_SAMPLE 0x2e38
#define PAD_WARP 0xb0
#define SETTINGS "/data/Settings/Akai/MPC Sample/MPC Sample.settings"

static uintptr_t base;
static float (*sample_tempo)(const void *);

// A loop: whole bars at its tempo (4/4), ending within a tenth of a beat of a bar line. Breaks cut on the
// bar pass; a kick, an 808 or a stem tagged with the wrong tempo don't. ponytail: a long one-shot still
// lands on a bar line by chance about 1 time in 20; Warp is one knob away on the Tune page.
int import_warp_is_loop(long frames, int rate, float bpm)
{
    if (frames <= 0 || rate < 8000 || !(bpm >= 40 && bpm <= 300)) return 0;
    double beats = (double)frames / rate * bpm / 60;
    return beats >= 3.9 && fabs(beats - 4 * round(beats / 4)) <= 0.1;
}

static int ok(uintptr_t p)
{
    return p > 0x10000 && p < 0xffffffffffffull && !(p & 7) && !msync((void *)(p & ~0xfffUL), 4096, MS_ASYNC);
}

// Stock's "Audio Tracks Auto Warp" preference, which stock applies to audio tracks only. Missing file or key: on.
static int warp_default_on(void)
{
    FILE *f = fopen(SETTINGS, "r");
    char line[256];
    int on = 1;
    while (f && fgets(line, sizeof line, f))
        if (strstr(line, "name=\"WarpDefault\"")) { on = !strstr(line, "val=\"0\""); break; }
    if (f) fclose(f);
    return on;
}

// Reads through /proc/self/mem: the heap can shrink under a scan, and a read there fails instead of faulting.
static int peek(int mem, uintptr_t at, void *to, size_t n)
{
    return pread(mem, to, n, (off_t)at) == (ssize_t)n;
}

// The pad's tempo parameter for this Sample: one pass over [heap] (tens of ms on the unit), off the UI thread.
// ponytail: heap scan per load; swap for a program->pad pointer path if one turns up.
static uintptr_t find_pad(int mem, uintptr_t sample)
{
    static uintptr_t chunk[1 << 16];   // 512 KiB; only the one warp thread at a time uses it (see busy)
    FILE *m = fopen("/proc/self/maps", "r");
    char line[512];
    uintptr_t pad = 0;
    while (m && !pad && fgets(line, sizeof line, m)) {
        unsigned long a, b;
        char perm[5];
        if (!strstr(line, "[heap]") || sscanf(line, "%lx-%lx %4s", &a, &b, perm) != 3 || perm[1] != 'w') continue;
        for (uintptr_t at = a; at < b && !pad; at += sizeof chunk) {
            size_t n = b - at < sizeof chunk ? b - at : sizeof chunk;
            if (!peek(mem, at, chunk, n)) break;
            for (size_t i = 0; i < n / 8 && !pad; ++i) {
                uintptr_t t = at + i * 8 - PAD_SAMPLE;
                struct { float value; uint32_t zero; float lo, hi; } p;
                uint8_t warp;
                if (chunk[i] == sample && t - PAD_WARP >= a && peek(mem, t, &p, sizeof p) &&
                    p.zero == 0 && p.lo == 20.f && p.hi == 999.f && peek(mem, t - PAD_WARP, &warp, 1) && warp <= 1)
                    pad = t;
            }
        }
    }
    if (m) fclose(m);
    return pad;
}

static int busy;   // one scan at a time; a load during a scan is skipped (loads are seconds apart)
static float load_tempo;   // the loaded sample's tempo, read on the UI thread before the scan starts

static void *warp_pad(void *arg)
{
    uintptr_t sample = (uintptr_t)arg, pad;
    int mem = open("/proc/self/mem", O_RDWR | O_CLOEXEC);
    float tempo = load_tempo;
    if (mem >= 0 && warp_default_on() && (pad = find_pad(mem, sample))) {
        // Same tempo as the sample: this pad really took the load. Written like stock's Warp control does.
        float now;
        uintptr_t still;
        static const uint8_t on = 1;
        if (peek(mem, pad, &now, sizeof now) && now == tempo && peek(mem, pad + PAD_SAMPLE, &still, sizeof still) &&
            still == sample && pwrite(mem, &on, 1, (off_t)(pad - PAD_WARP)) != 1)
            fputs("import-warp: write failed\n", stderr);
    }
    if (mem >= 0) close(mem);
    __atomic_store_n(&busy, 0, __ATOMIC_RELEASE);
    return NULL;
}

__attribute__((visibility("hidden"), used)) void import_warp_after_load(uintptr_t cmd)
{
    if (!ok(cmd) || *(uintptr_t *)cmd != base + LOAD_VTABLE + 16) return;
    uintptr_t sample = *(uintptr_t *)(cmd + CMD_SAMPLE);
    if (!ok(sample) || *(uintptr_t *)sample != base + SAMPLE_VPTR) return;
    uintptr_t buffer = *(uintptr_t *)(sample + SAMPLE_BUFFER);
    if (!ok(buffer)) return;
    const int32_t *b = (const int32_t *)buffer;   // channels, frames, rate
    float tempo = sample_tempo((const void *)sample);
    if (!import_warp_is_loop(b[1], b[2], tempo)) return;
    if (__atomic_exchange_n(&busy, 1, __ATOMIC_ACQUIRE)) return;
    load_tempo = tempo;
    pthread_t t;
    if (pthread_create(&t, NULL, warp_pad, (void *)sample)) __atomic_store_n(&busy, 0, __ATOMIC_RELEASE);
    else pthread_detach(t);
}

#if defined(__aarch64__)   // the hook is for the unit; a host build keeps only the loop rule (check_import.c)
// Post-hook trampoline: the original returns an object through x8, so x8 must reach it intact.
__attribute__((visibility("hidden"))) uintptr_t import_warp_orig;
__asm__(".text\n.type import_warp_hook,%function\nimport_warp_hook:\n"
        "stp x29, x30, [sp, #-32]!\nmov x29, sp\nstr x0, [sp, #16]\n"
        "adrp x16, import_warp_orig\nldr x16, [x16, :lo12:import_warp_orig]\nblr x16\n"
        "str x0, [sp, #24]\nldr x0, [sp, #16]\nbl import_warp_after_load\nldr x0, [sp, #24]\n"
        "ldp x29, x30, [sp], #32\nret\n");
void import_warp_hook(void);

static const unsigned char build_id[] = {
    0x1d, 0xac, 0x7f, 0x48, 0x1a, 0x0f, 0xe8, 0xd1, 0x28, 0xe4,
    0x35, 0xac, 0xe8, 0x8d, 0x7e, 0x6a, 0xec, 0xe0, 0xc9, 0xc9
};

static int find_base(struct dl_phdr_info *i, size_t s, void *d)
{
    (void)s; (void)d;
    if (*i->dlpi_name) return 0;
    for (unsigned k = 0; k < i->dlpi_phnum; ++k) {
        const ElfW(Phdr) *seg = &i->dlpi_phdr[k];
        if (seg->p_type != PT_NOTE) continue;
        const unsigned char *note = (const void *)(i->dlpi_addr + seg->p_vaddr);
        size_t left = seg->p_filesz;
        while (left >= sizeof(ElfW(Nhdr))) {
            ElfW(Nhdr) h;
            memcpy(&h, note, sizeof h);
            size_t name = (h.n_namesz + 3) & ~3UL, desc = (h.n_descsz + 3) & ~3UL, len = sizeof h + name + desc;
            if (len > left) break;
            if (h.n_type == NT_GNU_BUILD_ID && h.n_namesz == 4 && h.n_descsz == sizeof build_id &&
                !memcmp(note + sizeof h, "GNU", 4) && !memcmp(note + sizeof h + name, build_id, sizeof build_id)) {
                base = i->dlpi_addr;
                return 1;
            }
            note += len;
            left -= len;
        }
    }
    return 1;  // the app is always first: any other build leaves base at 0 and the hook off
}

__attribute__((constructor)) static void install(void)
{
    dl_iterate_phdr(find_base, NULL);
    if (!base) return;
    uintptr_t *slot = (uintptr_t *)(base + LOAD_SLOT);
    if (*slot != base + LOAD_SLOT_FN) {
        fputs("import-warp: load command differs, off\n", stderr);
        return;
    }
    long pg = sysconf(_SC_PAGESIZE);
    void *page = (void *)((uintptr_t)slot & ~(uintptr_t)(pg - 1));
    if (pg <= 0 || mprotect(page, (size_t)pg, PROT_READ | PROT_WRITE)) return;
    sample_tempo = (float (*)(const void *))(base + SAMPLE_TEMPO);
    import_warp_orig = *slot;
    *slot = (uintptr_t)import_warp_hook;
    mprotect(page, (size_t)pg, PROT_READ);
    fputs("import-warp: installed\n", stderr);
}
#endif
