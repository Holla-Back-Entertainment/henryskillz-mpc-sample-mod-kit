/* Host check + preview render for the browser tag, on real lab screenshots (raw 320x240 0x00RRGGBB).
 *   cc -DBROWSER_TAG_HOST -I. check_tag.c browser_tag.c -o /tmp/check-tag
 *   /tmp/check-tag browser.raw other-screen.raw out.raw     (preview-tag.py makes the raws and the png) */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "browser_tag.h"

static uint32_t *load(const char *path) {
    static uint32_t buf[3][320 * 240]; static int k;
    FILE *f = fopen(path, "rb"); assert(f);
    assert(fread(buf[k], 4, 320 * 240, f) == 320 * 240); fclose(f);
    return buf[k++];
}

int main(int argc, char **argv) {
    assert(argc == 4);
    uint32_t *b = load(argv[1]), *other = load(argv[2]);
    assert(browser_tag_is_browser(b, 320) && !browser_tag_is_browser(other, 320));
    uint32_t row = browser_tag_row(b, 320);
    assert(row && !browser_tag_row(other, 320) && !browser_tag_paint(other, 320, "C MAJ", 90));
    static uint32_t moved[320 * 240];                       /* another name in the row: another row */
    memcpy(moved, b, sizeof moved);
    for (int y = 48; y < 62; ++y) memmove(moved + y * 320 + 44, b + y * 320 + 50, 100 * 4);
    assert(browser_tag_row(moved, 320) != row);
    memcpy(moved, b, sizeof moved);                          /* the same name in blue (a loaded file) */
    for (int y = 42; y < 68; ++y) for (int x = 40; x < 176; ++x)
        if ((b[y * 320 + x] & 0xffffff) == 0xffffff) moved[y * 320 + x] = 0x4a9eff;
    assert(browser_tag_row(moved, 320) == row);
    assert(!browser_tag_paint(b, 320, "", 0));               /* nothing known: nothing drawn */
    assert(browser_tag_paint(b, 320, "G MIN", 107.5f));   /* what i get lifted.wav reads */
    assert(browser_tag_row(b, 320) == row);                  /* the tag never changes its own row */
    FILE *f = fopen(argv[3], "wb"); assert(f);
    fwrite(b, 4, 320 * 240, f); fclose(f);
    puts("check-tag: all passed");
    return 0;
}
