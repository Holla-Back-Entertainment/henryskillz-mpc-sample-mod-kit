#ifndef HS_AIR_EXCITER_H
#define HS_AIR_EXCITER_H

#include <stddef.h>

struct hs_air_exciter {
    float filter_coefficient;
    float drive;
    float amount;
    float input_low[2];
    float harmonic_low[2];
};

/* Configure outside the audio callback. Zero-initialize state before first use. */
void hs_air_exciter_configure(struct hs_air_exciter *state, float sample_rate,
                              float frequency, float drive_db, float amount);
void hs_air_exciter_process(struct hs_air_exciter *state, float *left, float *right,
                            size_t frames);

#endif
