#include <stdint.h>
/* Called on every browser preview with the file and the tempo the preview plays it at. */
void browser_tag_audition(const char *path, float bpm);
/* Pure screen parts, also built on the Mac for the checks and the preview render. */
int browser_tag_is_browser(const uint32_t *px, int stride);
uint32_t browser_tag_row(const uint32_t *px, int stride);
int browser_tag_paint(uint32_t *px, int stride, const char *key, float bpm);
