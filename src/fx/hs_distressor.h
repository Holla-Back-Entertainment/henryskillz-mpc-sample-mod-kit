#ifndef HS_DISTRESSOR_H
#define HS_DISTRESSOR_H
enum { HS_DISTRESSOR_ID = 0x48530B };
/* Knobs 0-10 like the hardware. Ratio 0-7: 1, 2, 3, 4, 6, 10 Opto, 20, Max.
 * Detector 0-3: Norm, HP, Band, HP+Band. Audio 0-5: Clean, HP, Dist 2, Dist 3, HP+D2, HP+D3. */
enum { DS_INPUT, DS_ATTACK, DS_RELEASE, DS_OUTPUT, DS_RATIO, DS_DETECTOR, DS_AUDIO, DS_BRIT,
       DS_MIX, DS_BYPASS, DS_COUNT };
/* Gain reduction in dB of one instance, for the panel meter. */
float hs_distressor_gr(const void *handle);
#endif
