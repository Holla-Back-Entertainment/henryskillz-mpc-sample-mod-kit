#include <dlfcn.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <ladspa.h>

#include "hs_channel.h"
#include "hs_xsat.h"
#include "hs_bus.h"
#include "hs_eq2.h"
#include "hs_pump.h"
#include "hs_heat.h"
static LADSPA_Descriptor_Function load_descriptor;

enum { BLOCK = 128, RATE = 44100, SETTLE = 170, MEASURE = 100 };
struct fixture {
    const LADSPA_Descriptor *plugin;
    LADSPA_Handle handle;
    float controls[CH_COUNT];
    float in[2][BLOCK], out[2][BLOCK];
};

static void neutral(struct fixture *f)
{
    const float values[CH_COUNT] = {
        [CH_HP] = 20, [CH_LP] = 16000,
        [CH_LF_HZ] = 100, [CH_LM_HZ] = 700, [CH_LM_Q] = 1,
        [CH_HM_HZ] = 2500, [CH_HM_Q] = 1, [CH_HF_HZ] = 8000,
        [CH_COMP_RATIO] = 1, [CH_COMP_RELEASE] = 100,
        [CH_GATE_THRESHOLD] = -30, [CH_GATE_RELEASE] = 100
    };
    memcpy(f->controls, values, sizeof values);
}

static float response(struct fixture *f, float frequency, float amplitude)
{
    double in = 0, out = 0;
    f->plugin->activate(f->handle);
    for (int block = 0; block < SETTLE + MEASURE; ++block) {
        for (int i = 0; i < BLOCK; ++i)
            f->in[0][i] = amplitude * sinf(6.28318530718f * frequency * (block*BLOCK+i) / RATE);
        f->plugin->run(f->handle, BLOCK);
        for (int i = 0; i < BLOCK; ++i) {
            assert(isfinite(f->out[0][i]));
            assert(f->out[1][i] == 0);
            if (block >= SETTLE) {
                in += f->in[0][i] * f->in[0][i];
                out += f->out[0][i] * f->out[0][i];
            }
        }
    }
    return 10 * log10(out / in);
}

static float attack_time(struct fixture *f, int fast)
{
    neutral(f);
    f->controls[CH_COMP_THRESHOLD] = -8.0206f;
    f->controls[CH_COMP_RATIO] = 20;
    f->controls[CH_COMP_HARD] = 0;  /* the reference has no hard-knee mode */
    f->controls[CH_COMP_FAST] = fast;
    f->plugin->activate(f->handle);
    for (int i = 0; i < BLOCK; ++i) f->in[0][i] = .5f;
    const float ten_db = .5f * .316227766f;
    for (int block = 0; block < 20; ++block) {
        f->plugin->run(f->handle, BLOCK);
        for (int i = 0; i < BLOCK; ++i)
            if (f->out[0][i] <= ten_db)
                return 1000.0f * (block*BLOCK+i+1) / RATE;
    }
    return -1;
}

/* Expected values below are a commercial console channel-strip plugin at the same settings,
 * measured offline 10 Nov 2026. Tolerances cover the 44.1 vs 48 kHz fit. */
#define NEAR(v, want, tol) (fabsf((v) - (want)) < (tol))

int main(int argc, char **argv)
{
    assert(argc == 2);
    void *module = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!module) { fprintf(stderr, "%s\n", dlerror()); return 1; }
    load_descriptor = (LADSPA_Descriptor_Function)dlsym(module, "ladspa_descriptor");
    assert(load_descriptor);
    struct fixture f = {0};
    f.plugin = load_descriptor(2);
    assert(f.plugin && f.plugin->UniqueID == HS_CHANNEL_ID && f.plugin == load_descriptor(HS_CHANNEL_ID));
    assert(f.plugin->PortCount == CH_COUNT + 4);
    assert(load_descriptor(0)->UniqueID == 0x485301);
    assert(load_descriptor(1)->UniqueID == 0x485302);
    assert(load_descriptor(3)->UniqueID == HS_XSAT_ID);
    assert(load_descriptor(3) == load_descriptor(HS_XSAT_ID));
    assert(load_descriptor(3)->PortCount == XS_COUNT + 4);
    assert(load_descriptor(4)->UniqueID == HS_BUS_ID);
    assert(load_descriptor(4) == load_descriptor(HS_BUS_ID));
    assert(load_descriptor(4)->PortCount == BUS_COUNT + 4);
    assert(load_descriptor(5)->UniqueID == HS_EQ2_ID);
    assert(load_descriptor(5) == load_descriptor(HS_EQ2_ID));
    assert(load_descriptor(5)->PortCount == EQ_COUNT + 4);
    const LADSPA_Descriptor *eq = load_descriptor(5);
    const LADSPA_PortRangeHint *eq_input = &eq->PortRangeHints[4 + EQ_INPUT];
    const LADSPA_PortRangeHint *eq_output = &eq->PortRangeHints[4 + EQ_OUTPUT];
    assert(LADSPA_IS_HINT_DEFAULT_HIGH(eq_input->HintDescriptor));
    assert(LADSPA_IS_HINT_DEFAULT_HIGH(eq_output->HintDescriptor));
    assert(fabsf(.25f * eq_input->LowerBound + .75f * eq_input->UpperBound) < 1e-6f);
    assert(fabsf(.25f * eq_output->LowerBound + .75f * eq_output->UpperBound) < 1e-6f);

    /* A newly inserted Para EQ must be a true unity stage. SampQOL uses the
       range-derived defaults checked above; all bands begin switched off. */
    LADSPA_Handle eq_handle = eq->instantiate(eq, RATE);
    assert(eq_handle);
    float eq_controls[EQ_COUNT] = {0};
    float eq_in[2][BLOCK], eq_out[2][BLOCK];
    for (int i = 0; i < BLOCK; ++i) {
        eq_in[0][i] = .25f * sinf(i * .17f);
        eq_in[1][i] = .17f * cosf(i * .11f);
    }
    for (int i = 0; i < 2; ++i) {
        eq->connect_port(eq_handle, i, eq_in[i]);
        eq->connect_port(eq_handle, i + 2, eq_out[i]);
    }
    eq_controls[EQ_INPUT] = .25f * eq_input->LowerBound + .75f * eq_input->UpperBound;
    eq_controls[EQ_OUTPUT] = .25f * eq_output->LowerBound + .75f * eq_output->UpperBound;
    for (int i = 0; i < EQ_COUNT; ++i)
        eq->connect_port(eq_handle, i + 4, &eq_controls[i]);
    eq->activate(eq_handle);
    eq->run(eq_handle, BLOCK);
    assert(!memcmp(eq_in, eq_out, sizeof eq_in));
    eq->cleanup(eq_handle);
    assert(load_descriptor(6)->UniqueID == HS_PUMP_ID);
    assert(load_descriptor(6) == load_descriptor(HS_PUMP_ID));
    assert(load_descriptor(6)->PortCount == PU_COUNT + 4);
    assert(load_descriptor(7)->UniqueID == HS_HEAT_ID);
    assert(load_descriptor(7) == load_descriptor(HS_HEAT_ID));
    assert(load_descriptor(7)->PortCount == HE_COUNT + 4);
    assert(!load_descriptor(8));
    f.handle = f.plugin->instantiate(f.plugin, RATE);
    assert(f.handle);
    for (int i = 0; i < 2; ++i) {
        f.plugin->connect_port(f.handle, i, f.in[i]);
        f.plugin->connect_port(f.handle, i+2, f.out[i]);
    }
    for (int i = 0; i < CH_COUNT; ++i)
        f.plugin->connect_port(f.handle, i+4, &f.controls[i]);

    neutral(&f);
    assert(fabsf(response(&f, 997, .1f)) < 1e-6f);
    assert(!memcmp(f.in, f.out, sizeof f.in));
    f.controls[CH_HP] = 200;
    float hp = response(&f, 50, .1f);
    /* Reference plugin, HPF dial 200: -32.16 dB at 50 Hz, -1.56 dB at 200 Hz (corner sits below the dial). */
    assert(NEAR(hp, -32.16f, 1.0f));
    assert(NEAR(response(&f, 200, .1f), -1.56f, .4f));
    neutral(&f);
    f.controls[CH_LP] = 3000;
    float lp = response(&f, 10000, .1f);
    assert(lp < -20);
    neutral(&f);
    f.controls[CH_LF_DB] = 9;
    float lf = response(&f, 25, .1f);
    assert(NEAR(lf, 7.68f, .6f));
    neutral(&f);
    f.controls[CH_LM_DB] = 9;
    float bell = response(&f, 700, .1f);
    assert(NEAR(bell, 5.78f, .6f));  /* the real LMF centre sits ~0.66x the dial here */
    neutral(&f);
    f.controls[CH_HM_DB] = -9;
    float hm = response(&f, 2500, .1f);
    assert(NEAR(hm, -6.41f, .6f));
    neutral(&f);
    f.controls[CH_HF_DB] = 9;
    float hf = response(&f, 15000, .1f);
    assert(NEAR(hf, 7.14f, .75f));
    neutral(&f);
    f.controls[CH_COMP_THRESHOLD] = 0;
    f.controls[CH_COMP_RATIO] = 4;
    float comp = response(&f, 997, .5f);
    assert(NEAR(comp, -4.74f, .75f));
    assert(fabsf(response(&f, 997, .04f)) < .05f);
    neutral(&f);
    f.controls[CH_GATE_THRESHOLD] = -7;
    f.controls[CH_GATE_RANGE] = 24;
    f.controls[CH_GATE_RELEASE] = 100;
    float gate = response(&f, 997, .01f);
    assert(NEAR(gate, -30.23f, 2.5f));  /* reference: -30.23 (depth runs past the range dial); emulator ~1.7 dB shallower here */
    f.controls[CH_GATE_EXPAND] = 1;
    /* Reference plugin, threshold -7, range 24, expander: open (0.0 dB) at -28 dBFS, -4.46 dB at -34 dBFS. */
    assert(fabsf(response(&f, 997, .04f)) < .1f);
    float expand = response(&f, 997, .02f);
    assert(NEAR(expand, -4.46f, 1.5f));
    float slow_attack = attack_time(&f, 0);
    float fast_attack = attack_time(&f, 1);
    printf("EQ vs real: LF %+.2f (7.68) LM %+.2f (5.78) HM %+.2f (-6.41) HF %+.2f (7.14), comp %.2f (-4.74)\n", lf, bell, hm, hf, comp);
    assert(NEAR(slow_attack, 46, 12) && NEAR(fast_attack, 1.5f, .75f));  /* real: 46 / 1.5 ms */
    printf("Time to 10 dB reduction: slow %.2f ms, fast %.2f ms\n", slow_attack, fast_attack);
    neutral(&f);
    f.controls[CH_DRIVE] = 100;
    assert(response(&f, 997, .5f) < -4);
    memset(f.in, 0, sizeof f.in);
    f.plugin->run(f.handle, BLOCK);
    for (int i = 0; i < BLOCK; ++i)
        assert(f.out[0][i] == 0 && f.out[1][i] == 0);
    printf("Unity and stereo isolation passed. HP %.2f, LP %.2f, bell %+.2f, compressor %.2f, gate %.2f dB\n",
           hp, lp, bell, comp, gate);

    f.controls[CH_HP] = 100;
    f.controls[CH_LP] = 12000;
    f.controls[CH_LF_DB] = 3;
    f.controls[CH_LM_DB] = -3;
    f.controls[CH_HM_DB] = 4;
    f.controls[CH_HF_DB] = 3;
    f.controls[CH_COMP_THRESHOLD] = -20;
    f.controls[CH_COMP_RATIO] = 4;
    f.controls[CH_GATE_RANGE] = 12;
    f.controls[CH_GATE_EXPAND] = 1;
    for (int i = 0; i < BLOCK; ++i) f.in[0][i] = f.in[1][i] = .5f * sinf(i * .11f);
    clock_t begin = clock();
    for (int block = 0; block < 50000; ++block) f.plugin->run(f.handle, BLOCK);
    double seconds = (clock() - begin) / (double)CLOCKS_PER_SEC;
    printf("All sections active, %.1fx realtime in isolated DSP test\n", 50000.0 * BLOCK / RATE / seconds);
    f.plugin->cleanup(f.handle);
}
