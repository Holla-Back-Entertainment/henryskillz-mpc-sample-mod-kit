# Henry Skillz add-ons for SampQOL OS on the MPC Sample.
# Sourced by SampQOL's qol-run.sh (the installer adds one line for this).
# Copies the add-ons to RAM and sets HS_PRELOAD. Nothing is written to the MPC itself.
# If anything is missing, HS_PRELOAD stays empty and SampQOL runs exactly as stock.
HS="$DIR/hs"
HS_PRELOAD=
if cp "$HS/hs-flavor.so" /tmp/hs-flavor.so &&
   cp "$HS/hs-eq.so" /tmp/hs-eq.so &&
   cp "$HS/hs-fx.so" /tmp/hs-fx.so &&
   cp "$HS/hs-seq.so" /tmp/hs-seq.so &&
   cp "$HS/hs-browser.so" /tmp/hs-browser.so &&
   mkdir -p /tmp/hs-ladspa-test &&
   cp "$HS/hs-pro-fx-ladspa.so" /tmp/hs-ladspa-test/; then
    chmod 644 /tmp/hs-flavor.so /tmp/hs-browser.so /tmp/hs-eq.so /tmp/hs-fx.so /tmp/hs-seq.so /tmp/hs-ladspa-test/hs-pro-fx-ladspa.so
    HS_PRELOAD="/tmp/hs-flavor.so:/tmp/hs-browser.so:/tmp/hs-eq.so:/tmp/hs-fx.so:/tmp/hs-seq.so:"
fi 2>/dev/null
# Track chains: rename a track VOX, DRUMS or BASS and, if it has no inserts, that chain loads.
# Rename a dialled-in track NAME# to save its inserts as MPC-Sample/Chains/NAME.xfx. Optional.
if [ -n "$HS_PRELOAD" ] && mkdir -p /tmp/hs-chains && cp "$HS/"*.xfx /tmp/hs-chains/ &&
   cp "$HS/hs-chains.so" /tmp/hs-chains.so; then
    chmod 644 /tmp/hs-chains.so /tmp/hs-chains/*
    export HS_CHAINS_USER="${DIR%/*}/MPC-Sample/Chains"
    HS_PRELOAD="/tmp/hs-flavor.so:/tmp/hs-chains.so:${HS_PRELOAD#/tmp/hs-flavor.so:}"  # same order as Henry's card
fi 2>/dev/null
