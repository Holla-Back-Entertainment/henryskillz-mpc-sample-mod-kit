#include <stddef.h>
/* 0 = found: root 0..11 (C..B), scale index, or scale -1 = a one-shot's note. -1 = no answer. */
int browser_detect_key(const float *mono, long n, int sample_rate, float bpm, int *root, int *scale);
void browser_key_text(int root, int scale, char *out, size_t size);
int browser_is_drums(const char *path);
float *browser_decode(const char *path, unsigned *count, unsigned *sample_rate);
