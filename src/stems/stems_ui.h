/* pure parts of hs_stems_ui.c, host-checked in check_ui.c */
#include <stddef.h>
#include <stdint.h>
int stems_is_browser(const uint32_t *px, int stride);
int stems_is_shift_browser(const uint32_t *px, int stride);
uint32_t stems_row(const uint32_t *px, int stride);
int stems_is_wav(const char *path);
int stems_out(const char *path, char *dir, size_t dn, char *base, size_t bn);
void stems_paint_b1(uint32_t *px, int stride, const char *label);
void stems_paint_box(uint32_t *px, int stride, const char *title, int pct, const char *line);
void stems_paint_tabs(uint32_t *px, int stride, const char *b1, const char *b2, const char *b3);
void stems_paint_chooser(uint32_t *px, int stride, const char *title, int ticks, int sel, int summed);
/* stems_pad.c */
int stems_pad_label(const uint32_t *px, int stride, int *track, int *pad);
const char *stems_xpj_sample(const char *json, int track, int pad, char *file, size_t n);
int stems_free_pads(const char *json, int track, int from, int skip, int *out, int max, const char **why);
int stems_pads_note(const int *pads, int placed, int wanted, char *out, size_t n);
int stems_pick(int ticks, int summed, char *pick, size_t n);
int stems_out_name(const char *base, int ticks, int summed, int i, char *out, size_t n);
void stems_paint_b3(uint32_t *px, int stride, const char *label);
void stems_paint_note(uint32_t *px, int stride, const char *line);
