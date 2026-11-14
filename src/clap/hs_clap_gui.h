#ifndef HS_CLAP_GUI_H
#define HS_CLAP_GUI_H
/* The plugin side the editor needs (hs_clap.c), and the clap.gui extension (hs_clap_gui_mac.m). */
#include <clap/clap.h>
#include "../fx/ladspa.h"

enum { HS_EDIT_BEGIN, HS_EDIT_VALUE, HS_EDIT_END };

typedef struct {
    const LADSPA_Descriptor *d;
    float *ctl;                        /* indexed by LADSPA port */
    const uint32_t *param_port;
    uint32_t nparams;
    void *ctx;
    void **slot;                       /* the editor's state lives here, one per instance */
    const void *(*handle)(void *ctx);  /* the running LADSPA instance, or NULL (meters) */
    void (*edit)(void *ctx, int what, uint32_t port, float value);   /* knob turned: tell host and DSP */
} hs_gui_target;

hs_gui_target hs_clap_gui_target(const clap_plugin_t *p);
extern const clap_plugin_gui_t hs_clap_gui;
#endif
