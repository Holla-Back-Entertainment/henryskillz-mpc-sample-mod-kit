# Henry Skillz add-ons for SampQOL OS on the MPC Sample.
# Sourced by SampQOL's qol-run.sh (the installer adds one line for this).
# Copies the add-ons to RAM and sets HS_PRELOAD. Nothing is written to the MPC itself.
# If anything is missing, HS_PRELOAD stays empty and SampQOL runs exactly as stock.
HS="$DIR/hs"
HS_PRELOAD=
if cp "$HS/hs-flavor.so" /tmp/hs-flavor.so &&
   cp "$HS/hs-eq.so" /tmp/hs-eq.so &&
   cp "$HS/hs-fx.so" /tmp/hs-fx.so &&
   mkdir -p /tmp/hs-ladspa-test &&
   cp "$HS/hs-pro-fx-ladspa.so" /tmp/hs-ladspa-test/; then
    chmod 644 /tmp/hs-flavor.so /tmp/hs-eq.so /tmp/hs-fx.so /tmp/hs-ladspa-test/hs-pro-fx-ladspa.so
    HS_PRELOAD="/tmp/hs-flavor.so:/tmp/hs-eq.so:/tmp/hs-fx.so:"
fi 2>/dev/null
# Desk-style SEQ page with live VU and output LEDs. Optional: without hs-seq.so the stock SEQ page shows.
if [ -n "$HS_PRELOAD" ] && cp "$HS/hs-seq.so" /tmp/hs-seq.so 2>/dev/null; then
    chmod 644 /tmp/hs-seq.so
    HS_PRELOAD="$HS_PRELOAD/tmp/hs-seq.so:"
fi
