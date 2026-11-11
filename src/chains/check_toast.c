// Host check for the chain box: cc -DCHAINS_TOAST_HOST check_toast.c chains_toast.c && ./a.out [in.raw out.raw TITLE DETAIL]
// With arguments it paints the box on a 320x240 BGRX frame (preview-toast.py makes the PNG).
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "chains_toast.h"

static uint32_t px[240 * 320];

int main(int argc, char **argv)
{
    if (argc == 5) {
        FILE *f = fopen(argv[1], "rb");
        if (!f || fread(px, 4, 320 * 240, f) != 320 * 240) return 1;
        fclose(f);
        chains_toast_paint(px, 320, argv[3], argv[4]);
        f = fopen(argv[2], "wb");
        if (!f || fwrite(px, 4, 320 * 240, f) != 320 * 240) return 1;
        return fclose(f) != 0;
    }
    memset(px, 0x40, sizeof px);
    chains_toast_paint(px, 320, "Chain loaded", "A name far too long to fit inside the box at all");
    assert((px[88 * 320 + 40] & 0xffffff) == 0xffffff && (px[152 * 320 + 280] & 0xffffff) == 0xffffff);
    assert((px[87 * 320 + 40] & 0xffffff) == 0x404040 && (px[100 * 320 + 39] & 0xffffff) == 0x404040);  // nothing outside
    int lit = 0;
    for (int y = 124; y < 140; ++y) for (int x = 42; x < 279; ++x) lit += (px[y * 320 + x] & 0xff) > 0x60;
    assert(lit > 50);
    puts("check-toast: OK");
    return 0;
}
