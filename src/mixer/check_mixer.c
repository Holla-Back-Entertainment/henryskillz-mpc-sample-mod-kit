// Host check for the console's strip reads (graphic_mixer.c): mute and solo, colour, meter scale.
// gcc -O2 -Wall -Wextra check_mixer.c -o /tmp/check-mixer -lm -ldl && /tmp/check-mixer
#include "graphic_mixer.c"
#include <stdio.h>

static int failed;
static void expect(int ok, const char *what) { if (!ok) { printf("FAIL %s\n", what); failed = 1; } }

int main(void)
{
    /* Meter: linear amplitude, 20 log10. 0 dBFS (1.0) and -6.02 dBFS (0.5) are 6 rows apart. */
    expect(meter_y(1.0f) == 34, "meter 0 dBFS");
    expect(meter_y(0.5f) == 40, "meter -6 dBFS");
    expect(meter_y(0.001f) == 88, "meter -60 dBFS clamps to -54");
    expect(meter_y(0.0f) == 90, "meter silence");

    /* Colour: 0x00RRGGBB as is, 0 = default grey. */
    uint8_t s[120] = {0};
    uint32_t c = 0xff0000; memcpy(s + 40, &c, 4);
    expect(strip_colour(s) == 0xff0000, "red stays red");
    c = 0x123456; memcpy(s + 40, &c, 4);
    expect(strip_colour(s) == 0x123456, "colour as is");
    c = 0; memcpy(s + 40, &c, 4);
    expect(strip_colour(s) == rgb(205, 211, 216), "no colour");

    /* Console: strip 0 muted, strip 1 soloed (int 0/1 at +72 and +76). */
    static uint8_t snap[12 + 2 * 120];
    int two = 2, one = 1;
    memcpy(snap + 4, &two, 4);
    memcpy(snap + 12 + 72, &one, 4);
    memcpy(snap + 12 + 120 + 76, &one, 4);
    static int selected[3], editing[3];
    struct qol_map q = {0};
    q.mix_snapshot = (uintptr_t)snap; q.mix_selected = (uintptr_t)selected; q.mix_editing = (uintptr_t)editing;
    q.strip_bytes = 120;
    draw_console(&q, 0, 2);
    const uint32_t on_mute = rgb(245, 91, 30), on_solo = rgb(255, 194, 25), off = rgb(61, 66, 70);
    #define PX(x, y) console_pixels[(y) * W + (x)]
    expect(PX(4 + 5, 20) == on_mute, "strip 0 mute lit");
    expect(PX(4 + 21, 20) == off, "strip 0 solo dark");
    expect(PX(43 + 5, 20) == off, "strip 1 mute dark");
    expect(PX(43 + 21, 20) == on_solo, "strip 1 solo lit");

    puts(failed ? "check_mixer: FAIL" : "check_mixer: OK");
    return failed;
}
