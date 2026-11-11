#include <ladspa.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "hs_air_exciter.h"
#include "hs_channel.h"
#include "hs_xsat.h"
#include "hs_bus.h"
#include "hs_eq2.h"
#include "hs_pump.h"
#include "hs_heat.h"
#include "hs_distressor.h"
#include "hs_bass_focus.h"
#include "hs_tube_strip.h"

extern const LADSPA_Descriptor hs_channel_descriptor, hs_xsat_descriptor, hs_bus_descriptor, hs_eq2_descriptor, hs_pump_descriptor, hs_heat_descriptor, hs_distressor_descriptor, hs_bass_focus_descriptor, hs_tube_strip_descriptor;
#include "hs_saturator.h"

enum { INPUT_L, INPUT_R, OUTPUT_L, OUTPUT_R, CONTROL_1, CONTROL_2, CONTROL_3, PORT_COUNT };
enum { SATURATOR_ID = 0x485301, EXCITER_ID = 0x485302, BLOCK_SIZE = 128 };

struct effect_instance {
    unsigned long sample_rate;
    int exciter;
    float *ports[PORT_COUNT];
    float controls[3];
    union {
        struct hs_saturator saturator;
        struct hs_air_exciter exciter;
    } state;
};

static LADSPA_Handle instantiate(const LADSPA_Descriptor *descriptor, unsigned long sample_rate)
{
    struct effect_instance *instance = calloc(1, sizeof *instance);
    if (instance) {
        instance->sample_rate = sample_rate;
        instance->exciter = descriptor->UniqueID == EXCITER_ID;
    }
    return instance;
}

static void connect_port(LADSPA_Handle handle, unsigned long port, LADSPA_Data *data)
{
    struct effect_instance *instance = handle;
    if (port < PORT_COUNT) instance->ports[port] = data;
}

static void activate(LADSPA_Handle handle)
{
    struct effect_instance *instance = handle;
    memset(&instance->state, 0, sizeof instance->state);
    for (int i = 0; i < 3; ++i) instance->controls[i] = NAN;
}

static void run(LADSPA_Handle handle, unsigned long frames)
{
    struct effect_instance *instance = handle;
    float **ports = instance->ports;
    const float defaults[] = {instance->exciter ? 4899.0f : 0.0f,
                              instance->exciter ? 9.0f : 0.5f,
                              instance->exciter ? 0.0f : 1.0f};
    float controls[3];
    int changed = 0;
    for (int i = 0; i < 3; ++i) {
        float *port = ports[CONTROL_1 + i];
        controls[i] = port && isfinite(*port) ? *port : defaults[i];
        if (controls[i] != instance->controls[i]) changed = 1;
    }
    if (changed) {
        if (instance->exciter)
            hs_air_exciter_configure(&instance->state.exciter, instance->sample_rate,
                                     controls[0], controls[1], controls[2]);
        else
            hs_saturator_configure(&instance->state.saturator, instance->sample_rate,
                                   controls[0], controls[1], controls[2]);
        memcpy(instance->controls, controls, sizeof controls);
    }

    for (unsigned long offset = 0; offset < frames;) {
        unsigned long count = frames - offset;
        if (count > BLOCK_SIZE) count = BLOCK_SIZE;
        float left[BLOCK_SIZE], right[BLOCK_SIZE];
        size_t bytes = count * sizeof *left;
        memcpy(left, ports[INPUT_L] + offset, bytes);
        memcpy(right, ports[INPUT_R] + offset, bytes);
        if (instance->exciter)
            hs_air_exciter_process(&instance->state.exciter, left, right, count);
        else
            hs_saturator_process(&instance->state.saturator, left, right, count);
        memcpy(ports[OUTPUT_L] + offset, left, bytes);
        memcpy(ports[OUTPUT_R] + offset, right, bytes);
        offset += count;
    }
}

static const LADSPA_PortDescriptor ports[] = {
    LADSPA_PORT_INPUT | LADSPA_PORT_AUDIO, LADSPA_PORT_INPUT | LADSPA_PORT_AUDIO,
    LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO, LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO,
    LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL, LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL,
    LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL
};

static const char *const saturator_names[] = {
    "Input L", "Input R", "Output L", "Output R", "Drive (dB)", "Tone", "Mix"
};
static const char *const exciter_names[] = {
    "Input L", "Input R", "Output L", "Output R", "Frequency (Hz)", "Drive (dB)", "Amount"
};

#define BOUNDS (LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE)
static const LADSPA_PortRangeHint saturator_hints[] = {
    {0}, {0}, {0}, {0},
    {BOUNDS | LADSPA_HINT_DEFAULT_0, 0, 24},
    {BOUNDS | LADSPA_HINT_DEFAULT_MIDDLE, 0, 1},
    {BOUNDS | LADSPA_HINT_DEFAULT_1, 0, 1}
};
static const LADSPA_PortRangeHint exciter_hints[] = {
    {0}, {0}, {0}, {0},
    {BOUNDS | LADSPA_HINT_LOGARITHMIC | LADSPA_HINT_DEFAULT_MIDDLE, 2000, 12000},
    {BOUNDS | LADSPA_HINT_DEFAULT_MIDDLE, 0, 18},
    {BOUNDS | LADSPA_HINT_DEFAULT_0, 0, 1}
};
#undef BOUNDS

static const LADSPA_Descriptor effects[] = {
    {
        .UniqueID = SATURATOR_ID, .Label = "hs_saturator", .Name = "HS Saturator",
        .Maker = "Henry Skillz", .Copyright = "Prototype",
        .PortCount = PORT_COUNT, .PortDescriptors = ports,
        .PortNames = saturator_names, .PortRangeHints = saturator_hints,
        .instantiate = instantiate, .connect_port = connect_port,
        .activate = activate, .run = run, .cleanup = free
    },
    {
        .UniqueID = EXCITER_ID, .Label = "hs_air_exciter", .Name = "HS Air Exciter",
        .Maker = "Henry Skillz", .Copyright = "Prototype",
        .PortCount = PORT_COUNT, .PortDescriptors = ports,
        .PortNames = exciter_names, .PortRangeHints = exciter_hints,
        .instantiate = instantiate, .connect_port = connect_port,
        .activate = activate, .run = run, .cleanup = free
    }
};

/* Every new effect also needs a row in hs_fx_picker.c hs_rows, in this order: the picker
   drops the whole HS Effects menu when the counts or names differ. */
const LADSPA_Descriptor *ladspa_descriptor(unsigned long index)
{
#ifdef HS_MPC_LADSPA_RECALL
    /* MPC 1.3.0 saves UniqueID and passes it to this ordinal lookup on reopen. */
    if (index == SATURATOR_ID) return &effects[0];
    if (index == EXCITER_ID) return &effects[1];
    if (index == HS_CHANNEL_ID) return &hs_channel_descriptor;
    if (index == HS_XSAT_ID) return &hs_xsat_descriptor;
    if (index == HS_BUS_ID) return &hs_bus_descriptor;
    if (index == HS_EQ2_ID) return &hs_eq2_descriptor;
    if (index == HS_PUMP_ID) return &hs_pump_descriptor;
    if (index == HS_HEAT_ID) return &hs_heat_descriptor;
    if (index == HS_DISTRESSOR_ID) return &hs_distressor_descriptor;
    if (index == HS_BASS_FOCUS_ID) return &hs_bass_focus_descriptor;
    if (index == HS_TUBE_STRIP_ID) return &hs_tube_strip_descriptor;
#endif
    if (index == sizeof effects / sizeof *effects) return &hs_channel_descriptor;
    if (index == sizeof effects / sizeof *effects + 1) return &hs_xsat_descriptor;
    if (index == sizeof effects / sizeof *effects + 2) return &hs_bus_descriptor;
    if (index == sizeof effects / sizeof *effects + 3) return &hs_eq2_descriptor;
    if (index == sizeof effects / sizeof *effects + 4) return &hs_pump_descriptor;
    if (index == sizeof effects / sizeof *effects + 5) return &hs_heat_descriptor;
    if (index == sizeof effects / sizeof *effects + 6) return &hs_distressor_descriptor;
    if (index == sizeof effects / sizeof *effects + 7) return &hs_bass_focus_descriptor;
    if (index == sizeof effects / sizeof *effects + 8) return &hs_tube_strip_descriptor;
    return index < sizeof effects / sizeof *effects ? &effects[index] : NULL;
}
