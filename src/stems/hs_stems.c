// hs-stems: on-device 4-stem separation (vocals, drums, bass, other) for the MPC Sample.
// Reimplements Spleeter 4stems inference (Deezer, MIT) in plain C + OpenBLAS sgemm.
// Weights: export_weights.py output. Usage:
//   hs-stems WEIGHTS IN.wav OUTDIR [PREFIX [PICK]]   -> OUTDIR/PREFIX-Vocals.wav ... (16-bit, 44.1 kHz stereo;
//   any input rate is converted to 44.1 kHz first, the rate the model was trained at)
//   PICK: the stems to write, by letter (V D B O), default VDBO. A trailing + sums them into one file:
//   "BO+" -> PREFIX-Bass+Other.wav. Summing masks before the iSTFT is exact (the iSTFT is linear).
// Prints "progress N/M" lines to stdout so a caller can show a bar.
#define _GNU_SOURCE
#include <cblas.h>
#include <sched.h>
#include <sys/prctl.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define NFFT 4096
#define HOP 1024
#define NBIN (NFFT / 2 + 1)
#define T 512   // frames per segment
#define F 1024  // modelled bins (bins >= F get mask 0, Spleeter "zeros" extension)
#define NSTEM 4
static const char *STEM[NSTEM] = {"Vocals", "Drums", "Bass", "Other"};

static void die(const char *m) { fprintf(stderr, "hs-stems: %s\n", m); exit(1); }
static void *xcalloc(size_t n, size_t s) { void *p = calloc(n, s); if (!p) die("out of memory"); return p; }

// ---------------- resampling (any rate -> 44.1 kHz, the model's rate) ----------------
// Windowed sinc, 32 zero crossings each side, Blackman window, cutoff just under the lower Nyquist.
// ponytail: one shared kernel table, linearly interpolated (~67 taps per output sample).
#define RS_ZC 32
#define RS_RES 512                                         // kernel table points per zero crossing
static float *resample(const float *in, long n, uint32_t sr, long *frames) {
    static float tab[RS_ZC * RS_RES + 2];                  // windowed sinc at |x| = k / RS_RES, built once
    if (!tab[0]) for (int k = 0; k <= RS_ZC * RS_RES + 1; k++) {
        double x = (double)k / RS_RES, u = (x / RS_ZC + 1) / 2;
        double si = k ? sin(M_PI * x) / (M_PI * x) : 1;
        tab[k] = x >= RS_ZC ? 0 : (float)(si * (0.42 - 0.5 * cos(2 * M_PI * u) + 0.08 * cos(4 * M_PI * u)));
    }
    double step = (double)sr / 44100;                     // input samples per output sample
    float fc = step > 1 ? (float)(0.97 / step) : 0.97f;   // cutoff as a fraction of the input Nyquist
    long m = (long)((double)n / step);
    float *out = xcalloc((size_t)(m > 0 ? m : 1) * 2, sizeof(float));
    long half = (long)(RS_ZC / fc) + 1;                   // filter half-width in input samples
    for (long j = 0; j < m; j++) {
        double t = j * step; long c = (long)t;
        long a = c - half, b = c + half;
        if (a < 0) a = 0; if (b > n - 1) b = n - 1;
        float sl = 0, sr_ = 0, wsum = 0;
        for (long i = a; i <= b; i++) {
            float x = fabsf((float)(i - t) * fc) * RS_RES;
            if (x >= RS_ZC * RS_RES) continue;
            int k = (int)x; float f = x - k, w = tab[k] + f * (tab[k + 1] - tab[k]);
            sl += w * in[i * 2]; sr_ += w * in[i * 2 + 1]; wsum += w;
        }
        if (wsum != 0) { out[j * 2] = sl / wsum; out[j * 2 + 1] = sr_ / wsum; }
    }
    *frames = m;
    return out;
}

// ---------------- MP3 (minimp3, public domain; the same decoder key detect uses) ----------------
#define MINIMP3_IMPLEMENTATION
#define MINIMP3_FLOAT_OUTPUT
#define MINIMP3_NO_STDIO
#include "minimp3.h"
static float *read_mp3(const char *path, long *frames) {
    FILE *f = fopen(path, "rb"); if (!f) die("cannot open input");
    fseek(f, 0, SEEK_END); long len = ftell(f); rewind(f);
    if (len < 128 || len > 200L << 20) die("MP3 size out of range");
    uint8_t *buf = malloc((size_t)len); if (!buf) die("out of memory");
    if (fread(buf, 1, (size_t)len, f) != (size_t)len) die("cannot read input");
    fclose(f);
    long pos = 0;    // skip an ID3v2 tag (cover art can be MBs)
    if (!memcmp(buf, "ID3", 3) && !((buf[6] | buf[7] | buf[8] | buf[9]) & 0x80))
        pos = 10 + ((long)buf[6] << 21 | (long)buf[7] << 14 | (long)buf[8] << 7 | buf[9]) + (buf[5] & 0x10 ? 10 : 0);
    mp3dec_t dec; mp3dec_init(&dec);
    float pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];
    long cap = 1 << 20, n = 0; int hz = 0;
    float *out = xcalloc((size_t)cap * 2, sizeof(float));
    while (pos < len) {
        mp3dec_frame_info_t info;
        int k = mp3dec_decode_frame(&dec, buf + pos, (int)(len - pos > INT32_MAX ? INT32_MAX : len - pos), pcm, &info);
        if (!info.frame_bytes) break;
        pos += info.frame_bytes;
        if (!k) continue;
        if (!hz) hz = info.hz;
        if (info.hz != hz || info.channels < 1 || info.channels > 2) continue;   // ignore odd frames mid-stream
        if (n + k > 8L * 60 * hz) die("file too long (max 8 min)");
        if (n + k > cap) { cap *= 2; out = realloc(out, (size_t)cap * 2 * sizeof(float)); if (!out) die("out of memory"); }
        for (int i = 0; i < k; i++) {
            out[(n + i) * 2] = pcm[i * info.channels];
            out[(n + i) * 2 + 1] = pcm[i * info.channels + info.channels - 1];
        }
        n += k;
    }
    free(buf);
    if (!n) die("no MP3 audio found");
    *frames = n;
    if (hz != 44100) { float *r = resample(out, n, (uint32_t)hz, frames); free(out); out = r; }
    return out;
}

// ---------------- WAV I/O ----------------
static float *read_wav(const char *path, long *frames) {
    FILE *f = fopen(path, "rb"); if (!f) die("cannot open input");
    uint8_t h[12]; if (fread(h, 1, 12, f) != 12 || memcmp(h, "RIFF", 4) || memcmp(h + 8, "WAVE", 4)) die("not a WAV");
    int fmt = 0, ch = 0, bits = 0; uint32_t sr = 0; float *out = NULL;
    for (;;) {
        uint8_t c[8]; if (fread(c, 1, 8, f) != 8) die("WAV has no data chunk");
        uint32_t len = c[4] | c[5] << 8 | c[6] << 16 | (uint32_t)c[7] << 24;
        if (!memcmp(c, "fmt ", 4)) {
            uint8_t b[40] = {0}; if (len < 16 || fread(b, 1, len < 40 ? len : 40, f) < 16) die("bad fmt chunk");
            if (len > 40) fseek(f, len - 40, SEEK_CUR);
            fmt = b[0] | b[1] << 8; ch = b[2] | b[3] << 8; sr = b[4] | b[5] << 8 | b[6] << 16 | (uint32_t)b[7] << 24; bits = b[14] | b[15] << 8;
            if (fmt == 0xFFFE && len >= 26) fmt = b[24] | b[25] << 8;  // WAVE_FORMAT_EXTENSIBLE subformat
        } else if (!memcmp(c, "data", 4)) {
            if (!ch) die("data before fmt");
            if (sr < 8000 || sr > 384000) die("unsupported sample rate");
            if (ch < 1 || ch > 2) die("only mono or stereo input");
            if (!((fmt == 1 && (bits == 16 || bits == 24 || bits == 32)) || (fmt == 3 && bits == 32))) die("unsupported sample format");
            int bps = bits / 8; long n = len / (bps * ch); *frames = n;
            if (n > 8L * 60 * (long)sr) die("file too long (max 8 min)");   /* ~620 MB peak; longer risks running the Sample out of memory */
            uint8_t *raw = malloc((size_t)len); if (!raw) die("out of memory");
            n = fread(raw, 1, len, f) / (bps * ch); *frames = n;
            out = xcalloc((size_t)n * 2, sizeof(float));
            for (long i = 0; i < n; i++)
                for (int k = 0; k < 2; k++) {
                    uint8_t *p = raw + (i * ch + (ch == 2 ? k : 0)) * bps; float v;
                    if (fmt == 3) memcpy(&v, p, 4);
                    else if (bits == 16) v = (int16_t)(p[0] | p[1] << 8) / 32768.f;
                    else if (bits == 24) v = (int32_t)((uint32_t)p[0] << 8 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 24) / 2147483648.f;
                    else v = (int32_t)(p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24) / 2147483648.f;
                    out[i * 2 + k] = v;
                }
            free(raw);
            if (sr != 44100) { float *r = resample(out, n, sr, frames); free(out); out = r; }
            break;
        } else fseek(f, len + (len & 1), SEEK_CUR);
    }
    fclose(f); return out;
}

static void put32(FILE *f, uint32_t v) { uint8_t b[4] = {v, v >> 8, v >> 16, v >> 24}; fwrite(b, 1, 4, f); }
static void put16(FILE *f, uint16_t v) { uint8_t b[2] = {v, v >> 8}; fwrite(b, 1, 2, f); }
// ---------------- FFT (radix-2, complex, in place) ----------------
static float twr[NFFT / 2], twi[NFFT / 2], win[NFFT];
static void fft_init(void) {
    for (int i = 0; i < NFFT / 2; i++) { twr[i] = cosf(2 * M_PI * i / NFFT); twi[i] = -sinf(2 * M_PI * i / NFFT); }
    for (int i = 0; i < NFFT; i++) win[i] = 0.5f - 0.5f * cosf(2 * M_PI * i / NFFT);  // periodic Hann
}
static void fft(float *re, float *im, int inv) {
    for (int i = 1, j = 0; i < NFFT; i++) {
        int b = NFFT >> 1; for (; j & b; b >>= 1) j ^= b; j |= b;
        if (i < j) { float t = re[i]; re[i] = re[j]; re[j] = t; t = im[i]; im[i] = im[j]; im[j] = t; }
    }
    for (int len = 2; len <= NFFT; len <<= 1)
        for (int i = 0; i < NFFT; i += len)
            for (int k = 0; k < len / 2; k++) {
                float wr = twr[k * (NFFT / len)], wi = inv ? -twi[k * (NFFT / len)] : twi[k * (NFFT / len)];
                float *a = re + i + k, *b = im + i + k, *c = re + i + k + len / 2, *d = im + i + k + len / 2;
                float xr = *c * wr - *d * wi, xi = *c * wi + *d * wr;
                *c = *a - xr; *d = *b - xi; *a += xr; *b += xi;
            }
}

// ---------------- U-Net ----------------
typedef struct { const float *k, *b, *s, *t; int ci, co; } Layer;
typedef struct { Layer enc[6], dec[6]; const float *fk, *fb; } Net;

static const float *take(const float **p, size_t n) { const float *r = *p; *p += n; return r; }
static const float *load_weights(const char *path, Net *net) {  // "SPH4" + float16 little-endian
    FILE *f = fopen(path, "rb"); if (!f) die("cannot open weights");
    char magic[4]; if (fread(magic, 1, 4, f) != 4 || memcmp(magic, "SPH4", 4)) die("weights file is not SPH4");
    fseek(f, 0, SEEK_END); long n = (ftell(f) - 4) / 2; fseek(f, 4, SEEK_SET);
    float *buf = malloc(n * sizeof(float)); __fp16 chunk[65536];
    if (!buf) die("out of memory");
    for (long i = 0; i < n;) {
        long k = n - i < 65536 ? n - i : 65536;
        if (fread(chunk, 2, k, f) != (size_t)k) die("cannot read weights");
        for (long j = 0; j < k; j++) buf[i + j] = chunk[j];
        i += k;
    }
    fclose(f);
    long sz = n * 4;
    static const int ch[7] = {2, 16, 32, 64, 128, 256, 512};
    const float *p = buf;
    for (int s = 0; s < NSTEM; s++) {
        for (int l = 0; l < 6; l++) {
            Layer *L = &net[s].enc[l]; L->ci = ch[l]; L->co = ch[l + 1];
            L->k = take(&p, 25 * L->ci * L->co); L->b = take(&p, L->co); L->s = take(&p, L->co); L->t = take(&p, L->co);
        }
        for (int l = 0; l < 6; l++) {  // dec l: in = 512 (l=0) or 2*ch[6-l]; out = ch[5-l] (1 for the last)
            Layer *L = &net[s].dec[l]; L->ci = l == 0 ? 512 : 2 * ch[6 - l]; L->co = l == 5 ? 1 : ch[5 - l];
            L->k = take(&p, 25 * L->ci * L->co); L->b = take(&p, L->co); L->s = take(&p, L->co); L->t = take(&p, L->co);
        }
        net[s].fk = take(&p, 32); net[s].fb = take(&p, 2);
    }
    if ((size_t)(p - buf) * 4 != (size_t)sz) die("weights file has the wrong size");
    return buf;
}

static inline float elu(float x) { return x > 0 ? x : expm1f(x); }

// TF Conv2D 5x5 stride 2 'same' (pad 1 before, 2 after). in [H][W][ci] -> out [H/2][W/2][co], raw (+bias).
static void conv_s2(const float *in, int H, int W, const Layer *L, float *out, float *col, int colstride) {
    int Ho = H / 2, Wo = W / 2, K = 25 * L->ci, ci = L->ci;
    for (int y = 0; y < Ho; y++)
        for (int x = 0; x < Wo; x++) {
            float *c = col + ((size_t)y * Wo + x) * colstride;
            for (int ky = 0; ky < 5; ky++) {
                int iy = 2 * y + ky - 1;
                for (int kx = 0; kx < 5; kx++, c += ci) {
                    int ix = 2 * x + kx - 1;
                    if (iy < 0 || iy >= H || ix < 0 || ix >= W) memset(c, 0, ci * 4);
                    else memcpy(c, in + ((size_t)iy * W + ix) * ci, ci * 4);
                }
            }
        }
    for (int i = 0; i < Ho * Wo; i++) memcpy(out + (size_t)i * L->co, L->b, L->co * 4);
    cblas_sgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans, Ho * Wo, L->co, K, 1, col, colstride, L->k, L->co, 1, out, L->co);
}

// TF Conv2DTranspose 5x5 stride 2 'same': out[2i+k-1] += in[i]*w[k]. in [H][W][ci] -> out [2H][2W][co] (+bias).
// Kernel layout [5][5][co][ci] = rows (ky,kx,co), cols ci -> cols = in * K^T. Done in row bands to bound memory.
static void convT_s2(const float *in, int H, int W, const Layer *L, float *out, float *col) {
    int Ho = 2 * H, Wo = 2 * W, co = L->co, N = 25 * co;
    for (int i = 0; i < Ho * Wo; i++) memcpy(out + (size_t)i * co, L->b, co * 4);
    int band = (int)((size_t)(1 << 21) / ((size_t)W * N)); if (band < 1) band = 1;  // ~8 MB of col per band
    for (int y0 = 0; y0 < H; y0 += band) {
        int nb = H - y0 < band ? H - y0 : band;
        cblas_sgemm(CblasRowMajor, CblasNoTrans, CblasTrans, nb * W, N, L->ci, 1, in + (size_t)y0 * W * L->ci, L->ci, L->k, L->ci, 0, col, N);
        for (int y = 0; y < nb; y++)
            for (int x = 0; x < W; x++) {
                const float *c = col + ((size_t)y * W + x) * N;
                for (int ky = 0; ky < 5; ky++) {
                    int oy = 2 * (y0 + y) + ky - 1;
                    for (int kx = 0; kx < 5; kx++, c += co) {
                        int ox = 2 * x + kx - 1;
                        if (oy < 0 || oy >= Ho || ox < 0 || ox >= Wo) continue;
                        float *o = out + ((size_t)oy * Wo + ox) * co;
                        for (int j = 0; j < co; j++) o[j] += c[j];
                    }
                }
            }
    }
}

// One stem: mag [T][F][2] -> est [T][F][2] (sigmoid mask * mag). Scratch buffers are reused across calls.
static float *skip[6], *act, *dec, *col, *catb;
static void unet(const Net *n, const float *mag, float *est) {
    static const int ch[7] = {2, 16, 32, 64, 128, 256, 512};
    const float *x = mag; int H = T, W = F;
    for (int l = 0; l < 6; l++) {  // rel = ELU(BN(conv)); skips keep the raw conv output
        const Layer *L = &n->enc[l];
        conv_s2(x, H, W, L, skip[l], col, 25 * L->ci);
        H /= 2; W /= 2;
        if (l < 5) {
            size_t m = (size_t)H * W * L->co;
            for (size_t i = 0; i < m; i++) { int c = i % L->co; act[i] = elu(skip[l][i] * L->s[c] + L->t[c]); }
            x = act;
        }
    }
    const float *prev = skip[5];  // up1 takes the raw conv6
    for (int l = 0; l < 6; l++) {  // up = BN(ELU(convT(prev))); merge = concat(skip, up)
        const Layer *L = &n->dec[l];
        convT_s2(prev, H, W, L, dec, col);
        H *= 2; W *= 2; size_t m = (size_t)H * W; int co = L->co;
        if (l < 5) {
            int cs = ch[5 - l];
            for (size_t i = 0; i < m; i++) {
                memcpy(catb + i * (cs + co), skip[4 - l] + i * cs, cs * 4);
                for (int c = 0; c < co; c++) catb[i * (cs + co) + cs + c] = elu(dec[i * co + c]) * L->s[c] + L->t[c];
            }
            prev = catb;
        } else
            for (size_t i = 0; i < m; i++) dec[i] = elu(dec[i]) * L->s[0] + L->t[0];
    }
    // final Conv2D 4x4 dilation 2 'same' (pad 3/3), 1 -> 2 channels, sigmoid, times input magnitude
    for (int y = 0; y < T; y++)
        for (int x = 0; x < F; x++)
            for (int c = 0; c < 2; c++) {
                float a = n->fb[c];
                for (int ky = 0; ky < 4; ky++) {
                    int iy = y + 2 * ky - 3; if (iy < 0 || iy >= T) continue;
                    for (int kx = 0; kx < 4; kx++) {
                        int ix = x + 2 * kx - 3; if (ix < 0 || ix >= F) continue;
                        a += dec[(size_t)iy * F + ix] * n->fk[(ky * 4 + kx) * 2 + c];
                    }
                }
                size_t i = ((size_t)y * F + x) * 2 + c;
                est[i] = mag[i] / (1 + expf(-a));
            }
}

static FILE *open_wav(const char *path, long n) {
    FILE *f = fopen(path, "wb"); if (!f) return NULL;
    uint32_t data = (uint32_t)n * 4;
    fwrite("RIFF", 1, 4, f); put32(f, 36 + data); fwrite("WAVEfmt ", 1, 8, f);
    put32(f, 16); put16(f, 1); put16(f, 2); put32(f, 44100); put32(f, 44100 * 4); put16(f, 4); put16(f, 16);
    fwrite("data", 1, 4, f); put32(f, data);
    return f;
}
static uint32_t rng = 22222;
static void put_pcm(FILE *f, const float *x, long frames) {  // 16-bit with TPDF dither
    for (long i = 0; i < frames * 2; i++) {
        rng = rng * 1664525u + 1013904223u; float d = (rng >> 8) / 16777216.f;
        rng = rng * 1664525u + 1013904223u; d -= (rng >> 8) / 16777216.f;
        long q = lrintf(x[i] * 32767.f + d);
        put16(f, (uint16_t)(int16_t)(q > 32767 ? 32767 : q < -32768 ? -32768 : q));
    }
}

int main(int argc, char **argv) {
    prctl(PR_SET_PDEATHSIG, SIGTERM);   /* the app died or restarted mid-job: stop, don't keep writing */
    if (getppid() == 1) return 1;
    if (argc < 4) { fprintf(stderr, "usage: hs-stems WEIGHTS IN.wav OUTDIR [PREFIX [PICK]]\n"); return 2; }
    // outputs: each one a set of stems (bit s = STEM[s]) mixed into one file
    const char *pick = argc > 5 ? argv[5] : "VDBO";
    int bits[NSTEM], nout = 0, summed = pick[0] && pick[strlen(pick) - 1] == '+';
    for (const char *q = pick; *q && *q != '+'; q++) {
        const char *hit = strchr("VDBO", *q);
        if (!hit || nout == NSTEM) die("PICK is letters from VDBO, optionally ending in +");
        bits[nout++] = 1 << (hit - "VDBO");
    }
    if (!nout) die("PICK names no stem");
    if (summed) { for (int o = 1; o < nout; o++) bits[0] |= bits[o]; nout = 1; }
    // Lowest CPU class so the app's audio threads always win; OpenBLAS threads inherit it.
    struct sched_param sp = {0};
    if (!getenv("HS_STEMS_NORMAL") && sched_setscheduler(0, SCHED_IDLE, &sp)) perror("hs-stems: SCHED_IDLE");
    const char *prefix = argc > 4 ? argv[4] : "stem";
    Net net[NSTEM]; float *wbuf = (float *)load_weights(argv[1], net);
    long n; size_t al = strlen(argv[2]);
    float *x = al > 4 && !strcasecmp(argv[2] + al - 4, ".mp3") ? read_mp3(argv[2], &n) : read_wav(argv[2], &n);
    if (n < 1) die("empty input");
    fft_init();

    // Spleeter framing: NFFT zeros prepended, pad_end -> ceil(len/HOP) frames, T-frame segments.
    long nfr = (n + NFFT + HOP - 1) / HOP, nseg = (nfr + T - 1) / T;
    size_t big = (size_t)T * F * 2;
    float *spr = xcalloc((size_t)T * NBIN * 2, 4), *spi = xcalloc((size_t)T * NBIN * 2, 4), *mag = xcalloc(big, 4), *est[NSTEM], *y[NSTEM];
    long ylen = (long)T * HOP + NFFT;  // output ring: one segment plus the overlap tail
    FILE *out[NSTEM]; char path[NSTEM][4096], part[NSTEM][4200];
    for (int s = 0; s < NSTEM; s++) est[s] = xcalloc(big, 4);
    for (int o = 0; o < nout; o++) {
        y[o] = xcalloc((size_t)ylen * 2, 4);
        char name[64] = "";
        for (int s = 0; s < NSTEM; s++)
            if (bits[o] >> s & 1) snprintf(name + strlen(name), sizeof name - strlen(name), "%s%s", *name ? "+" : "", STEM[s]);
        snprintf(path[o], sizeof path[o], "%s/%s-%s.wav", argv[3], prefix, name);
        snprintf(part[o], sizeof part[o], "%.4000s.part", path[o]);
        if (!(out[o] = open_wav(part[o], n))) { fprintf(stderr, "hs-stems: cannot write %s: %s\n", part[o], strerror(errno)); return 1; }
    }
    for (int l = 0, H = T / 2, W = F / 2, c = 16; l < 6; l++, H /= 2, W /= 2, c *= 2) skip[l] = xcalloc((size_t)H * W * c, 4);
    act = xcalloc(big * 2, 4); dec = xcalloc(big * 2, 4); catb = xcalloc(big * 4, 4);
    col = xcalloc((size_t)(T / 4) * (F / 4) * 400, 4);  // largest im2col (encoder layer 2)
    float re[NFFT], im[NFFT]; long done = 0;  // samples of the padded timeline already written

    for (long g = 0; g < nseg; g++) {
        int nt = nfr - g * T < T ? (int)(nfr - g * T) : T;
        memset(mag, 0, big * 4);
        for (int t = 0; t < nt; t++)
            for (int c = 0; c < 2; c++) {
                long fr = g * T + t;
                for (int i = 0; i < NFFT; i++) {
                    long s = fr * HOP + i - NFFT;
                    re[i] = (s >= 0 && s < n ? x[s * 2 + c] : 0) * win[i]; im[i] = 0;
                }
                fft(re, im, 0);
                for (int b = 0; b < NBIN; b++) {
                    size_t k = ((size_t)t * NBIN + b) * 2 + c; spr[k] = re[b]; spi[k] = im[b];
                    if (b < F) mag[((size_t)t * F + b) * 2 + c] = hypotf(re[b], im[b]);
                }
            }
        for (int s = 0; s < NSTEM; s++) { unet(&net[s], mag, est[s]); printf("progress %ld/%ld\n", g * NSTEM + s + 1, nseg * NSTEM); fflush(stdout); }
        for (int s = 0; s < nout; s++) {  // s: output
            for (int t = 0; t < nt; t++)
                for (int c = 0; c < 2; c++) {
                    for (int b = 0; b < NBIN; b++) {
                        float m = 0;
                        if (b < F) {
                            size_t i = ((size_t)t * F + b) * 2 + c; float sum = 1e-10f;
                            for (int k = 0; k < NSTEM; k++) sum += est[k][i] * est[k][i];
                            for (int k = 0; k < NSTEM; k++)
                                if (bits[s] >> k & 1) m += (est[k][i] * est[k][i] + 1e-10f / NSTEM) / sum;
                        }
                        size_t k = ((size_t)t * NBIN + b) * 2 + c; re[b] = spr[k] * m; im[b] = spi[k] * m;
                    }
                    im[0] = im[NFFT / 2] = 0;
                    for (int b = 1; b < NFFT / 2; b++) { re[NFFT - b] = re[b]; im[NFFT - b] = -im[b]; }
                    fft(re, im, 1);
                    for (int i = 0; i < NFFT; i++) y[s][((long)t * HOP + i) * 2 + c] += re[i] * (2.f / 3.f / NFFT) * win[i];
                }
            // samples [g*T*HOP, (g+1)*T*HOP) are final; write the part inside [NFFT, NFFT+n)
            long base = g * T * HOP, end = base + (long)T * HOP, lo = base > NFFT ? base : NFFT, hi = end < NFFT + n ? end : NFFT + n;
            if (hi > lo) put_pcm(out[s], y[s] + (lo - base) * 2, hi - lo);
            memmove(y[s], y[s] + (size_t)T * HOP * 2, (size_t)NFFT * 2 * 4);
            memset(y[s] + (size_t)NFFT * 2, 0, (size_t)T * HOP * 2 * 4);
            if (s == nout - 1) done = hi;
        }
    }
    if (done != NFFT + n) die("internal: short write");
    for (int s = 0; s < nout; s++) {
        if (ferror(out[s]) | fclose(out[s]) || rename(part[s], path[s])) { fprintf(stderr, "hs-stems: cannot write %s\n", path[s]); return 1; }
        printf("wrote %s\n", path[s]);
    }
    free(wbuf);
    return 0;
}
