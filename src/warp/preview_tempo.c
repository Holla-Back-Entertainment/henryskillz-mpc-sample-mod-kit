#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#define MINIMP3_IMPLEMENTATION
#define MINIMP3_FLOAT_OUTPUT
#define MINIMP3_NO_STDIO
#include "minimp3.h"

#ifdef WARP_DIAG
#include <stdarg.h>
void bwlog(const char *fmt, ...) {
    FILE *f = fopen("/tmp/browser-warp.log", "a");
    if (!f) return;
    va_list ap; va_start(ap, fmt); vfprintf(f, fmt, ap); va_end(ap);
    fputc('\n', f); fclose(f);
}
#define BWLOG(...) bwlog(__VA_ARGS__)
#else
#define BWLOG(...) ((void)0)
#endif

/* Tempo from a spectral-flux onset envelope + autocorrelation with a tempo prior.
 * This locks the beat of dense full mixes (songs/samples), not just clean loops. */
enum {
    envelope_rate = 200, max_seconds = 24, max_bins = envelope_rate * max_seconds,
    fft_n = 1024, fft_hop = 512, spec_bins = fft_n / 2 + 1,
    min_lag = 63, max_lag = 172,          /* 200 Hz grid -> ~70..190 BPM */
};
#define MP3_READ_CAP (2u << 20)            /* after the ID3 tag: 24 s even at 640 kbps free format */
#define ACCEPT 0.16f                      /* raw autocorr at the prior-picked lag */

static uint32_t le32(const unsigned char *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static unsigned le16(const unsigned char *p) { return p[0] | (unsigned)p[1] << 8; }

static float decode(const unsigned char *p, unsigned format, unsigned bytes) {
    if (format == 3) {
        uint32_t raw = le32(p); float v; memcpy(&v, &raw, sizeof v);
        return isfinite(v) ? fmaxf(-1, fminf(1, v)) : 0;
    }
    if (bytes == 1) return ((int)p[0] - 128) / 128.0f;
    uint32_t raw = 0;
    for (unsigned i = 0; i < bytes; ++i) raw |= (uint32_t)p[i] << (8 * i);
    int32_t v = (int32_t)(raw << (32 - 8 * bytes));
    return v / 2147483648.0f;
}

/* Reuse the same rotation coefficients for every frame of a sample. */
static void make_twiddles(float *real, float *imag) {
    for (unsigned len = 2; len <= fft_n; len <<= 1) {
        unsigned half = len / 2;
        float ang = -2.0f * (float)M_PI / (float)len;
        float wr = cosf(ang), wi = sinf(ang), cr = 1, ci = 0;
        for (unsigned k = 0; k < half; ++k) {
            real[half + k] = cr;
            imag[half + k] = ci;
            float next = cr * wr - ci * wi;
            ci = cr * wi + ci * wr;
            cr = next;
        }
    }
}

static void fft(float *re, float *im, const float *real, const float *imag) {
    const unsigned n = fft_n;
    for (unsigned i = 1, j = 0; i < n; ++i) {
        unsigned bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { float t = re[i]; re[i] = re[j]; re[j] = t; t = im[i]; im[i] = im[j]; im[j] = t; }
    }
    for (unsigned len = 2; len <= n; len <<= 1) {
        unsigned half = len / 2;
        for (unsigned i = 0; i < n; i += len) {
            for (unsigned k = 0; k < half; ++k) {
                float cr = real[half + k], ci = imag[half + k];
                float xr = re[i + k + half], xi = im[i + k + half];
                float vr = xr * cr - xi * ci, vi = xr * ci + xi * cr;
                float ur = re[i + k], ui = im[i + k];
                re[i + k] = ur + vr; im[i + k] = ui + vi;
                re[i + k + half] = ur - vr; im[i + k + half] = ui - vi;
            }
        }
    }
}

/* spectral-flux onset envelope of mono PCM, resampled to envelope_rate (200 Hz) */
static unsigned onset_envelope(const float *mono, unsigned n, unsigned sr, float *onsets) {
    if (n < fft_n) return 0;
    unsigned frames = 1 + (n - fft_n) / fft_hop;
    float *flux = calloc(frames, sizeof *flux);
    float *re = malloc(fft_n * sizeof *re), *im = malloc(fft_n * sizeof *im);
    float *win = malloc(fft_n * sizeof *win), *prev = calloc(spec_bins, sizeof *prev);
    float *twiddles = malloc(2 * fft_n * sizeof *twiddles);
    unsigned bins = 0;
    if (!flux || !re || !im || !win || !prev || !twiddles) goto done;
    make_twiddles(twiddles, twiddles + fft_n);
    for (unsigned k = 0; k < fft_n; ++k) win[k] = 0.5f - 0.5f * cosf(2.0f * (float)M_PI * k / fft_n);
    for (unsigned t = 0; t < frames; ++t) {
        const float *seg = mono + (size_t)t * fft_hop;
        for (unsigned k = 0; k < fft_n; ++k) { re[k] = seg[k] * win[k]; im[k] = 0; }
        fft(re, im, twiddles, twiddles + fft_n);
        float f = 0;
        for (unsigned k = 0; k < spec_bins; ++k) {
            float mag = log1pf(sqrtf(re[k] * re[k] + im[k] * im[k]));
            float d = mag - prev[k];
            if (d > 0) f += d;
            prev[k] = mag;
        }
        flux[t] = f;
    }
    /* resample flux (rate sr/hop) to the 200 Hz onset grid by linear interpolation */
    float frate = (float)sr / fft_hop;
    float dur = frames / frate;
    unsigned want = (unsigned)(dur * envelope_rate);
    if (want > (unsigned)max_bins) want = max_bins;
    for (unsigned i = 0; i < want; ++i) {
        float x = i * (frate / envelope_rate);
        unsigned a = (unsigned)x;
        float frac = x - a;
        float v0 = a < frames ? flux[a] : 0;
        float v1 = a + 1 < frames ? flux[a + 1] : v0;
        onsets[i] = v0 + frac * (v1 - v0);
    }
    bins = want;
done:
    free(flux); free(re); free(im); free(win); free(prev); free(twiddles);
    return bins;
}

static float estimate(float *onsets, unsigned bins) {
    if (bins < 3 * envelope_rate) { BWLOG("  est: too few bins %u", bins); return 0; }
    double mean = 0;
    for (unsigned i = 0; i < bins; ++i) mean += onsets[i];
    mean /= bins;
    if (mean < 1e-9) { BWLOG("  est: silent"); return 0; }
    for (unsigned i = 0; i < bins; ++i) onsets[i] -= (float)mean;   /* mean-center */
    float raw[max_lag + 1] = {0};
    for (unsigned L = min_lag; L <= max_lag; ++L) {
        float cross = 0, left = 0, right = 0;
        for (unsigned i = L; i < bins; ++i) {
            float a = onsets[i - L], b = onsets[i];
            cross += a * b; left += a * a; right += b * b;
        }
        raw[L] = (left > 0 && right > 0) ? cross / sqrtf(left * right) : 0;
    }
    /* pick the lag maximising autocorr * log-normal tempo prior (centre 120 BPM) */
    float best_w = -1e9f, conf = 0;
    unsigned lag = 0;
    for (unsigned L = min_lag; L <= max_lag; ++L) {
        float bpm = 60.0f * envelope_rate / L;
        float z = log2f(bpm / 120.0f) / 0.9f;
        float w = raw[L] * expf(-0.5f * z * z);
        if (w > best_w) { best_w = w; lag = L; conf = raw[L]; }
    }
    BWLOG("  est: lag=%u bpm=%.1f conf=%.3f (accept>=%.2f)", lag, lag ? 60.0f * envelope_rate / lag : 0, conf, ACCEPT);
    if (conf < ACCEPT || !lag) { BWLOG("  est: below accept"); return 0; }
    /* sub-bin parabolic interpolation around the chosen lag */
    float off = 0;
    if (lag > min_lag && lag < max_lag) {
        float l = raw[lag - 1], r = raw[lag + 1], d = l - 2 * conf + r;
        if (d < 0) off = 0.5f * (l - r) / d;
    }
    off = fmaxf(-0.5f, fminf(0.5f, off));
    return 60.0f * envelope_rate / (lag + off);
}

static float analyze(const float *mono, unsigned n, unsigned sr) {
    float *onsets = calloc(max_bins, sizeof *onsets);
    if (!onsets) return 0;
    unsigned bins = onset_envelope(mono, n, sr, onsets);
    BWLOG("  bins=%u", bins);
    float r = estimate(onsets, bins);
    free(onsets);
    return r;
}

/* ---- front-ends: decode to a bounded mono float buffer, then analyse ---- */

static float *decode_wav(const char *path, unsigned *count, unsigned *sample_rate) {
    FILE *file = fopen(path, "rb");
    if (!file) { BWLOG("  fopen fail"); return NULL; }
    unsigned char header[40], block[16384];
    unsigned format = 0, channels = 0, rate = 0, align = 0, bytes = 0;
    uint32_t audio_size = 0;
    float *mono = NULL;
    if (fread(header, 1, 12, file) != 12 || memcmp(header, "RIFF", 4) ||
        memcmp(header + 8, "WAVE", 4)) { BWLOG("  not RIFF/WAVE"); goto out; }
    for (unsigned chunk = 0; chunk < 128 && fread(header, 1, 8, file) == 8; ++chunk) {
        uint32_t size = le32(header + 4);
        if (!memcmp(header, "data", 4)) { audio_size = size; break; }
        if (!memcmp(header, "fmt ", 4)) {
            size_t take = size < sizeof header ? size : sizeof header;
            if (take < 16 || fread(header, 1, take, file) != take) goto out;
            format = le16(header); channels = le16(header + 2); rate = le32(header + 4);
            align = le16(header + 12); bytes = (le16(header + 14) + 7) / 8;
            if (format == 65534 && take == 40) format = le16(header + 24);
            if (fseek(file, (long)size - (long)take + (size & 1), SEEK_CUR)) goto out;
        } else if (fseek(file, (long)size + (size & 1), SEEK_CUR)) goto out;
    }
    BWLOG("  wav fmt=%u ch=%u rate=%u bytes=%u", format, channels, rate, bytes);
    if (channels < 1 || channels > 2 || rate < 8000 || rate > 96000 || bytes < 1 || bytes > 4 ||
        align != bytes * channels || (format != 1 && (format != 3 || bytes != 4))) { BWLOG("  wav gate fail"); goto out; }
    unsigned frames = audio_size / align;
    if (frames > (unsigned)max_seconds * rate) frames = max_seconds * rate;
    mono = malloc((size_t)frames * sizeof *mono);
    if (!mono) goto out;
    unsigned got = 0;
    for (unsigned remaining = frames; remaining > 0;) {
        unsigned take = sizeof block / align;
        if (take > remaining) take = remaining;
        if (fread(block, align, take, file) != take) break;
        remaining -= take;
        for (unsigned i = 0; i < take; ++i) {
            float s = 0;
            for (unsigned c = 0; c < channels; ++c) s += decode(block + i * align + c * bytes, format, bytes);
            mono[got++] = s / channels;
        }
    }
    *count = got; *sample_rate = rate;
    fclose(file);
    return mono;
out:
    free(mono); fclose(file);
    return NULL;
}

static float *decode_mp3(const char *path, unsigned *count, unsigned *sample_rate) {
    FILE *file = fopen(path, "rb");
    if (!file) { BWLOG("  mp3 fopen fail"); return NULL; }
    unsigned char *buf = malloc(MP3_READ_CAP);
    float *mono = NULL;
    if (!buf) { fclose(file); return NULL; }
    unsigned char id3[10];   /* skip an ID3v2 tag (cover art can be MBs) so only audio is read */
    if (fread(id3, 1, 10, file) == 10 && !memcmp(id3, "ID3", 3) && !((id3[6] | id3[7] | id3[8] | id3[9]) & 0x80)) {
        long tag = 10 + ((long)id3[6] << 21 | (long)id3[7] << 14 | (long)id3[8] << 7 | id3[9]) + (id3[5] & 0x10 ? 10 : 0);
        fseek(file, tag, SEEK_SET);
    } else rewind(file);
    size_t n = fread(buf, 1, MP3_READ_CAP, file);
    fclose(file);
    mono = malloc((size_t)max_seconds * 48000 * sizeof *mono);   /* >= 24 s at any supported rate */
    if (!mono) { free(buf); return NULL; }
    mp3dec_t dec; mp3dec_init(&dec);
    mp3d_sample_t pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];
    unsigned rate = 0, channels = 0, got = 0, cap = 0;
    size_t pos = 0;
    while (pos < n && (cap == 0 || got < cap)) {
        mp3dec_frame_info_t info;
        int samples = mp3dec_decode_frame(&dec, buf + pos, (int)(n - pos), pcm, &info);
        if (info.frame_bytes <= 0) break;
        pos += (size_t)info.frame_bytes;
        if (samples <= 0) continue;
        if (!rate) {
            rate = (unsigned)info.hz; channels = (unsigned)info.channels;
            BWLOG("  mp3 hz=%u ch=%u", rate, channels);
            if (channels < 1 || channels > 2 || rate < 8000 || rate > 48000) { BWLOG("  mp3 gate fail"); break; }
            cap = max_seconds * rate;
        }
        if ((unsigned)info.hz != rate || (unsigned)info.channels != channels) break;
        for (int i = 0; i < samples && got < cap; ++i) {
            float s = 0;
            for (unsigned c = 0; c < channels; ++c) s += pcm[i * channels + c];
            mono[got++] = s / channels;
        }
    }
    free(buf);
    *count = got; *sample_rate = rate ? rate : 44100;
    return mono;
}

/* First 24 s of a WAV or MP3 as mono floats (caller frees), shared by tempo and key detection. */
float *browser_decode(const char *path, unsigned *count, unsigned *sample_rate) {
    struct stat info;
    if (!path || stat(path, &info) || !S_ISREG(info.st_mode)) { BWLOG("  stat/notreg fail"); return NULL; }
    const char *name = strrchr(path, '/'); name = name ? name + 1 : path;
    size_t len = strlen(name);
    if (len >= 4 && !strcasecmp(name + len - 4, ".mp3")) return decode_mp3(path, count, sample_rate);
    return decode_wav(path, count, sample_rate);
}

float browser_detect_tempo(const char *path) {
    BWLOG("detect: path=%s", path ? path : "(null)");
    unsigned n = 0, sr = 0;
    float *mono = browser_decode(path, &n, &sr);
    if (!mono) return 0;
    float result = analyze(mono, n, sr);
    BWLOG("  -> %.2f bpm", result);
    free(mono);
    return result;
}
