/* stems_pad.c: the pure parts of Stems on the Sample (pad) screen; host-checked in check_ui.c.
 *
 * Which pad: the label under the tabs, read off the glass. Stock shows "A06"; with SampQOL it reads
 * "S03 T01 A06". The glyphs (pad_glyphs.h) were cut from lab shots; a cell is matched by normalised
 * correlation over a quarter-pixel shift search, so the S and T digits read right although the
 * table only holds them as they sit in the pad slot.
 *
 * Which file: the app autosaves the open project ~10 s after a change to
 * /data/TempProjects/Temp-Project.xpj (gzip of a short header + JSON). Track T's drum program lists
 * each pad's layers; layer sampleFile names a WAV in Temp-Project_[ProjectData]. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pad_glyphs.h"
#include "stems_ui.h"

/* ---------- the pad label ---------- */
static float red(const uint32_t *px, int stride, float x, int y) {  /* linear in x */
    int i = (int)floorf(x); float f = x - i;
    float a = px[(size_t)y * stride + i] >> 16 & 255, b = px[(size_t)y * stride + i + 1] >> 16 & 255;
    return a * (1 - f) + b * f;
}
/* Best glyph for the cell at x; its correlation in *score. */
static char cell(const uint32_t *px, int stride, int x, int w, const char *chars, const unsigned char *g, float *score) {
    char best = '?'; float bs = -2;
    for (int sh = -6; sh <= 6; ++sh) {
        float v[GLYPH_H * 8], m = 0, n = 0;
        int k = 0;
        for (int y = 0; y < GLYPH_H; ++y)
            for (int c = 0; c < w; ++c) m += v[k++] = red(px, stride, x + c + sh / 4.f, GLYPH_Y + y);
        m /= k;
        for (int i = 0; i < k; ++i) { v[i] -= m; n += v[i] * v[i]; }
        if (n < 1) continue;                                  /* blank */
        n = sqrtf(n);
        for (const char *c = chars; *c; ++c) {
            const unsigned char *t = g + (c - chars) * GLYPH_H * w;
            float tm = 0, tn = 0, dot = 0;
            for (int i = 0; i < k; ++i) tm += t[i];
            tm /= k;
            for (int i = 0; i < k; ++i) { float d = t[i] - tm; tn += d * d; dot += d * v[i]; }
            float s = tn > 0 ? dot / (sqrtf(tn) * n) : -1;
            if (s > bs) { bs = s; best = *c; }
        }
    }
    *score = bs;
    return best;
}
static int lit(uint32_t c) { int r = c >> 16 & 255, g = c >> 8 & 255, b = c & 255; return r > 50 && r > g + 15 && b < 40; }
static int digits(const char *s) { return s[0] >= '0' && s[0] <= '9' && s[1] >= '0' && s[1] <= '9'; }

int stems_pad_label(const uint32_t *px, int stride, int *track, int *pad) {
    int box = 0;                                              /* the waveform box's bottom edge */
    for (int x = 20; x < 300; ++x) box += lit(px[(size_t)188 * stride + x]);
    if (box < 250) return -1;
    static const int MX[9] = {12, 19, 26, 39, 46, 53, 65, 72, 79}, SX[3] = {12, 20, 28};
    char s[10] = {0}; float sc, worst = 1;
    for (int i = 0; i < 9; ++i) { s[i] = cell(px, stride, MX[i], 7, MOD_CH, &MOD_G[0][0][0], &sc); if (sc < worst) worst = sc; }
    const char *p = NULL;
    if (worst > 0.85f && s[0] == 'S' && digits(s + 1) && s[3] == 'T' && digits(s + 4)) {
        *track = (s[4] - '0') * 10 + s[5] - '0';
        p = s + 6;
    } else {
        worst = 1;
        for (int i = 0; i < 3; ++i) { s[i] = cell(px, stride, SX[i], 8, STOCK_CH, &STOCK_G[0][0][0], &sc); if (sc < worst) worst = sc; }
        if (worst <= 0.85f) return -1;
        *track = 0;                                           /* not shown: the project's current track */
        p = s;
    }
    int n = (p[1] - '0') * 10 + p[2] - '0';
    if (p[0] < 'A' || p[0] > 'H' || !digits(p + 1) || n < 1 || n > 16) return -1;
    *pad = (p[0] - 'A') * 16 + n - 1;
    return 0;
}

/* ---------- a little JSON walking (just enough for the autosave) ---------- */
static const char *ws(const char *p) { while (p && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) ++p; return p; }
static const char *skip_str(const char *p) {                  /* p at '"' */
    for (++p; *p && *p != '"'; ++p) if (*p == '\\' && !*++p) return NULL;
    return *p ? p + 1 : NULL;
}
static const char *skip(const char *p) {                      /* one value */
    p = ws(p);
    if (!p || !*p) return NULL;
    if (*p == '"') return skip_str(p);
    if (*p != '{' && *p != '[') { while (*p && !strchr(",}] \n\r\t", *p)) ++p; return p; }
    int depth = 0;
    for (; *p; ++p) {
        if (*p == '"') { if (!(p = skip_str(p))) return NULL; --p; }
        else if (*p == '{' || *p == '[') ++depth;
        else if ((*p == '}' || *p == ']') && !--depth) return p + 1;
    }
    return NULL;
}
static const char *get(const char *p, const char *key) {      /* member of the object at p */
    p = ws(p);
    if (!p || *p != '{') return NULL;
    size_t kn = strlen(key);
    for (p = ws(p + 1); p && *p == '"'; ) {
        const char *k = p + 1, *e = skip_str(p);
        if (!e) return NULL;
        p = ws(e);
        if (!p || *p != ':') return NULL;
        if ((size_t)(e - 1 - k) == kn && !memcmp(k, key, kn)) return ws(p + 1);
        p = ws(skip(p + 1));
        if (p && *p == ',') p = ws(p + 1);
    }
    return NULL;
}
static const char *at_index(const char *p, int i) {           /* element i of the array at p */
    p = ws(p);
    if (!p || *p != '[') return NULL;
    for (p = ws(p + 1); p && *p && *p != ']'; ) {
        if (!i--) return p;
        p = ws(skip(p));
        if (p && *p == ',') p = ws(p + 1);
    }
    return NULL;
}
static int num(const char *p, long *v) { char *e; p = ws(p); if (!p) return -1; *v = strtol(p, &e, 10); return e == p ? -1 : 0; }
static int put_utf8(char **o, char *end, unsigned c) {
    unsigned char b[4]; int n = c < 0x80 ? 1 : c < 0x800 ? 2 : c < 0x10000 ? 3 : 4;
    if (end - *o < n) return -1;
    if (n == 1) b[0] = c;
    else { for (int i = n - 1; i > 0; --i, c >>= 6) b[i] = 0x80 | (c & 63); b[0] = ((0xf00 >> n) & 0xff) | c; }
    memcpy(*o, b, n); *o += n;
    return 0;
}
static int hex4(const char *p, unsigned *v) {
    *v = 0;
    for (int i = 0; i < 4; ++i) {
        char c = p[i]; int d = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
        if (d < 0) return -1;
        *v = *v << 4 | d;
    }
    return 0;
}
static int str(const char *p, char *out, size_t n) {          /* decoded string at p; -1 if it doesn't fit */
    p = ws(p);
    if (!p || *p != '"' || !n) return -1;
    char *o = out, *end = out + n - 1;
    for (++p; *p && *p != '"'; ++p) {
        if (*p != '\\') { if (o == end) return -1; *o++ = *p; continue; }
        unsigned c;
        switch (*++p) {
        case 'n': c = '\n'; break; case 't': c = '\t'; break; case 'r': c = '\r'; break;
        case 'b': c = '\b'; break; case 'f': c = '\f'; break;
        case 'u':
            if (hex4(p + 1, &c)) return -1;
            p += 4;
            if (c >= 0xd800 && c < 0xdc00 && p[1] == '\\' && p[2] == 'u') {
                unsigned lo;
                if (hex4(p + 3, &lo) || lo < 0xdc00 || lo > 0xdfff) return -1;
                c = 0x10000 + ((c - 0xd800) << 10) + (lo - 0xdc00); p += 6;
            }
            break;
        default: c = (unsigned char)*p; if (!c) return -1;
        }
        if (put_utf8(&o, end, c)) return -1;
    }
    *o = 0;
    return *p == '"' ? 0 : -1;
}

/* Track T's drum pads (T as in stems_xpj_sample), or NULL and why. */
static const char *drum_pads(const char *json, int track, const char **why) {
    const char *d = get(strchr(json, '{'), "data"), *tracks = d ? get(d, "tracks") : NULL, *t = NULL;
    if (!tracks) { *why = "Can't read the project"; return NULL; }
    long v;
    if (track > 0) {                                          /* the T-th track that isn't a submix (8) or an output (9) */
        for (int i = 0, seen = 0; (t = at_index(tracks, i)); ++i) {
            const char *ty = get(get(t, "program"), "type");
            if (!ty || num(ty, &v) || v == 8 || v == 9) continue;
            if (++seen == track) break;
        }
    } else if (!num(get(d, "currentTrackIndex"), &v) && v >= 0)
        t = at_index(tracks, (int)v);
    if (!t) { *why = "Track not found"; return NULL; }
    const char *pads = get(get(get(t, "program"), "drum"), "instruments");
    if (!pads) *why = "Not a drum program";
    return pads;
}

const char *stems_xpj_sample(const char *json, int track, int pad, char *file, size_t n) {
    const char *why = NULL, *pads = drum_pads(json, track, &why), *t;
    if (!pads) return why;
    const char *layers = get(at_index(pads, pad), "layersv");
    if (!layers) return "Not a drum program";
    for (int i = 0; (t = at_index(layers, i)); ++i) {
        const char *f = get(t, "sampleFile");
        if (f && !str(f, file, n) && *file && !strchr(file, '/') && strcmp(file, "..")) return NULL;
    }
    return "No sample on this pad";
}

/* Pads with no sample (every layer's sampleFile empty), in the bank of pad `from`: from it to the
   bank's end, then from the bank's start. Pad `skip` never counts. Up to max into out; the count,
   or -1 with *why. */
int stems_free_pads(const char *json, int track, int from, int skip, int *out, int max, const char **why) {
    const char *pads = drum_pads(json, track, why), *l, *f;
    if (!pads) return -1;
    if (from < 0 || from > 127) { *why = "No pad"; return -1; }
    int n = 0, bank = from / 16 * 16;
    for (int k = 0; k < 16 && n < max; ++k) {
        int pad = bank + (from - bank + k) % 16;
        const char *layers = pad == skip ? NULL : get(at_index(pads, pad), "layersv");
        if (!layers) continue;                                /* no such pad: not ours to fill */
        int used = 0;
        for (int i = 0; !used && (l = at_index(layers, i)); ++i) used = (f = get(l, "sampleFile")) && *ws(f) == '"' && ws(f)[1] != '"';
        if (!used) out[n++] = pad;
    }
    return n;
}

/* ---------- the chooser ---------- */
static const char *NAME[4] = {"Vocals", "Drums", "Bass", "Other"};
int stems_pick(int ticks, int summed, char *pick, size_t n) {  /* "VDBO" letters, + to sum */
    if (n < 6 || !(ticks & 15)) return -1;
    char *o = pick;
    for (int i = 0; i < 4; ++i) if (ticks >> i & 1) *o++ = "VDBO"[i];
    if (summed) *o++ = '+';
    *o = 0;
    return 0;
}
/* The files hs-stems writes for this pick (same names as hs_stems.c), one per call; i from 0. */
int stems_out_name(const char *base, int ticks, int summed, int i, char *out, size_t n) {
    char mix[40] = "";
    if (summed) {
        if (i) return -1;
        for (int s = 0; s < 4; ++s)
            if (ticks >> s & 1) snprintf(mix + strlen(mix), sizeof mix - strlen(mix), "%s%s", *mix ? "+" : "", NAME[s]);
    } else {
        int s = 0;
        for (int k = -1; s < 4; ++s) if (ticks >> s & 1 && ++k == i) break;
        if (s == 4) return -1;
        snprintf(mix, sizeof mix, "%s", NAME[s]);
    }
    int r = snprintf(out, n, "%s-%s.wav", base, mix);
    return r < 0 || (size_t)r >= n ? -2 : 0;
}

/* The result box's line for where the stems went: "On pads A02-A05", "On pads A02 A05", "On pad A04",
   "On A15 A16, bank full", "Bank full: not on pads". */
int stems_pads_note(const int *pads, int placed, int wanted, char *out, size_t n) {
    if (!placed) return snprintf(out, n, "Bank full: not on pads") < 0 ? -1 : 0;
    int run = 1;
    for (int i = 1; i < placed; ++i) run &= pads[i] == pads[i - 1] + 1;
    char list[48] = "";
    for (int i = 0; i < placed; ++i) {
        if (run && placed > 2 && i > 0 && i < placed - 1) continue;      /* A02-A05 */
        snprintf(list + strlen(list), sizeof list - strlen(list), "%s%c%02d",
                 !i ? "" : run && placed > 2 ? "-" : " ", 'A' + pads[i] / 16, pads[i] % 16 + 1);
    }
    int r = placed == wanted ? snprintf(out, n, "On pad%s %s", placed > 1 ? "s" : "", list) : snprintf(out, n, "On %s, bank full", list);
    return r < 0 || (size_t)r >= n ? -1 : 0;
}
