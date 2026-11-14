/* Key of a loop for the sample browser tag. Port of ~/Projects/sample-chords/detect.c (stage 4,
 * 11/11 synthetic keys, real loops checked against their labels): whitened STFT -> tuning-corrected
 * chroma -> one chord per half bar -> 12 roots x 9 scales ranked by note set + home note.
 * Project/song-key, pads and chord printing are left out: the browser only shows the key. */
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "key_detect.h"

static const char *const NOTES[12] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};

typedef struct { const char *tag; int n; int iv[7]; } Scale;
static const Scale SCALES[] = {
    {"MAJ", 7, {0,2,4,5,7,9,11}}, {"MIN", 7, {0,2,3,5,7,8,10}},
    {"DORIAN", 7, {0,2,3,5,7,9,10}}, {"PHRYGIAN", 7, {0,1,3,5,7,8,10}},
    {"LYDIAN", 7, {0,2,4,6,7,9,11}}, {"MIXO", 7, {0,2,4,5,7,9,10}},
    {"HARM MIN", 7, {0,2,3,5,7,8,11}}, {"MEL MIN", 7, {0,2,3,5,7,9,11}},
    {"PHRYG DOM", 7, {0,1,4,5,7,8,10}},
};
#define NSCALES (int)(sizeof SCALES / sizeof *SCALES)
#define MODE_PRIOR 0.12   /* plain Major/Minor only; modes need a clear margin */

typedef struct { const char *suf; int n; int iv[4]; } Chord;
static const Chord CHORDS[] = {
    {"", 3, {0,4,7}}, {"m", 3, {0,3,7}}, {"7", 4, {0,4,7,10}}, {"maj7", 4, {0,4,7,11}},
    {"m7", 4, {0,3,7,10}}, {"dim", 3, {0,3,6}}, {"sus4", 3, {0,5,7}},
};
#define NCHORDS (int)(sizeof CHORDS / sizeof *CHORDS)

static void fft_mag(const float *x, int n, float *re, float *im, float *mag) {
    for (int i = 0, j = 0; i < n; i++) {
        re[j] = x[i]; im[j] = 0;
        int bit = n >> 1;
        if (i < n - 1) { while (j & bit) { j ^= bit; bit >>= 1; } j |= bit; }
    }
    for (int len = 2; len <= n; len <<= 1) {
        double a = -2 * M_PI / len; float wr = cos(a), wi = sin(a);
        for (int s = 0; s < n; s += len) {
            float cr = 1, ci = 0;
            for (int k = 0; k < len / 2; k++) {
                int p = s + k, q = p + len / 2;
                float tr = re[q] * cr - im[q] * ci, ti = re[q] * ci + im[q] * cr;
                re[q] = re[p] - tr; im[q] = im[p] - ti; re[p] += tr; im[p] += ti;
                float t = cr * wr - ci * wi; ci = cr * wi + ci * wr; cr = t;
            }
        }
    }
    for (int k = 0; k <= n / 2; k++) mag[k] = sqrtf(re[k] * re[k] + im[k] * im[k]);
}

/* Chroma only needs up to ~1 kHz: low-pass at 2.2 kHz, keep every Dth sample. */
static float *decimate(const float *y, long n, int sr, int *sr_out, long *n_out) {
    int D = sr / 5000; if (D < 1) D = 1;
    enum { TAPS = 127 };
    float h[TAPS]; double fc = 2200.0 / sr, sum = 0;
    for (int i = 0; i < TAPS; i++) {
        double m = i - (TAPS - 1) / 2.0;
        double s = m == 0 ? 2 * fc : sin(2 * M_PI * fc * m) / (M_PI * m);
        h[i] = s * (0.42 - 0.5 * cos(2 * M_PI * i / (TAPS - 1)) + 0.08 * cos(4 * M_PI * i / (TAPS - 1)));
        sum += h[i];
    }
    for (int i = 0; i < TAPS; i++) h[i] /= sum;
    long m = n / D;
    float *out = malloc(sizeof(float) * (m ? m : 1));
    if (!out) return NULL;
    for (long o = 0; o < m; o++) {
        long c = o * D; double acc = 0;
        for (int i = 0; i < TAPS; i++) {
            long t = c + i - TAPS / 2;
            if (t >= 0 && t < n) acc += h[i] * y[t];
        }
        out[o] = acc;
    }
    *sr_out = sr / D; *n_out = m;
    return out;
}

typedef struct { int sr, nfft, hop, bins, frames; float *S; } Spec;   /* S: frames x bins */

static int spectra(const float *y, long n, int sr, Spec *sp) {
    sp->sr = sr; sp->nfft = 1 << (int)nearbyint(log2(0.3715 * sr)); sp->hop = sp->nfft / 4;
    sp->bins = sp->nfft / 2 + 1;
    long len = n < sp->nfft ? sp->nfft : n;
    sp->frames = 1 + (len - sp->nfft) / sp->hop;
    sp->S = malloc(sizeof(float) * sp->frames * sp->bins);
    float *w = malloc(sizeof(float) * sp->nfft), *buf = malloc(sizeof(float) * sp->nfft);
    float *re = malloc(sizeof(float) * sp->nfft), *im = malloc(sizeof(float) * sp->nfft);
    float *mag = malloc(sizeof(float) * sp->bins);
    int rc = -1;
    if (!sp->S || !w || !buf || !re || !im || !mag) goto out;
    for (int i = 0; i < sp->nfft; i++) w[i] = 0.5 - 0.5 * cos(2 * M_PI * i / (sp->nfft - 1));
    for (int i = 0; i < sp->frames; i++) {
        for (int j = 0; j < sp->nfft; j++) {
            long t = (long)i * sp->hop + j;
            buf[j] = (t < n ? y[t] : 0) * w[j];
        }
        fft_mag(buf, sp->nfft, re, im, mag);
        float *row = sp->S + (long)i * sp->bins; double run = 0;   /* whiten: minus a 31-bin mean */
        for (int b = 0; b < 15 && b < sp->bins; b++) run += mag[b];
        for (int b = 0; b < sp->bins; b++) {
            if (b + 15 < sp->bins) run += mag[b + 15];
            if (b - 16 >= 0) run -= mag[b - 16];
            float v = mag[b] - run / 31;
            row[b] = v > 0 ? v : 0;
        }
    }
    rc = 0;
out:
    free(w); free(buf); free(re); free(im); free(mag);
    if (rc) { free(sp->S); sp->S = NULL; }
    return rc;
}

static double freq(const Spec *sp, int b) { return (double)b * sp->sr / sp->nfft; }

static double tuning(const Spec *sp) {
    double sr_ = 0, si = 0;
    for (int b = 1; b < sp->bins; b++) {
        double f = freq(sp, b);
        if (f < 80 || f > 1000) continue;
        double s = 0;
        for (int i = 0; i < sp->frames; i++) s += sp->S[(long)i * sp->bins + b];
        double fr = fmod(12 * log2(f / 440.0), 1.0); if (fr < 0) fr += 1;
        sr_ += s * s * cos(2 * M_PI * fr); si += s * s * sin(2 * M_PI * fr);
    }
    return atan2(si, sr_) / (2 * M_PI);
}

static float *chroma(const Spec *sp, double fmin, double fmax, double tune) {
    float *out = calloc((size_t)sp->frames * 12, sizeof(float));
    if (!out) return NULL;
    for (int b = 1; b < sp->bins; b++) {
        double f = freq(sp, b);
        if (f < fmin || f > fmax) continue;
        double semis = 12 * log2(f / 440.0) + 9 - tune, r = nearbyint(semis);
        double tri = 1 - 2 * fabs(semis - r); if (tri <= 0) continue;
        int pc = ((int)r % 12 + 12) % 12;
        for (int i = 0; i < sp->frames; i++) out[i * 12 + pc] += sp->S[(long)i * sp->bins + b] * tri;
    }
    return out;
}

static void normv(const double *v, double *o) {
    double s = 0; for (int i = 0; i < 12; i++) s += v[i] * v[i];
    s = sqrt(s);
    for (int i = 0; i < 12; i++) o[i] = s > 1e-9 ? v[i] / s : v[i];
}

typedef struct { int root, chord; double score; } Hit;

static Hit score_chord(const double *c, const double *bass) {
    double mx = 0, cn[12], nc[12];
    for (int i = 0; i < 12; i++) if (c[i] > mx) mx = c[i];
    for (int i = 0; i < 12; i++) cn[i] = c[i] / (mx > 1e-9 ? mx : 1e-9);
    normv(c, nc);
    Hit best = {-1, 0, -1e300};
    for (int r = 0; r < 12; r++)
        for (int k = 0; k < NCHORDS; k++) {
            const Chord *ch = &CHORDS[k];
            if (ch->n == 4 && cn[(r + ch->iv[3]) % 12] < 0.35) continue;
            if (k == 6 && fmax(cn[(r + 3) % 12], cn[(r + 4) % 12]) > 0.3) continue;
            double dot = 0;
            for (int i = 0; i < ch->n; i++) dot += nc[(r + ch->iv[i]) % 12];
            double s = dot / sqrt(ch->n) + 0.15 * bass[r] - 0.02 * (ch->n - 3);
            if (s > best.score) { best.root = r; best.chord = k; best.score = s; }
        }
    return best;
}

typedef struct { double s; int r, k; } Key;

/* best key for note set w (sums to 1), chord-root share, bass chroma, first/last chord */
static Key best_key(const double *w, const double *roots, const double *bn, const Hit *first, const Hit *last) {
    Key best = {-1e300, 0, 0};
    for (int r = 0; r < 12; r++)
        for (int k = 0; k < NSCALES; k++) {
            const Scale *sc = &SCALES[k]; double in = 0; int minor_scale = 0;
            for (int i = 0; i < sc->n; i++) { in += w[(r + sc->iv[i]) % 12]; if (sc->iv[i] == 3) minor_scale = 1; }
            double home = 0;
            for (int e = 0; e < 2; e++) {
                const Hit *h = e ? last : first; double wt = e ? 0.15 : 0.3;
                if (!h || h->root != r) continue;
                const char *suf = CHORDS[h->chord].suf;
                if (!strncmp(suf, "sus", 3)) { home += wt / 2; continue; }
                int q_minor = (suf[0] == 'm' && strncmp(suf, "maj", 3)) || !strcmp(suf, "dim");
                home += q_minor == minor_scale ? wt : wt / 4;
            }
            double s = -3.0 * (1 - in) + home + 0.8 * roots[r] + 0.2 * bn[r] + (k < 2 ? MODE_PRIOR : 0);
            /* ties: same order as detect.c's sort (higher root, then later scale name, wins) */
            if (s > best.s || (s == best.s && (r > best.r || (r == best.r && strcmp(SCALES[k].tag, SCALES[best.k].tag) > 0))))
                best = (Key){s, r, k};
        }
    return best;
}

int browser_detect_key(const float *y, long n, int sr, float bpm, int *root, int *scale) {
    if (!y || n < sr / 8 || sr < 8000) return -1;   /* 125 ms: short one-shots still read their note */
    if (!(bpm >= 40 && bpm <= 300)) bpm = 120;   /* one-shot / no clear beat: the key still reads */
    Spec sp = {0};
    int dsr; long dn;
    float *yd = decimate(y, n, sr, &dsr, &dn), *full = NULL, *low = NULL;
    Hit *seq = NULL;
    int rc = -1;
    if (!yd || spectra(yd, dn, dsr, &sp)) goto out;
    double tune = tuning(&sp);
    full = chroma(&sp, 80, 1000, tune); low = chroma(&sp, 40, 200, tune);
    if (!full || !low) goto out;

    double ctot[12] = {0}, btot[12] = {0};
    for (int i = 0; i < sp.frames; i++) for (int p = 0; p < 12; p++) { ctot[p] += full[i * 12 + p]; btot[p] += low[i * 12 + p]; }
    if (n < 3L * sr) {   /* one-shot: its note, not a key */
        int note = 0;
        for (int p = 1; p < 12; p++) if (ctot[p] > ctot[note]) note = p;
        *root = note; *scale = -1; rc = 0;
        goto out;
    }

    double bar_frames = 4 * 60 / bpm * sp.sr / sp.hop;
    int n_bars = (int)nearbyint(sp.frames / bar_frames); if (n_bars < 1) n_bars = 1;
    seq = malloc(sizeof(Hit) * 2 * n_bars);
    if (!seq) goto out;
    int nseq = 0;
    for (int b = 0; b < n_bars; b++)
        for (int h = 0; h < 2; h++) {
            int a = (int)((b + h / 2.0) * bar_frames), z = (int)((b + (h + 1) / 2.0) * bar_frames);
            if (a >= sp.frames) break;
            if (z > sp.frames) z = sp.frames;
            double c[12] = {0}, lo[12] = {0}, lon[12], tot = 0;
            for (int i = a; i < z; i++) for (int p = 0; p < 12; p++) { c[p] += full[i * 12 + p]; lo[p] += low[i * 12 + p]; }
            for (int p = 0; p < 12; p++) tot += c[p];
            if (tot < 1e-6) continue;
            normv(lo, lon);
            Hit hit = score_chord(c, lon);
            if (hit.root >= 0) seq[nseq++] = hit;
        }

    double cn[12], bn[12], w[12], roots[12] = {0}, wsum = 0, nsum = 0;
    normv(ctot, cn); normv(btot, bn);
    for (int p = 0; p < 12; p++) nsum += cn[p];
    if (nsum < 1e-9) goto out;   /* silence */
    for (int p = 0; p < 12; p++) w[p] = cn[p] / nsum;
    for (int i = 0; i < nseq; i++) {
        const Chord *ch = &CHORDS[seq[i].chord];
        for (int t = 0; t < ch->n; t++) w[(seq[i].root + ch->iv[t]) % 12] += 1.0 / nseq;
        roots[seq[i].root] += 1.0 / nseq;
    }
    for (int p = 0; p < 12; p++) wsum += w[p];
    for (int p = 0; p < 12; p++) w[p] /= wsum;
    Key k = best_key(w, roots, bn, nseq ? &seq[0] : NULL, nseq ? &seq[nseq - 1] : NULL);
    *root = k.r; *scale = k.k; rc = 0;
out:
    free(yd); free(sp.S); free(full); free(low); free(seq);
    return rc;
}

/* "C# MIN", "F MAJ", or "NOTE E" for a one-shot */
void browser_key_text(int root, int scale, char *out, size_t size) {
    if (root < 0 || root > 11) { if (size) *out = 0; return; }
    if (scale < 0 || scale >= NSCALES) { snprintf(out, size, "NOTE %s", NOTES[root]); return; }
    snprintf(out, size, "%s %s", NOTES[root], SCALES[scale].tag);
}

/* Drum loops read as some key anyway; their names say what they are (whole words only:
 * "Breakfast" is not "break"). ponytail: filename words, no audio drum classifier. */
int browser_is_drums(const char *path) {
    static const char *const DRUMS[] = {"kick","snare","hat","hats","hihat","clap","perc","drum","drums","cymbal",
                                        "crash","ride","shaker","rim","tom","toms","break","breaks","kit","hh","sd","bd"};
    const char *q = strrchr(path, '/'); q = q ? q + 1 : path;
    while (*q) {
        while (*q && !isalpha((unsigned char)*q)) q++;
        char word[16]; int k = 0;
        while (*q && isalpha((unsigned char)*q)) { if (k < 15) word[k++] = tolower((unsigned char)*q); q++; }
        word[k] = 0;
        for (size_t i = 0; k && i < sizeof DRUMS / sizeof *DRUMS; i++) if (!strcmp(word, DRUMS[i])) return 1;
    }
    return 0;
}
