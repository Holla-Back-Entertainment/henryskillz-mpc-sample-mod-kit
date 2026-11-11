/* lufs.h: ITU-R BS.1770-4 / EBU R128 loudness of a stereo stream.
 * K-weighting (two biquads per channel), 100 ms blocks, momentary = last 400 ms, short-term = last 3 s,
 * integrated = 400 ms blocks (75% overlap) gated at -70 LUFS absolute and -10 LU relative.
 * Integrated restarts after LUFS_RESET_S of digital silence, so stopping playback starts a new measurement.
 * ponytail: sample peak, not true peak (no 4x oversampling); add a polyphase upsampler if true peak matters. */
#ifndef HS_LUFS_H
#define HS_LUFS_H
#include <math.h>
#include <string.h>

#define LUFS_RESET_S 2.f
enum { LUFS_HIST = 750 };                 /* 0.1 LU bins from -70 to +5 LUFS */

struct lufs {
    unsigned rate, block, n;              /* rate, frames per 100 ms block, frames in the current block */
    double b[2][3], a[2][3];              /* stage 0 shelf, stage 1 high-pass */
    double z[2][2][2];                    /* [channel][stage][state] */
    double acc;                           /* K-weighted energy summed over channels, current block */
    double blk[30];                       /* last 30 block energies (mean square per block) */
    int nblk, silent_blocks;
    unsigned cnt[LUFS_HIST]; double sum[LUFS_HIST];   /* gating histogram of 400 ms blocks */
    float m, s, i;                        /* published LUFS, -INFINITY when nothing yet */
    float peak;                           /* max sample peak since the integrated restarted */
};

static double lufs_of(double ms) { return ms > 0 ? -0.691 + 10 * log10(ms) : -INFINITY; }

static void lufs_reset_integrated(struct lufs *L)
{
    memset(L->cnt, 0, sizeof L->cnt); memset(L->sum, 0, sizeof L->sum); L->i = -INFINITY; L->peak = 0;
}

static void lufs_init(struct lufs *L, unsigned rate)
{
    memset(L, 0, sizeof *L);
    L->rate = rate ? rate : 48000; L->block = L->rate / 10;
    double K = tan(M_PI * 1681.974450955533 / L->rate), Q = 0.7071752369554196;
    double Vh = pow(10, 3.999843853973347 / 20), Vb = pow(Vh, 0.4996667741545416), a0 = 1 + K / Q + K * K;
    L->b[0][0] = (Vh + Vb * K / Q + K * K) / a0; L->b[0][1] = 2 * (K * K - Vh) / a0; L->b[0][2] = (Vh - Vb * K / Q + K * K) / a0;
    L->a[0][1] = 2 * (K * K - 1) / a0; L->a[0][2] = (1 - K / Q + K * K) / a0;
    K = tan(M_PI * 38.13547087602444 / L->rate); Q = 0.5003270373238773; a0 = 1 + K / Q + K * K;
    L->b[1][0] = 1; L->b[1][1] = -2; L->b[1][2] = 1;
    L->a[1][1] = 2 * (K * K - 1) / a0; L->a[1][2] = (1 - K / Q + K * K) / a0;
    L->m = L->s = L->i = -INFINITY;
}

static double lufs_mean(const struct lufs *L, int k)  /* mean of the last k blocks (fewer if not there yet) */
{
    int have = L->nblk < k ? L->nblk : k; double e = 0;
    for (int j = 0; j < have; ++j) e += L->blk[j];
    return have ? e / have : 0;
}

static void lufs_end_block(struct lufs *L)
{
    double e = L->acc / L->block; L->acc = 0; L->n = 0;
    memmove(L->blk + 1, L->blk, sizeof L->blk - sizeof *L->blk); L->blk[0] = e;
    if (L->nblk < 30) ++L->nblk;
    L->silent_blocks = e < 1e-14 ? L->silent_blocks + 1 : 0;
    if (L->silent_blocks == (int)(LUFS_RESET_S * 10)) lufs_reset_integrated(L);

    double m = L->nblk >= 4 ? lufs_mean(L, 4) : 0;
    L->m = (float)lufs_of(m);
    L->s = (float)lufs_of(lufs_mean(L, 30));
    double lm = lufs_of(m);
    if (L->nblk >= 4 && lm >= -70) {      /* absolute gate */
        int bin = (int)((lm + 70) * 10); if (bin >= LUFS_HIST) bin = LUFS_HIST - 1;
        ++L->cnt[bin]; L->sum[bin] += m;
        double e1 = 0; unsigned n1 = 0;
        for (int j = 0; j < LUFS_HIST; ++j) { e1 += L->sum[j]; n1 += L->cnt[j]; }
        double gate = lufs_of(e1 / n1) - 10, e2 = 0; unsigned n2 = 0;
        for (int j = (int)fmax(0, ceil((gate + 70) * 10)); j < LUFS_HIST; ++j) { e2 += L->sum[j]; n2 += L->cnt[j]; }
        L->i = n2 ? (float)lufs_of(e2 / n2) : -INFINITY;
    }
}

static inline double lufs_k(struct lufs *L, int c, double x)
{
    for (int st = 0; st < 2; ++st) {      /* transposed direct form II */
        double *z = L->z[c][st], y = L->b[st][0] * x + z[0];
        z[0] = L->b[st][1] * x - L->a[st][1] * y + z[1];
        z[1] = L->b[st][2] * x - L->a[st][2] * y;
        x = y;
    }
    return x;
}

static inline void lufs_frame(struct lufs *L, float l, float r)
{
    float al = fabsf(l), ar = fabsf(r), pk = al > ar ? al : ar;
    if (pk > L->peak) L->peak = pk;
    double a = lufs_k(L, 0, l), b = lufs_k(L, 1, r);
    L->acc += a * a + b * b;
    if (++L->n >= L->block) lufs_end_block(L);
}
#endif
