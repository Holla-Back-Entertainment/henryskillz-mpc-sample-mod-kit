#!/bin/sh
# The picker shows the HS Effects menu only when hs_rows matches the library's effect names in
# order (a mismatch hides the whole menu). Run after adding an effect: sh check-picker.sh
set -eu
cd "$(dirname "$0")"
T=$(mktemp -d)
cc -O2 -shared -fPIC -I. -DHS_MPC_LADSPA_RECALL hs_ladspa.c hs_eq2.c hs_pump.c hs_heat.c hs_distressor.c hs_bass_focus.c hs_tube_strip.c hs_bus.c hs_xsat.c \
   hs_channel.c hs_saturator.c hs_air_exciter.c -o "$T/fx.so" -lm $([ "$(uname)" = Darwin ] && echo -Wl,-undefined,dynamic_lookup)
cat > "$T/n.c" <<'C'
#include <dlfcn.h>
#include <stdio.h>
#include "ladspa.h"
int main(int c, char **v) { (void)c; void *l = dlopen(v[1], RTLD_NOW);
  LADSPA_Descriptor_Function f = (LADSPA_Descriptor_Function)dlsym(l, "ladspa_descriptor");
  for (unsigned long i = 0; f(i); ++i) printf("%s\n", f(i)->Name); return 0; }
C
cc -I. "$T/n.c" -o "$T/n" -ldl 2>/dev/null || cc -I. "$T/n.c" -o "$T/n"
"$T/n" "$T/fx.so" > "$T/lib.txt"
sed -n '/hs_rows\[\] = {/,/^};/p' hs_fx_picker.c | grep -o '"[^"]*"' | tr -d '"' > "$T/rows.txt"
diff "$T/lib.txt" "$T/rows.txt"
# SampQOL 0.2.0 leaves 8 menu rows, 9 with stock Bus Compressor hidden: more hides the whole HS menu.
[ $(($(wc -l < "$T/rows.txt") - 2)) -eq "$(sed -n 's/.*MENU_ROWS = \([0-9]*\).*/\1/p' hs_fx_picker.c)" ] || { echo "check-picker: menu rows != MENU_ROWS"; exit 1; }
echo "check-picker: OK"
rm -rf "$T"
