/* MP3 read check (in build.sh's container): a 1 kHz 48 kHz MP3 at -27.3 dB peak (ffmpeg volumedetect) decodes to ~1 s of 44.1 kHz audio. */
#include "hs_stems.c"
#undef main
#include <assert.h>
int main(int argc, char **argv) {
    long n; float *x = read_mp3(argc > 1 ? argv[1] : "test/tone48k.mp3", &n);
    double peak = 0; for (long i = 0; i < n * 2; i++) if (fabs(x[i]) > peak) peak = fabs(x[i]);
    setvbuf(stdout, 0, _IONBF, 0); printf("mp3: %ld frames at 44.1 kHz, peak %.3f\n", n, peak);
    assert(n > 44100 - 2000 && n < 44100 + 3000 && peak > 0.038 && peak < 0.048);
    puts("check-mp3: OK"); return 0;
}
