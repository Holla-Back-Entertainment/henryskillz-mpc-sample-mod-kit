#include "hs_air_exciter.h"

#include <math.h>

void hs_air_exciter_configure(struct hs_air_exciter *state, float sample_rate,
                              float frequency, float drive_db, float amount)
{
    sample_rate = fminf(192000.0f, fmaxf(8000.0f, sample_rate));
    frequency = fminf(fminf(12000.0f, sample_rate * 0.4f), fmaxf(2000.0f, frequency));
    state->filter_coefficient = 1.0f - expf(-6.2831853f * frequency / sample_rate);
    state->drive = powf(10.0f, fminf(18.0f, fmaxf(0.0f, drive_db)) / 20.0f);
    state->amount = 0.35f * fminf(1.0f, fmaxf(0.0f, amount));
}

void hs_air_exciter_process(struct hs_air_exciter *state, float *left, float *right,
                            size_t frames)
{
    if (state->amount == 0.0f) {
        state->input_low[0] = state->input_low[1] = 0.0f;
        state->harmonic_low[0] = state->harmonic_low[1] = 0.0f;
        return;
    }

    float *const channels[] = {left, right};
    for (int channel = 0; channel < 2; ++channel) {
        float *samples = channels[channel];
        float input_low = state->input_low[channel];
        float harmonic_low = state->harmonic_low[channel];
        for (size_t i = 0; i < frames; ++i) {
            float dry = samples[i];
            input_low += state->filter_coefficient * (dry - input_low);
            float driven = (dry - input_low) * state->drive;
            float shaped = driven / (1.0f + fabsf(driven));
            harmonic_low += state->filter_coefficient * (shaped - harmonic_low);
            samples[i] = dry + state->amount * (shaped - harmonic_low);
        }
        state->input_low[channel] = fabsf(input_low) > 1e-20f ? input_low : 0.0f;
        state->harmonic_low[channel] = fabsf(harmonic_low) > 1e-20f ? harmonic_low : 0.0f;
    }
}
