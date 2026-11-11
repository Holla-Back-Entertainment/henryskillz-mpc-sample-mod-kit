/* Renders the Crush and Bass Focus panels from the real faceplate code for the README (Linux, e.g. gcc:12):
   gcc -w -I. panel-render.c hs_ducker_gain.c -o r -lm -ldl -pthread && ./r crush 37 30 45 6 crush.ppm INPUT RATIO OUTPUT   (3x, rows 32..160) */
#include "hs_fx.c"
#include <stdio.h>
int main(int c, char **v)
{
    static struct effect_page page;
    if (c < 10) return 2;   /* kind v0 v1 v2 gr out.ppm label0 label1 label2 */
    page.kind = !strcmp(v[1], "crush") ? HS_DISTRESSOR : HS_BASS_FOCUS;
    for (int i = 0; i < 3; ++i) page.values[i] = page.main_values[i] = atoi(v[2 + i]);
    for (int i = 3; i < KNOBS; ++i) page.values[i] = page.main_values[i] = -1;
    dist_gr_db = atof(v[5]);
    static const int rows[] = {57, 73, 89};          /* the app prints knob names here; draw_gear copies them */
    for (int i = 0; i < 3; ++i) text(page.stock, 52, rows[i] + 3, v[7 + i], (int)strlen(v[7 + i]), 0xffffff);
    const uint32_t *px = draw_gear(&page);
    FILE *f = fopen(v[6], "wb");
    fprintf(f, "P6 %d %d 255\n", W * 3, 128 * 3);
    for (int y = 32 * 3; y < 160 * 3; ++y)
        for (int x = 0; x < W * 3; ++x) {
            uint32_t q = px[(y / 3) * W + x / 3];
            fputc(q >> 16 & 255, f); fputc(q >> 8 & 255, f); fputc(q & 255, f);
        }
    return fclose(f);
}
