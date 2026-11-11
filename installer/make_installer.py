#!/usr/bin/env python3
"""Builds dist/henryskillz-mpc-sample-installer.html: the template with addon/hs/* embedded."""
import base64, json, pathlib

ROOT = pathlib.Path(__file__).resolve().parent.parent
SAMPQOL = {"fc9285ac4a3145e04c1b162e3f5058a9c447a9752fec1e06510156ff20575dbd": "0.2.0"}  # mpcqol.so sha256 -> version

files = {f"hs/{p.name}": base64.b64encode(p.read_bytes()).decode()
         for p in sorted((ROOT / "addon/hs").iterdir()) if p.is_file() and not p.name.startswith(".")}
need = {"hs/hs-addons.sh", "hs/hs-splash.raw", "hs/hs-flavor.so", "hs/hs-eq.so", "hs/hs-fx.so", "hs/hs-seq.so", "hs/hs-browser.so", "hs/hs-pro-fx-ladspa.so"}
missing = need - files.keys()
if missing:
    raise SystemExit(f"missing add-on files (run ./build.sh): {sorted(missing)}")

html = (ROOT / "installer/template.html").read_text()
html = (html.replace("__SAMPQOL_HASHES__", json.dumps(SAMPQOL))
            .replace("__SAMPQOL_VERSION__", "/".join(SAMPQOL.values()))
            .replace("__ADDON_FILES__", json.dumps(files)))
# dist/ = the release download; docs/install/ = the online installer (GitHub Pages)
for out in (ROOT / "dist/henryskillz-mpc-sample-installer.html", ROOT / "docs/install/index.html"):
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(html)
    print(f"{out.relative_to(ROOT)}  {out.stat().st_size // 1024} KB, {len(files)} add-on files")
