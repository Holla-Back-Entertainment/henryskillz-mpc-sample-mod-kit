# Henry Skillz add-ons for SampQOL OS on the MPC Sample.
# Sourced by SampQOL's qol-run.sh (the installer adds one line for this).
# Copies the add-ons to RAM and sets HS_PRELOAD (before SampQOL) and HS_POSTLOAD (after it). Nothing is written to the MPC itself.
# If anything is missing, HS_PRELOAD stays empty and SampQOL runs exactly as stock.
HS="$DIR/hs"
HS_PRELOAD=
HS_POSTLOAD=
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
# SampQOL screen styling and the graphic channel mixer (SampQOL 0.2.0 only; each checks that itself and
# steps aside on any other build). Optional, each on its own: they go after hs-fx and before hs-seq.
for so in hs-chrome hs-mixer; do
    if [ -n "$HS_PRELOAD" ] && cp "$HS/$so.so" "/tmp/$so.so"; then
        chmod 644 "/tmp/$so.so"
        HS_PRELOAD="${HS_PRELOAD%/tmp/hs-seq.so:}/tmp/$so.so:/tmp/hs-seq.so:"
    fi 2>/dev/null
done
# Track chains: rename a track VOX, DRUMS or BASS and, if it has no inserts, that chain loads.
# Rename a dialled-in track NAME# to save its inserts as MPC-Sample/Chains/NAME.xfx. Optional.
if [ -n "$HS_PRELOAD" ] && mkdir -p /tmp/hs-chains && cp "$HS/"*.xfx /tmp/hs-chains/ &&
   cp "$HS/hs-chains.so" /tmp/hs-chains.so; then
    chmod 644 /tmp/hs-chains.so /tmp/hs-chains/*
    export HS_CHAINS_USER="${DIR%/*}/MPC-Sample/Chains"
    # After SampQOL (before Stems), so its "Chain loaded" box draws on top of SampQOL's screens.
    HS_POSTLOAD=":/tmp/hs-chains.so"
fi 2>/dev/null
# Stems (only if "Add Stems" was ticked): browser SHIFT + hold B1 on a previewed file, Sample screen
# SHIFT + hold B3 on a pad. The engine and model stay in autorun/stems; the hook loads last, after
# SampQOL, so its screen copy sees every other drawing.
if [ -n "$HS_PRELOAD" ] && [ -f "$DIR/stems/hs-stems-ui.so" ] && cp "$DIR/stems/hs-stems-ui.so" /tmp/hs-stems-ui.so; then
    chmod 644 /tmp/hs-stems-ui.so
    export HS_STEMS_DIR="$DIR/stems"
    HS_POSTLOAD="$HS_POSTLOAD:/tmp/hs-stems-ui.so"
fi 2>/dev/null
