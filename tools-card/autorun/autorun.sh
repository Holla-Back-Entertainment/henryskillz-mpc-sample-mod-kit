#!/bin/sh
# HS Tools for the MPC Sample on stock firmware (no SampQOL needed): on-device stems, browser key and
# BPM tags, SEQ desk and LUFS meters. Copies the tools to RAM and restarts the stock app with them
# loaded. Nothing is written to the MPC: power off and pull the card to boot stock.
set -u
DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
[ "$(id -u)" = 0 ] || exit 0

PRE=
for so in hs-browser hs-seq; do
    cp "$DIR/hs/$so.so" "/tmp/$so.so" 2>/dev/null && chmod 644 "/tmp/$so.so" && PRE="$PRE/tmp/$so.so:"
done
# Stems last, so its screen copy sees what the others drew.
if cp "$DIR/stems/hs-stems-ui.so" /tmp/hs-stems-ui.so 2>/dev/null; then
    chmod 644 /tmp/hs-stems-ui.so && PRE="$PRE/tmp/hs-stems-ui.so:"
fi
[ -n "$PRE" ] || exit 0

systemctl stop ac50.service 2>/dev/null || true
killall "MPC Sample" 2>/dev/null || true
sleep 1
# ponytail: the stock ac50 start line, minus its panel firmware check (that already ran at boot).
# A RAM-only unit: gone at the next power-on.
systemd-run --unit=hs-tools --property=Restart=always --setenv=HOME=/tmp --setenv=MALLOC_ARENA_MAX=1 \
    --setenv=LD_PRELOAD="${PRE%:}" --setenv=HS_STEMS_DIR="$DIR/stems" \
    /bin/sh -c 'ulimit -S -s 1024; exec setarch -R -- "/usr/bin/MPC Sample"' >/dev/null 2>&1 ||
    systemctl start ac50.service
exit 0
