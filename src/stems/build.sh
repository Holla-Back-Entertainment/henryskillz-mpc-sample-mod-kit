#!/bin/sh
# Builds hs-stems (static aarch64 engine) and hs-stems-ui.so (screen hook) into docs/install/stems/,
# where the online installer fetches them when "Add Stems" is ticked, and runs the host checks.
# The model (spl4h.bin) is already there: see README.md for how it was made. Needs Docker.
set -e
cd "$(dirname "$0")"
mkdir -p out
cc -Wall -Wextra -DSTEMS_HOST -o out/check_ui check_ui.c hs_stems_ui.c stems_pad.c -lm && out/check_ui
docker run --rm --platform linux/arm64 -v "$PWD":/s -w /s debian:bookworm sh -c '
apt-get update -qq >/dev/null && apt-get install -y -qq gcc libc6-dev libopenblas-pthread-dev libgfortran-12-dev zlib1g-dev >/dev/null 2>&1
gcc -O3 -march=armv8.2-a+fp16+dotprod -mtune=cortex-a55 -Wall -Wextra -static -o out/hs-stems hs_stems.c \
  -I/usr/include/aarch64-linux-gnu/openblas-pthread /usr/lib/aarch64-linux-gnu/openblas-pthread/libopenblas.a -lgfortran -lpthread -lm
gcc -O2 -Dmain=engine_main -I/usr/include/aarch64-linux-gnu/openblas-pthread -o /tmp/crs check-resample.c /usr/lib/aarch64-linux-gnu/openblas-pthread/libopenblas.a -lgfortran -lpthread -lm && /tmp/crs
gcc -O2 -Dmain=engine_main -I/usr/include/aarch64-linux-gnu/openblas-pthread -o /tmp/cm3 check-mp3.c /usr/lib/aarch64-linux-gnu/openblas-pthread/libopenblas.a -lgfortran -lpthread -lm && /tmp/cm3
gcc -O2 -Wall -Wextra -shared -fPIC -o out/hs-stems-ui.so hs_stems_ui.c stems_pad.c -ldl -lpthread -lz -lm
ls -la out/hs-stems out/hs-stems-ui.so'
cp out/hs-stems out/hs-stems-ui.so ../../docs/install/stems/
