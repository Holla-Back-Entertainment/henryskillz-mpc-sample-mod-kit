/* Factory presets, one source for all three places they appear:
 *  - the PRESETS page on the MPC (hold B2): src/fx/gen-factory.c writes them into src/fx/factory.inc,
 *    in this order per effect, so the first entry for an effect is its slot 1 (8 slots at most);
 *  - the CLAP build lists them in the host's preset browser;
 *  - entries with a file name are also written as an MPC chain by tools/xfx_preset.c
 *    (src/chains/FILE.xfx, loaded by naming a track FILE). file = NULL: no chain.
 * Values are in each knob's own units; "Port name prefix=value", the prefix must match exactly one
 * control input. Where two controls share a name (the Channel Strip's compressor and gate both have
 * "Release ms"), "Release ms#1=300" picks the first and "#2" the second. Knobs not listed keep the
 * effect's default. Names: 14 characters at most. */
#ifndef HS_FACTORY_PRESETS_H
#define HS_FACTORY_PRESETS_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ladspa.h"

typedef struct { const char *file, *name, *label, *args[32]; } hs_factory_preset;

static const hs_factory_preset hs_factory_presets[] = {
    {"KICK", "Kick", "hs_tube_strip",
     {"Drive=3.5", "HPF=20", "Output=1.5", "Threshold=-8", "Ratio=4", "Attack=15", "Release=110",
      "EQ Before=0", "SC EQ=0", "High Q=0", "Bass Hz=2", "Bass (dB)=2", "Low Mid (dB)=-3", "Low Mid (Hz)=350",
      "High Mid (Hz)=4000", "High Mid (dB)=2", "Treble Hz=1", "Treble (dB)=0", NULL}},
    {"BASSLINE", "Bassline", "hs_tube_strip",
     {"Drive=3", "HPF=30", "Output=1", "Threshold=-10", "Ratio=3", "Attack=30", "Release=300",
      "EQ Before=0", "SC EQ=0", "High Q=0", "Bass Hz=2", "Bass (dB)=3", "Low Mid (dB)=-2", "Low Mid (Hz)=250",
      "High Mid (Hz)=900", "High Mid (dB)=2", "Treble Hz=1", "Treble (dB)=-2", NULL}},
    {NULL, "VOCAL", "hs_channel_strip",
     {"HP (Hz)=90", "LM Hz=300", "LM dB=-2", "HM Hz=3500", "HM dB=2.5", "HF Hz=10000", "HF dB=3", "Comp Thresh=-8", "Comp Ratio=3", "Make-Up dB=3", "Release ms#1=300", "DS Freq Hz=6500", "DS Thresh dB=-28", "DS Range dB=6", NULL}},
    {NULL, "DRUM BUS", "hs_channel_strip",
     {"Drive (%)=20", "HP (Hz)=30", "LF Hz=70", "LF dB=3", "HM Hz=4000", "HM dB=2", "HF Hz=9000", "HF dB=2", "Comp Thresh=-6", "Comp Ratio=4", "Make-Up dB=3", "Release ms#1=150", NULL}},
    {NULL, "BASS", "hs_channel_strip",
     {"Drive (%)=15", "HP (Hz)=35", "LP (Hz)=6000", "LF Hz=80", "LF dB=3", "LM Hz=250", "LM dB=-3", "HM Hz=900", "HM dB=2", "Comp Thresh=-10", "Comp Ratio=4", "Make-Up dB=4", "Release ms#1=250", NULL}},
    {NULL, "WARM TAPE", "hs_drive_sat",
     {"Drive (dB)=6", "Shape=-20", "Depth (%)=60", "Harmonics (%)=40", "Output Gain (dB)=-3", NULL}},
    {NULL, "CRUNCH", "hs_drive_sat",
     {"Drive (dB)=14", "Shape=20", "Depth (%)=70", "Harmonics (%)=70", "Output Gain (dB)=-8", NULL}},
    {NULL, "PARALLEL", "hs_drive_sat",
     {"Drive (dB)=18", "Depth (%)=80", "Harmonics (%)=60", "Dry/Wet Mix (%)=35", "Output Gain (dB)=-6", NULL}},
    {NULL, "GLUE 2:1", "hs_bus_comp",
     {"Threshold (dB)=0", "Ratio step=1", "Attack step=2", "Release step=0", "Makeup (dB)=2", "Sidechain HPF (Hz)=60", NULL}},
    {NULL, "DRUM SMASH", "hs_bus_comp",
     {"Threshold (dB)=-8", "Ratio step=3", "Attack step=-1", "Release step=-5", "Makeup (dB)=5", "Sidechain HPF (Hz)=90", NULL}},
    {NULL, "PARALLEL", "hs_bus_comp",
     {"Threshold (dB)=-14", "Ratio step=4", "Attack step=-3", "Release step=-4", "Makeup (dB)=8", "Dry/Wet Mix (%)=40", NULL}},
    {NULL, "DUCK KEY A", "hs_pump",
     {"Input (dB)=14", "Ratio=5", "Attack=1", "Release=4", "Key In=1", NULL}},
    {NULL, "DRUM PUNCH", "hs_pump",
     {"Input (dB)=10", "Ratio=3", "Attack=5", "Release=1", "Sat=1", "Output (dB)=-4", NULL}},
    {NULL, "VOCAL LEVEL", "hs_pump",
     {"Input (dB)=8", "Ratio=3", "Attack=3", "Release=7", "Output (dB)=-3", NULL}},
    {NULL, "KEY SEND A", "hs_pump",
     {"Input (dB)=0", "Ratio=0", "Attack=3", "Release=3", "Key Send=1", NULL}},
    {NULL, "VOCAL OPTO", "hs_crush",
     {"Input=5.5", "Attack=6", "Release=5", "Ratio=5", "Output=6.5", NULL}},
    {NULL, "DRUM ROOM", "hs_crush",
     {"Input=7", "Attack=8", "Release=2", "Ratio=6", "Detector=1", "Audio=2", "Output=5", NULL}},
    {NULL, "PARALLEL MAX", "hs_crush",
     {"Input=8", "Attack=2", "Release=3", "Ratio=7", "Audio=3", "British Mode=1", "Output=4.5", "Mix (%)=35", NULL}},
    {NULL, "BASS 4:1", "hs_crush",
     {"Input=5", "Attack=7", "Release=4", "Ratio=3", "Detector=1", "Output=7", NULL}},
    {NULL, "WARM TAPE", "hs_heat",
     {"Circuit=1", "Drive=45", "EQ low=6", "EQ high=-8", NULL}},
    {NULL, "LOFI FILTER", "hs_heat",
     {"Circuit=4", "Drive=55", "Filter type=1", "Filter freq=78", "Resonance=30", "Dirt=40", "Volume (dB)=-3", NULL}},
    {NULL, "FUZZ BASS", "hs_heat",
     {"Circuit=6", "Drive=60", "EQ low=10", "Mix (%)=60", "Volume (dB)=-4", NULL}},
    {NULL, "AUTO WAH", "hs_heat",
     {"Circuit=2", "Drive=35", "Filter type=3", "Filter freq=60", "Resonance=70", "Env > freq=60", "Env attack=10", "Env release=50", NULL}},
    {NULL, "VOCAL", "hs_tube_strip",
     {"Drive=3", "HPF=80", "Threshold (dB)=-6", "Ratio=3", "Attack (ms)=10", "Release (ms)=400", "SC EQ=1", "Low Mid (Hz)=300", "Low Mid (dB)=-2", "High Mid (Hz)=3500", "High Mid (dB)=2", "Treble Hz=2", "Treble (dB)=3", NULL}},
    {NULL, "BASS DI", "hs_tube_strip",
     {"Drive=4.5", "Threshold (dB)=-10", "Ratio=4", "Attack (ms)=30", "Release (ms)=300", "Bass Hz=2", "Bass (dB)=4", "High Mid (Hz)=900", "High Mid (dB)=2", NULL}},
    {NULL, "MIX SHEEN", "hs_tube_strip",
     {"Drive=2", "Threshold (dB)=4", "Ratio=2", "Attack (ms)=30", "Release (ms)=600", "Bass Hz=1", "Bass (dB)=2", "Treble Hz=3", "Treble (dB)=4", NULL}},
    {NULL, "VOCAL CLEAN", "hs_para_eq",
     {"Band 1 Frequency=90", "Band 1 Gain=0", "Band 1 Q=1", "Band 1 Type=0", "Band 1 On=1", "Band 2 Frequency=300", "Band 2 Gain=-3", "Band 2 Q=1.2", "Band 2 Type=4", "Band 2 On=1", "Band 3 Frequency=3500", "Band 3 Gain=2.5", "Band 3 Q=1", "Band 3 Type=4", "Band 3 On=1", "Band 4 Frequency=10000", "Band 4 Gain=3", "Band 4 Q=1", "Band 4 Type=3", "Band 4 On=1", NULL}},
    {NULL, "KICK", "hs_para_eq",
     {"Band 1 Frequency=30", "Band 1 Gain=0", "Band 1 Q=1", "Band 1 Type=0", "Band 1 On=1", "Band 2 Frequency=60", "Band 2 Gain=4", "Band 2 Q=1.2", "Band 2 Type=4", "Band 2 On=1", "Band 3 Frequency=350", "Band 3 Gain=-4", "Band 3 Q=1.5", "Band 3 Type=4", "Band 3 On=1", "Band 4 Frequency=4000", "Band 4 Gain=3", "Band 4 Q=1.2", "Band 4 Type=4", "Band 4 On=1", NULL}},
    {NULL, "BASS TIGHT", "hs_para_eq",
     {"Band 1 Frequency=35", "Band 1 Gain=0", "Band 1 Q=1", "Band 1 Type=0", "Band 1 On=1", "Band 2 Frequency=90", "Band 2 Gain=3", "Band 2 Q=1", "Band 2 Type=4", "Band 2 On=1", "Band 3 Frequency=250", "Band 3 Gain=-3", "Band 3 Q=1.4", "Band 3 Type=4", "Band 3 On=1", "Band 4 Frequency=1200", "Band 4 Gain=2", "Band 4 Q=1", "Band 4 Type=4", "Band 4 On=1", "Band 5 Frequency=8000", "Band 5 Gain=0", "Band 5 Q=1", "Band 5 Type=1", "Band 5 On=1", NULL}},
    {NULL, "AIR MASTER", "hs_para_eq",
     {"Band 1 Frequency=25", "Band 1 Gain=0", "Band 1 Q=1", "Band 1 Type=0", "Band 1 On=1", "Band 2 Frequency=80", "Band 2 Gain=1.5", "Band 2 Q=1", "Band 2 Type=2", "Band 2 On=1", "Band 3 Frequency=300", "Band 3 Gain=-1", "Band 3 Q=0.8", "Band 3 Type=4", "Band 3 On=1", "Band 4 Frequency=12000", "Band 4 Gain=2.5", "Band 4 Q=1", "Band 4 Type=3", "Band 4 On=1", NULL}},
    {NULL, "PHONE", "hs_para_eq",
     {"Band 1 Frequency=400", "Band 1 Gain=0", "Band 1 Q=1", "Band 1 Type=0", "Band 1 On=1", "Band 2 Frequency=3200", "Band 2 Gain=0", "Band 2 Q=1", "Band 2 Type=1", "Band 2 On=1", "Band 3 Frequency=1500", "Band 3 Gain=4", "Band 3 Q=1", "Band 3 Type=4", "Band 3 On=1", NULL}},
};
enum { HS_FACTORY_PRESET_COUNT = sizeof hs_factory_presets / sizeof *hs_factory_presets };

static inline int hs_is_ctl_in(const LADSPA_Descriptor *d, unsigned long p) {
    return LADSPA_IS_PORT_CONTROL(d->PortDescriptors[p]) && LADSPA_IS_PORT_INPUT(d->PortDescriptors[p]);
}

/* A stepped control's default as the effect reads it: the LADSPA hint can land between steps (Heat's
 * Circuit is 1.75 on 0..7, Pump's Attack 3.5), so hosts and presets get the whole step instead.
 * Heat reads its 1.75 as Saturation (1), see hs_heat.c; every other effect rounds. */
static inline float hs_snap_default(const LADSPA_Descriptor *d, unsigned long p, float v) {
    LADSPA_PortRangeHintDescriptor h = d->PortRangeHints[p].HintDescriptor;
    if (!LADSPA_IS_HINT_INTEGER(h) && !LADSPA_IS_HINT_TOGGLED(h)) return v;
    if (!strcmp(d->Label, "hs_heat") && !strcmp(d->PortNames[p], "Circuit")) return 1;
    return (float)(long)(v + (v < 0 ? -.5f : .5f));
}

/* Write each argument's value into vals[port]. Returns 0, or -1 with the reason in err. */
static inline int hs_preset_apply(const LADSPA_Descriptor *d, const char *const *args, float *vals,
                                  char *err, size_t errlen) {
    for (; *args; ++args) {
        const char *eq = strrchr(*args, '=');
        size_t n = eq ? (size_t)(eq - *args) : 0;
        int want = 0;                                    /* "#2": the second control with that name */
        if (n > 2 && (*args)[n - 2] == '#') { want = (*args)[n - 1] - '0'; n -= 2; }
        long hit = -1; int hits = 0;
        for (unsigned long p = 0; p < d->PortCount && n; ++p)
            if (hs_is_ctl_in(d, p) && n <= strlen(d->PortNames[p]) && !strncmp(*args, d->PortNames[p], n))
                if (++hits == want || !want) hit = (long)p;
        if (want ? hits < want || want < 1 : hits != 1) { snprintf(err, errlen, "\"%s\" matches %d ports, needs exactly 1", *args, hits); return -1; }
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
