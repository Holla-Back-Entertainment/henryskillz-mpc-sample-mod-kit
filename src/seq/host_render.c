/* Offline check: raw 320x240 XRGB in -> restyled raw out; exit 3 if not detected as SEQ.
   Optional: RMS_DBFS PEAK_DBFS to preview the meters at a level. */
#define HS_SEQ_HOST
#include "hs_seq.c"
#include <stdio.h>
int main(int c, char **v)
{
    static uint32_t s[N], d[N];
    FILE *f = c >= 3 ? fopen(v[1], "rb") : 0;
    if (c == 5) for (int k = 0; k < 2; ++k) { hs_seq_ms[k] = powf(10, atof(v[3]) / 10); hs_seq_pk[k] = powf(10, atof(v[4]) / 20); }
    if (!f || fread(s, 4, N, f) != N) return 2;
    fclose(f);
    if (!hs_seq_is_seq(s)) return 3;
    hs_seq_draw(s, d);
    f = fopen(v[2], "wb"); if (!f) return 2;
    fwrite(d, 4, N, f); fclose(f); return 0;
}
