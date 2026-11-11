#ifndef HS_TUBE_STRIP_H
#define HS_TUBE_STRIP_H
#include "hs_mod.h"
enum { HS_TUBE_STRIP_ID = 0x48530D };
/* Bass Hz 0-3: 15, 30, 60, 150. Treble Hz 0-3: 10k, 15k, 20k, 32k (clamped below Nyquist).
 * Order 0 = Comp then EQ, 1 = EQ then Comp. SC EQ moves the two mids into the compressor's detector. */
enum { TS_DRIVE, TS_HPF, TS_OUTPUT, TS_THRESHOLD, TS_RATIO, TS_ATTACK, TS_RELEASE, TS_ORDER, TS_SC,
       TS_BASS_HZ, TS_BASS_DB, TS_LMID_DB, TS_LMID_HZ, TS_HMID_HZ, TS_HMID_DB, TS_TREBLE_HZ, TS_TREBLE_DB,
       TS_Q, TS_BYPASS, TS_MOD, TS_COUNT = TS_MOD + HS_MOD_PORTS };
/* Gain reduction in dB of one instance, for the panel meter. */
float hs_tube_strip_gr(const void *handle);
#endif
