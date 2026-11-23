#!/usr/bin/env sh
set -eu
cd "$(dirname "$0")/.."
root=$(pwd)
foma_revision=885100f027356014b42a1d9c7d9902eb3ac41801
toolchain=konsumer/foma-toolchain@sha256:722764b0fa28f2e19b53a001acc5b9c26af026b53c9b59ec89e4c078780bd42b
mkdir -p .build
if [ ! -e .build/foma/.git ]; then
  git init -q .build/foma
fi
if ! git -C .build/foma cat-file -e "$foma_revision^{commit}" 2>/dev/null; then
  git -C .build/foma fetch --depth 1 https://github.com/konsumer/foma.git "$foma_revision"
fi
git -C .build/foma checkout -q --detach "$foma_revision"
mkdir -p .build/dsb
git -C .build/foma archive "$foma_revision" | tar -x -C .build/dsb
rm -f .build/dsb/src/*.c
cp firmware/src/* .build/dsb/src/
cp firmware/firmware.json .build/dsb/
cp firmware/web/dsb.css firmware/web/dsb-logo.png .build/dsb/web/
python3 tools/theme.py .build/dsb/web/index.html
docker run --rm --platform linux/amd64 --user "$(id -u):$(id -g)" \
  -v "$root/.build/dsb:/work" "$toolchain" python3 tools/build.py
mkdir -p dist
version=$(python3 -c 'import json; print(json.load(open("firmware/firmware.json"))["version"])')
case "$version" in
  *-lab) web_output=dist/web-lab ;;
  *) web_output=dist/web ;;
esac
mkdir -p "$web_output"
cp -R .build/dsb/build/web/. "$web_output/"
cp ".build/dsb/build/dsb-$version.fwsc" dist/
