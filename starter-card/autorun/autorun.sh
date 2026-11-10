#!/bin/sh
# HenrySkillz MPC Sample starter splash.
# Changes only temporary runtime state; the stock app is restarted afterwards.
set -u

DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
SPLASH="$DIR/henryskillz-splash.raw"

[ "$(id -u)" = 0 ] || exit 0
[ -r "$SPLASH" ] || exit 0
[ "$(wc -c < "$SPLASH" | tr -d ' ')" = 307200 ] || exit 0

systemctl stop ac50.service 2>/dev/null || true
sleep 1
killall "MPC Sample" 2>/dev/null || true

if [ -w /sys/class/graphics/fb0/blank ] && [ -w /dev/fb0 ]; then
  echo 1 > /sys/class/graphics/fb0/blank 2>/dev/null || true
  echo 0 > /sys/class/graphics/fb0/blank 2>/dev/null || true
  cat "$SPLASH" > /dev/fb0 2>/dev/null || true
  sleep 2
fi

systemctl start ac50.service 2>/dev/null || true
exit 0
