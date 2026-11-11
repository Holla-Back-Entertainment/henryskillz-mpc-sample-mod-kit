#ifndef HS_XSAT_H
#define HS_XSAT_H
#include "hs_mod.h"

enum { HS_XSAT_ID = 0x485305 };
enum { XS_DRIVE, XS_SHAPE, XS_DEPTH, XS_HARMONICS, XS_MIX, XS_HEADROOM,
       XS_INPUT, XS_OUTPUT, XS_BYPASS, XS_MOD, XS_COUNT = XS_MOD + HS_MOD_PORTS };

#endif
