/* Factory presets, one source for both builds: tools/xfx_preset.c writes each as an MPC chain
 * (src/chains/FILE.xfx, loaded by naming a track FILE), and the CLAP build lists them in the host's
 * preset browser. Values are in each knob's own units; "Port name prefix=value", the prefix must match
 * exactly one control input. Knobs not listed keep the effect's default. */
#ifndef HS_FACTORY_PRESETS_H
#define HS_FACTORY_PRESETS_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ladspa.h"

typedef struct { const char *file, *name, *label, *args[24]; } hs_factory_preset;

static const hs_factory_preset hs_factory_presets[] = {
    {"KICK", "Kick", "hs_tube_strip",
     {"Drive=3.5", "HPF=20", "Output=1.5", "Threshold=-8", "Ratio=4", "Attack=15", "Release=110",
      "EQ Before=0", "SC EQ=0", "High Q=0", "Bass Hz=2", "Bass (dB)=2", "Low Mid (dB)=-3", "Low Mid (Hz)=350",
      "High Mid (Hz)=4000", "High Mid (dB)=2", "Treble Hz=1", "Treble (dB)=0", NULL}},
    {"BASSLINE", "Bassline", "hs_tube_strip",
     {"Drive=3", "HPF=30", "Output=1", "Threshold=-10", "Ratio=3", "Attack=30", "Release=300",
      "EQ Before=0", "SC EQ=0", "High Q=0", "Bass Hz=2", "Bass (dB)=3", "Low Mid (dB)=-2", "Low Mid (Hz)=250",
      "High Mid (Hz)=900", "High Mid (dB)=2", "Treble Hz=1", "Treble (dB)=-2", NULL}},
};
enum { HS_FACTORY_PRESET_COUNT = sizeof hs_factory_presets / sizeof *hs_factory_presets };

static inline int hs_is_ctl_in(const LADSPA_Descriptor *d, unsigned long p) {
    return LADSPA_IS_PORT_CONTROL(d->PortDescriptors[p]) && LADSPA_IS_PORT_INPUT(d->PortDescriptors[p]);
}

/* Write each argument's value into vals[port]. Returns 0, or -1 with the reason in err. */
static inline int hs_preset_apply(const LADSPA_Descriptor *d, const char *const *args, float *vals,
                                  char *err, size_t errlen) {
    for (; *args; ++args) {
        const char *eq = strrchr(*args, '=');
        size_t n = eq ? (size_t)(eq - *args) : 0;
        long hit = -1; int hits = 0;
        for (unsigned long p = 0; p < d->PortCount && n; ++p)
            if (hs_is_ctl_in(d, p) && n <= strlen(d->PortNames[p]) && !strncmp(*args, d->PortNames[p], n)) {
                hit = (long)p; ++hits;
            }
        if (hits != 1) { snprintf(err, errlen, "\"%s\" matches %d ports, needs exactly 1", *args, hits); return -1; }
        const LADSPA_PortRangeHint *r = &d->PortRangeHints[hit];
        char *end; float v = strtof(eq + 1, &end);
        if (end == eq + 1 || *end || v < r->LowerBound || v > r->UpperBound) {
            snprintf(err, errlen, "%s is not a number in %g..%g", *args, r->LowerBound, r->UpperBound);
            return -1;
        }
        vals[hit] = v;
    }
    return 0;
}
#endif
