/* Host check for the browser key tag. Builds and runs on the Mac:
 *   cc -O2 -I. check_key.c key_detect.c preview_tempo.c -o /tmp/check-key -lm && /tmp/check-key [FILE.wav ...]
 * With no files: synthetic loops of known key must read right. With files: prints what the tag shows. */
#define _GNU_SOURCE
#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "key_detect.h"
float browser_detect_tempo(const char *path);

enum { SR = 44100 };

/* one chord per bar at 90 BPM, three harmonics per note plus the root an octave down */
static float *render(const int chords[][3], int bars, long *n) {
    long bar = (long)(SR * 4 * 60 / 90.0);
    *n = bar * bars;
    float *y = calloc(*n, sizeof *y);
    for (int b = 0; b < bars; b++)
        for (int v = -1; v < 3; v++) {
            int midi = v < 0 ? chords[b][0] - 12 : chords[b][v];
            double f = 440 * pow(2, (midi - 69) / 12.0);
            for (long i = 0; i < bar; i++) {
                double t = (double)i / SR, env = exp(-1.5 * t);
                for (int h = 1; h <= 3; h++) y[b * bar + i] += 0.08f * env / h * sin(2 * M_PI * f * h * t);
            }
        }
    return y;
}

static void expect(const char *want, const int chords[][3], int bars) {
    long n; float *y = render(chords, bars, &n);
    int root, scale; char got[32];
    assert(browser_detect_key(y, n, SR, 90, &root, &scale) == 0);
    browser_key_text(root, scale, got, sizeof got);
    printf("%-3s %-10s got %s\n", strcmp(got, want) ? "XX" : "OK", want, got);
    assert(!strcmp(got, want));
    free(y);
}

int main(int argc, char **argv) {
    if (argc > 1) {
        for (int i = 1; i < argc; i++) {
            unsigned n, sr; float *y = browser_decode(argv[i], &n, &sr);
            /* the browser's order: "<n> BPM" in the name first, else the detected tempo */
            const char *b = strcasestr(argv[i], "bpm"), *d = b;
            while (d && d > argv[i] && (d[-1] == ' ' || d[-1] == '_' || d[-1] == '-' || isdigit((unsigned char)d[-1]))) d--;
            float bpm = d && d < b ? strtof(d + strspn(d, " _-"), NULL) : 0;
            if (!(bpm >= 40 && bpm <= 300)) bpm = browser_detect_tempo(argv[i]);
            int root = -1, scale = 0; char key[32] = "-";
            if (y && !browser_is_drums(argv[i]) && !browser_detect_key(y, n, sr, bpm, &root, &scale))
                browser_key_text(root, scale, key, sizeof key);
            printf("%-12s %6.1f BPM  %s\n", key, bpm, argv[i]);
            free(y);
        }
        return 0;
    }
    /* MIDI notes: C4 = 60 */
    static const int cmaj[][3] = {{60,64,67},{65,69,72},{67,71,74},{60,64,67}};      /* C F G C */
    static const int amin[][3] = {{57,60,64},{62,65,69},{64,67,71},{57,60,64}};      /* Am Dm Em Am */
    static const int csmin[][3] = {{61,64,68},{66,69,73},{68,71,75},{61,64,68}};     /* C#m F#m G#m C#m */
    static const int bbmaj[][3] = {{58,62,65},{63,67,70},{65,69,72},{58,62,65}};     /* Bb Eb F Bb */
    static const int gmin[][3] = {{55,58,62},{60,63,67},{62,65,69},{55,58,62}};      /* Gm Cm Dm Gm */
    expect("C MAJ", cmaj, 4);
    expect("A MIN", amin, 4);
    expect("C# MIN", csmin, 4);
    expect("A# MAJ", bbmaj, 4);
    expect("G MIN", gmin, 4);
    for (int k = 0; k < 2; k++) {   /* a 1.5 s and a 0.2 s E one-shot read as their note */
        long n = k ? SR / 5 : SR * 3 / 2; float *y = calloc(n, sizeof *y);
        for (long i = 0; i < n; i++) for (int h = 1; h <= 3; h++) y[i] += 0.2f / h * sin(2 * M_PI * 164.81 * h * i / SR);
        int root, scale; char got[32];
        assert(!browser_detect_key(y, n, SR, 0, &root, &scale));
        browser_key_text(root, scale, got, sizeof got);
        printf("%-3s %-10s got %s\n", strcmp(got, "NOTE E") ? "XX" : "OK", "NOTE E", got);
        assert(!strcmp(got, "NOTE E"));
        free(y);
    }
    assert(browser_is_drums("/x/Funky Drums 90.wav") && browser_is_drums("/x/hh_loop.wav"));
    assert(!browser_is_drums("/x/05 - Breakfast Club CMaj 64bpm.wav"));
    { int r, s; float z[100] = {0}; assert(browser_detect_key(z, 100, SR, 90, &r, &s) == -1); }
    puts("check-key: all passed");
    return 0;
}
