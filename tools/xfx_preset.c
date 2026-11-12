/* xfx_preset: write a one-effect chain (.xfx) that opens an HS effect at set values.
 *   xfx_preset LABEL "Port name prefix=value" ... > NAME.xfx
 *   xfx_preset --decode LABEL DATA          (prints each control's value, to check a file)
 * Values are in the knob's own units; ports not given keep the effect's default. The MPC stores
 * each control input as a float scaled 0..1 across its LADSPA range: linear, but along the log
 * curve for LADSPA_HINT_LOGARITHMIC knobs (emulator, 12 Nov 2026: 0.5 opened Release 100..5000 at
 * 706 ms, High Mid 200..20k at 2.06 kHz). Packed as JUCE MemoryBlock base64 ("size.chars",
 * alphabet ".A-Za-z0-9+", LSB-first bits). The MPC rounds each knob to about 1 %.
 * Build: cc -I src/fx tools/xfx_preset.c src/fx/hs_*.c (see tools/make-presets.sh). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ladspa.h"

const LADSPA_Descriptor *ladspa_descriptor(unsigned long index);
static const char B64[] = ".ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+";

static float def_value(const LADSPA_PortRangeHint *r) {
    int f = r->HintDescriptor, lg = LADSPA_IS_HINT_LOGARITHMIC(f);
    float lo = r->LowerBound, hi = r->UpperBound;
    #define MIX(a) (lg && lo > 0 ? expf(logf(lo) * (1 - (a)) + logf(hi) * (a)) : lo * (1 - (a)) + hi * (a))
    switch (f & LADSPA_HINT_DEFAULT_MASK) {
    case LADSPA_HINT_DEFAULT_MINIMUM: return lo;
    case LADSPA_HINT_DEFAULT_LOW: return MIX(.25f);
    case LADSPA_HINT_DEFAULT_MIDDLE: return MIX(.5f);
    case LADSPA_HINT_DEFAULT_HIGH: return MIX(.75f);
    case LADSPA_HINT_DEFAULT_MAXIMUM: return hi;
    case LADSPA_HINT_DEFAULT_0: return 0;
    case LADSPA_HINT_DEFAULT_1: return 1;
    case LADSPA_HINT_DEFAULT_100: return 100;
    case LADSPA_HINT_DEFAULT_440: return 440;
    default: return lo;
    }
    #undef MIX
}

static int is_log(const LADSPA_PortRangeHint *r) {
    return LADSPA_IS_HINT_LOGARITHMIC(r->HintDescriptor) && r->LowerBound > 0;
}
static float to_norm(const LADSPA_PortRangeHint *r, float v) {
    float lo = r->LowerBound, hi = r->UpperBound;
    return is_log(r) ? logf(v / lo) / logf(hi / lo) : (v - lo) / (hi - lo);
}
static float from_norm(const LADSPA_PortRangeHint *r, float x) {
    float lo = r->LowerBound, hi = r->UpperBound;
    return is_log(r) ? lo * powf(hi / lo, x) : lo + x * (hi - lo);
}

static const LADSPA_Descriptor *find(const char *label) {
    const LADSPA_Descriptor *d;
    for (unsigned long i = 0; (d = ladspa_descriptor(i)); ++i) if (!strcmp(d->Label, label)) return d;
    fprintf(stderr, "no effect with label %s\n", label);
    exit(2);
}
static int is_ctl_in(const LADSPA_Descriptor *d, unsigned long p) {
    return LADSPA_IS_PORT_CONTROL(d->PortDescriptors[p]) && LADSPA_IS_PORT_INPUT(d->PortDescriptors[p]);
}

static void encode(const unsigned char *b, size_t n, char *out) {
    out += sprintf(out, "%zu.", n);
    size_t chars = (n * 8 + 5) / 6;
    for (size_t i = 0; i < chars; ++i) {
        unsigned v = 0;
        for (int k = 0; k < 6; ++k) {
            size_t bit = i * 6 + k;
            if (bit < n * 8 && (b[bit / 8] >> (bit % 8) & 1)) v |= 1u << k;
        }
        *out++ = B64[v];
    }
    *out = 0;
}
static size_t decode(const char *s, unsigned char *b, size_t cap) {
    size_t n = strtoul(s, NULL, 10);
    const char *c = strchr(s, '.');
    if (!c || n > cap) return 0;
    memset(b, 0, n);
    for (size_t i = 0; c[1 + i]; ++i) {
        const char *p = strchr(B64, c[1 + i]);
        if (!p) return 0;
        for (int k = 0; k < 6; ++k) {
            size_t bit = i * 6 + k;
            if (bit < n * 8 && ((p - B64) >> k & 1)) b[bit / 8] |= (unsigned char)(1u << (bit % 8));
        }
    }
    return n;
}

int main(int argc, char **argv) {
    if (argc >= 4 && !strcmp(argv[1], "--decode")) {
        const LADSPA_Descriptor *d = find(argv[2]);
        unsigned char b[4096]; size_t n = decode(argv[3], b, sizeof b), k = 0;
        for (unsigned long p = 0; p < d->PortCount; ++p) if (is_ctl_in(d, p)) {
            if ((k + 1) * 4 > n) break;
            float x; memcpy(&x, b + 4 * k++, 4);
            const LADSPA_PortRangeHint *r = &d->PortRangeHints[p];
            printf("%-30s %8.3f  (%.4f)\n", d->PortNames[p], from_norm(r, x), x);
        }
        return 0;
    }
    if (argc < 2) { fprintf(stderr, "usage: xfx_preset LABEL \"Port=value\"... | --decode LABEL DATA\n"); return 2; }
    const LADSPA_Descriptor *d = find(argv[1]);
    float norm[512]; size_t k = 0;
    for (unsigned long p = 0; p < d->PortCount && k < 512; ++p) if (is_ctl_in(d, p)) {
        const LADSPA_PortRangeHint *r = &d->PortRangeHints[p];
        float v = def_value(r);
        size_t plen = strlen(d->PortNames[p]);
        for (int a = 2; a < argc; ++a) {
            const char *eq = strrchr(argv[a], '=');
            size_t n = eq ? (size_t)(eq - argv[a]) : 0;
            if (n && n <= plen && !strncmp(argv[a], d->PortNames[p], n)) {
                v = strtof(eq + 1, NULL);
                if (v < r->LowerBound || v > r->UpperBound) {
                    fprintf(stderr, "%s=%g is outside %g..%g\n", d->PortNames[p], v, r->LowerBound, r->UpperBound);
                    return 2;
                }
            }
        }
        norm[k++] = to_norm(r, v);
    }
    for (int a = 2; a < argc; ++a) {                      /* every argument must name a port */
        const char *eq = strrchr(argv[a], '=');
        size_t n = eq ? (size_t)(eq - argv[a]) : 0; int hit = 0;
        for (unsigned long p = 0; p < d->PortCount && n; ++p)
            if (is_ctl_in(d, p) && n <= strlen(d->PortNames[p]) && !strncmp(argv[a], d->PortNames[p], n)) ++hit;
        if (hit != 1) { fprintf(stderr, "\"%s\" matches %d ports, needs exactly 1\n", argv[a], hit); return 2; }
    }
    unsigned char bytes[2048]; char data[4096];
    memcpy(bytes, norm, k * 4);
    encode(bytes, k * 4, data);
    printf("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n\n<MPCVObject>\n  <Version>\n    <File_Version>2.1</File_Version>\n"
           "    <Application>MPC-V</Application>\n    <Application_Version>0.0.0.495</Application_Version>\n"
           "    <Platform>Linux</Platform>\n  </Version>\n  <InsertEffectStrip>\n    <Insert1>\n      <Is64Bits>1</Is64Bits>\n"
           "      <Name>%s</Name>\n      <DescriptiveName>%s</DescriptiveName>\n      <FormatName>LADSPA</FormatName>\n"
           "      <Category>Effect</Category>\n      <ManufacturerName>Henry Skillz</ManufacturerName>\n"
           "      <FileOrID>/tmp/hs-ladspa-test/hs-pro-fx-ladspa.so</FileOrID>\n      <UID>%lu</UID>\n"
           "      <IsInstrument>0</IsInstrument>\n      <NumInput>2</NumInput>\n      <NumOutput>2</NumOutput>\n"
           "      <ProgramNumber>0</ProgramNumber>\n      <Data>%s</Data>\n    </Insert1>\n"
           "    <InsertEnable1>True</InsertEnable1>\n  </InsertEffectStrip>\n</MPCVObject>\n",
           d->Label, d->Name, d->UniqueID, data);
    return 0;
}
