// chains_toast.c: the box hs_chains.c shows when a chain loads, saves or is skipped
#include <stdint.h>
__attribute__((visibility("hidden"))) void chains_toast_paint(uint32_t *px, int stride, const char *title, const char *detail);  // pure, host-checked
__attribute__((visibility("hidden"))) void chains_toast(const char *title, const char *detail);  // up for 1.5 s on the next frames
