#include <time.h>
#include "hs_env.h"

/* ponytail: last sender wins and a float store is atomic on aarch64; two tracks on one bus would
   flicker between them, give the bus a max-of-senders if anyone needs that. */
static volatile float bus_peak[HS_ENV_BUSES];
static volatile long long bus_ms[HS_ENV_BUSES];

static long long now_ms(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec*1000LL + t.tv_nsec/1000000;
}

void hs_env_send(int bus, float peak)
{
    if (bus < 1 || bus > HS_ENV_BUSES) return;
    bus_peak[bus-1] = peak;
    bus_ms[bus-1] = now_ms();
}

float hs_env_read(int bus)
{
    if (bus < 1 || bus > HS_ENV_BUSES || now_ms() - bus_ms[bus-1] > 100) return 0;
    return bus_peak[bus-1];
}
