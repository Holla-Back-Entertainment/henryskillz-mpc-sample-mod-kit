// check_import.c: host check for warp on import's loop test (cc check_import.c import_warp.c -lm -pthread).
#include <assert.h>
#include <stdio.h>
int import_warp_is_loop(long frames, int rate, float bpm);
int main(void)
{
    assert(import_warp_is_loop(522779, 44100, 81));        // Break-KitA3 81: 11.85 s, 16 beats
    assert(import_warp_is_loop(48000 * 4, 48000, 120));    // 2 bars at 120, 48 kHz
    assert(import_warp_is_loop(44100 * 2, 44100, 120));    // exactly 1 bar
    assert(!import_warp_is_loop(16339, 44100, 88));        // Break-Beet1 88: a 0.37 s hit
    assert(!import_warp_is_loop(60416, 44100, 87.592f));   // Drill-808-Knock1: 2 beats
    assert(!import_warp_is_loop(348552, 44100, 78));       // an 81 BPM stem read as 78: 10.3 beats
    assert(!import_warp_is_loop(44100 * 3, 44100, 90));    // 4.5 beats: not on a beat
    assert(!import_warp_is_loop(44100 * 3, 44100, 120));   // 6 beats: on a beat, not on a bar
    assert(!import_warp_is_loop(522779, 44100, 0));        // no tempo
    assert(!import_warp_is_loop(522779, 0, 81));
    assert(!import_warp_is_loop(0, 44100, 81));
    puts("check_import: ok");
    return 0;
}
