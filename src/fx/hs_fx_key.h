/* hs_fx_key.h: an effect's stable id, from its LADSPA Label.
 * The insert list prints the Label, so since v0.6.1 Labels are display names ("HS Bus Compressor").
 * Preset folders (MPC-Sample/Presets/<id>/) and the factory preset table keep the short ids they had
 * before, so presets saved on v0.6.0 still show. Crush moves from hs_distressor to hs_crush (trademark).
 * Unknown labels pass through unchanged. */
#ifndef HS_FX_KEY_H
#define HS_FX_KEY_H
#include <string.h>

static inline const char *hs_fx_key(const char *label)
{
    static const char *const keys[][2] = {
        {"HS Saturator", "hs_saturator"},       {"HS Air Exciter", "hs_air_exciter"},
        {"HS Channel Strip", "hs_channel_strip"}, {"HS Drive Sat", "hs_drive_sat"},
        {"HS Bus Compressor", "hs_bus_comp"},    {"HS Para EQ", "hs_para_eq"},
        {"HS Pump", "hs_pump"},                  {"HS Heat", "hs_heat"},
        {"HS Crush", "hs_crush"},                {"HS Tube Strip", "hs_tube_strip"},
        {"HS Bass Focus", "hs_bass_focus"},
    };
    for (unsigned i = 0; label && i < sizeof keys / sizeof *keys; ++i)
        if (!strcmp(label, keys[i][0])) return keys[i][1];
    return label;
}
#endif
