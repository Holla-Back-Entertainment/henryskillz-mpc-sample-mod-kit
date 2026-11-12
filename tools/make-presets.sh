#!/bin/sh
# Rebuild the preset chains (one HS Tube Strip each) into src/chains/. Run from the repo root.
set -e
cc -O1 -Wall -Wextra -Werror -Isrc/fx tools/xfx_preset.c src/fx/hs_ladspa.c src/fx/hs_eq2.c src/fx/hs_pump.c \
  src/fx/hs_heat.c src/fx/hs_distressor.c src/fx/hs_bass_focus.c src/fx/hs_tube_strip.c src/fx/hs_bus.c \
  src/fx/hs_xsat.c src/fx/hs_channel.c src/fx/hs_saturator.c src/fx/hs_air_exciter.c src/fx/hs_env.c -o /tmp/xfxp -lm
EQ='"EQ Before=0" "SC EQ=0" "High Q=0" "Treble Hz=1"'
eval /tmp/xfxp hs_tube_strip Drive=3.5 HPF=20 Output=1.5 Threshold=-8 Ratio=4 Attack=15 Release=110 $EQ \
  '"Bass Hz=2" "Bass (dB)=2" "Low Mid (dB)=-3" "Low Mid (Hz)=350" "High Mid (Hz)=4000" "High Mid (dB)=2" "Treble (dB)=0"' > src/chains/KICK.xfx
eval /tmp/xfxp hs_tube_strip Drive=3 HPF=30 Output=1 Threshold=-10 Ratio=3 Attack=30 Release=300 $EQ \
  '"Bass Hz=2" "Bass (dB)=3" "Low Mid (dB)=-2" "Low Mid (Hz)=250" "High Mid (Hz)=900" "High Mid (dB)=2" "Treble (dB)=-2"' > src/chains/BASSLINE.xfx
