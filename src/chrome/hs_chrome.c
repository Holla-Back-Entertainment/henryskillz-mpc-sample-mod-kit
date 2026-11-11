// HS styling for SampQOL's own list screens (SampQOL 0.2.0 only).
//   mode 26 = header layer 320x31, 27 = list layer 320x160, 28 = soft-key bar 320x40.
//   - a red brand bar on the header (as on the channel mixer)
//   - a cyan edge on the selected row (SampQOL fills it with grey 46,46,51)
//   - the same dark blue palette on all three layers (stock_lut.h)
// Version guard: SampQOL's overlay_update must sit at its 0.2.0 offset in mpcqol.so; any other build
// and every call goes straight through, unstyled.
// Build: see build.sh at the top of the kit (gcc -O2 -Wall -Wextra -Werror -shared -fPIC hs_chrome.c -ldl)
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdint.h>
#include "stock_lut.h"

#define QOL_OVERLAY_UPDATE 0x61be4      /* overlay_update in mpcqol.so of SampQOL 0.2.0 */

typedef void (*ov_fn)(int, const void *, int);
static ov_fn real_ov;
static int matched = -1;                /* -1 not checked yet, 0 other build, 1 SampQOL 0.2.0 */
enum { W = 320 };
static const uint32_t RED = 0x00C8061Cu, CYAN = 0x004CB8E7u;

static inline int selgray(uint32_t p)
{
    unsigned r = (p >> 16) & 255, g = (p >> 8) & 255, b = p & 255;
    return (r >= 38 && r <= 58 && g >= 38 && g <= 58 && b >= 42 && b <= 66 && (r > g ? r - g : g - r) <= 6 && b >= g)
        || (p & 0xFFFFFFu) == 0x344252u;   /* the same row after navy(), for retained buffers */
}

static uint32_t lut(uint32_t c)
{
    uint32_t lo = 0, hi = LUT_N;
    while (lo < hi) { uint32_t m = (lo + hi) / 2; if (LUT_K[m] < c) lo = m + 1; else hi = m; }
    return (lo < LUT_N && LUT_K[lo] == c) ? LUT_V[lo] : c;
}

/* Exact layer sizes (SampQOL 0.2.0): writing past them corrupts a neighbouring layer. */
static int npx(int mode) { return mode == 26 ? W * 31 : mode == 27 ? W * 160 : mode == 28 ? W * 40 : 0; }

/* Idempotent, so buffers SampQOL keeps between frames are safe. Runs after chrome(): its scan wants the stock grey. */
static void navy(int mode, uint32_t *p)
{
    uint32_t li = 0xFFFFFFFFu, lo = 0;
    int n = npx(mode);
    for (int i = 0; i < n; ++i) {
        uint32_t c = p[i] & 0xFFFFFFu;
        if (c != li) { li = c; lo = lut(c); }
        if (lo != c) p[i] = (p[i] & 0xFF000000u) | lo;
    }
}

static void chrome(int mode, uint32_t *p)
{
    if (mode == 26) for (int i = 0; i < W * 2; ++i) p[i] = RED;    /* brand bar */
    for (int y = 3, rows = npx(mode) / W; y < rows - 2; ++y) {       /* cyan selection edge */
        uint32_t *row = &p[y * W];
        int cnt = 0, lx = -1;
        for (int x = 0; x < 140; ++x) if (selgray(row[x])) { ++cnt; if (lx < 0) lx = x; }
        if (cnt >= 40 && lx >= 0) { row[lx] = CYAN; row[lx + 1] = CYAN; row[lx + 2] = CYAN; }
    }
}

__attribute__((visibility("default")))
void overlay_update(int mode, const void *buf, int flag)
{
    if (matched < 0) {
        real_ov = (ov_fn)dlsym(RTLD_NEXT, "overlay_update");
        Dl_info info;
        matched = real_ov && dladdr((void *)real_ov, &info) &&
                  (uintptr_t)real_ov - (uintptr_t)info.dli_fbase == QOL_OVERLAY_UPDATE;
    }
    if (matched && buf) {
        if (mode == 26 || mode == 27) chrome(mode, (uint32_t *)buf);
        if (mode >= 26 && mode <= 28) navy(mode, (uint32_t *)buf);
    }
    if (real_ov) real_ov(mode, buf, flag);
}
