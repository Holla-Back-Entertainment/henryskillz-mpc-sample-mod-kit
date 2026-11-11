#ifndef HS_PUMP_H
#define HS_PUMP_H
enum { HS_PUMP_ID = 0x485308 };
enum { PU_INPUT, PU_RATIO, PU_ATTACK, PU_RELEASE, PU_ATMOD, PU_DET_HP, PU_SAT,
       PU_OUTPUT, PU_MIX, PU_BYPASS,
       PU_KEY, PU_SEND, PU_COUNT };   /* Key In 0 Own, 1-4 bus A-D; Send 0 Off, 1-4 */
#endif
