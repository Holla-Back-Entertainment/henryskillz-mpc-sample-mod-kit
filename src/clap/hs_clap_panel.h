#ifndef HS_CLAP_PANEL_H
#define HS_CLAP_PANEL_H
/* The MPC faceplate of one CLAP instance, 320x200 pixels (0xRRGGBB): header 40 rows, panel 160. */
#include <stdint.h>
#include "../fx/ladspa.h"

enum { HS_PANEL_W = 320, HS_PANEL_H = 200 };
typedef struct hs_panel hs_panel;

/* ctl is indexed by LADSPA port; param_port maps control ordinals to ports. Both stay owned by the caller. */
hs_panel *hs_panel_new(const LADSPA_Descriptor *d, float *ctl, const uint32_t *param_port, uint32_t nparams);
void hs_panel_free(hs_panel *s);
const uint32_t *hs_panel_draw(hs_panel *s, const void *ladspa_handle);   /* handle may be NULL */
int hs_panel_click(hs_panel *s, int x, int y);         /* page / band buttons; 1 = redraw */
int hs_panel_knob_at(hs_panel *s, int x, int y);       /* LADSPA port under (x, y), or -1 */
float hs_panel_drag(hs_panel *s, int port, float start_value, float up_pixels);
#endif
