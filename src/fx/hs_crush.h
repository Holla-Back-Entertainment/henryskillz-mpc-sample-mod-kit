#ifndef HS_CRUSH_H
#define HS_CRUSH_H
#include "hs_mod.h"
enum { HS_CRUSH_ID = 0x48530B };
/* Knobs 0-10 like the hardware. Ratio 0-7: 1, 2, 3, 4, 6, 10 Opto, 20, Max.
 * Detector 0-3: Norm, HP, Band, HP+Band. Audio 0-5: Clean, HP, Dist 2, Dist 3, HP+D2, HP+D3. */
enum { DS_INPUT, DS_ATTACK, DS_RELEASE, DS_OUTPUT, DS_RATIO, DS_DETECTOR, DS_AUDIO, DS_BRIT,
       DS_MIX, DS_BYPASS, DS_KEY, DS_SEND, DS_MOD, DS_AUTO = DS_MOD + HS_MOD_PORTS, DS_COUNT };   /* Key In 0 Own, 1-4 bus A-D; Send 0 Off, 1-4 */
/* DS_AUTO (Auto Gain, v0.8) comes after the MOD ports: old projects load it at 0, off. */
/* Gain reduction in dB of one instance, for the panel meter. */
float hs_crush_gr(const void *handle);
#endif
