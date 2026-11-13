/* Draws every page of every HS effect's CLAP faceplate and checks each has the panel on it.
 * With a folder argument, also writes one PPM per page (3x) there for a look.
 *   built and run by build-mac.sh; by hand: see that script's check-panel line. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hs_clap_panel.h"
#include "../chains/factory_presets.h"

const LADSPA_Descriptor *ladspa_descriptor(unsigned long index);

static float start_value(const LADSPA_Descriptor *d, unsigned long q)
{
    const LADSPA_PortRangeHint *r = &d->PortRangeHints[q];
    float lo = r->LowerBound, hi = r->UpperBound;
    return hs_snap_default(d, q, lo + (hi - lo) * .4f);   /* off-centre, so knobs show a position */
}

static void save(const uint32_t *px, const char *dir, const char *name, int page)
{
    char path[512];
    snprintf(path, sizeof path, "%s/%s-%02d.ppm", dir, name, page);
    for (char *c = path + strlen(dir); *c; ++c) if (*c == ' ') *c = '_';
    FILE *f = fopen(path, "wb");
    assert(f);
    fprintf(f, "P6 %d %d 255\n", HS_PANEL_W * 3, HS_PANEL_H * 3);
    for (int y = 0; y < HS_PANEL_H * 3; ++y)
        for (int x = 0; x < HS_PANEL_W * 3; ++x) {
            uint32_t q = px[(y / 3) * HS_PANEL_W + x / 3];
            fputc(q >> 16 & 255, f); fputc(q >> 8 & 255, f); fputc(q & 255, f);
        }
    fclose(f);
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : NULL;
    int effects = 0, pages = 0;
    for (const LADSPA_Descriptor *d; (d = ladspa_descriptor((unsigned long)effects)); ++effects) {
        float *ctl = calloc(d->PortCount, sizeof *ctl);
        uint32_t *pp = calloc(d->PortCount, sizeof *pp), n = 0;
        for (unsigned long q = 0; q < d->PortCount; ++q)
            if (hs_is_ctl_in(d, q)) { ctl[q] = start_value(d, q); pp[n++] = (uint32_t)q; }
        hs_panel *s = hs_panel_new(d, ctl, pp, n);
        assert(s);
        int bands = !strcmp(d->Name, "HS Para EQ") ? 3 : 1;   /* global + two bands is enough to see */
        for (int b = 0; b < bands; ++b) {
            static uint32_t first[HS_PANEL_W * 40];   /* the header: its page label is unique per page */
            for (int page = 0; page < 40; ++page) {
                const uint32_t *px = hs_panel_draw(s, NULL);
                if (page == 0) memcpy(first, px, sizeof first);
                else if (!memcmp(first, px, sizeof first)) break;   /* wrapped: every page seen */
                int lit = 0;                                     /* the panel must not be blank */
                for (int i = 40 * HS_PANEL_W; i < HS_PANEL_W * HS_PANEL_H; ++i) lit += (px[i] & 0xffffff) != 0;
                assert(lit > HS_PANEL_W * 100);
                if (dir) save(px, dir, d->Name, b * 10 + page);
                ++pages;
                /* the art/tabs strip above the knobs is not a knob (Para EQ's band and page tabs live there) */
                assert(hs_panel_knob_at(s, 50, 40 + 50) < 0);
                /* turn a knob through the drag path: value must stay in range */
                int port = hs_panel_knob_at(s, 57, 120);
                if (port >= 0) {
                    float v = hs_panel_drag(s, port, ctl[port], 500);
                    const LADSPA_PortRangeHint *r = &d->PortRangeHints[port];
                    assert(v >= r->LowerBound - 1e-3f && v <= r->UpperBound + 1e-3f && isfinite(v));
                }
                hs_panel_click(s, 300, 30);                      /* next page */
            }
            hs_panel_click(s, 300, 8);                           /* Para EQ: next band */
        }
        hs_panel_free(s);
        free(ctl); free(pp);
    }
    assert(effects == 11);
    printf("check-panel: %d effects, %d pages drawn\n", effects, pages);
    return 0;
}
