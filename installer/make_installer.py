#!/usr/bin/env python3
"""Build the installer with the exact, reviewed add-on payload embedded."""
import base64, json, pathlib

ROOT = pathlib.Path(__file__).resolve().parent.parent
SAMPQOL = {"fc9285ac4a3145e04c1b162e3f5058a9c447a9752fec1e06510156ff20575dbd": "0.2.0"}  # mpcqol.so sha256 -> version

PAYLOAD = (
    "BASS.xfx",
    "DRUMS.xfx",
    "VOX.xfx",
    "hs-addons.sh",
    "hs-browser.so",
    "hs-chains.so",
    "hs-chrome.so",
    "hs-eq.so",
    "hs-flavor.so",
    "hs-fx.so",
    "hs-mixer.so",
    "hs-pro-fx-ladspa.so",
    "hs-seq.so",
    "hs-splash.raw",
)
def payload_files(root: pathlib.Path) -> dict[str, str]:
    """Return only the release allowlist, never incidental build/test files."""
    payload_dir = root / "addon/hs"
    missing = [name for name in PAYLOAD if not (payload_dir / name).is_file()]
    if missing:
        raise SystemExit(f"missing add-on files (run ./build.sh): {missing}")
    return {
        f"hs/{name}": base64.b64encode((payload_dir / name).read_bytes()).decode()
        for name in PAYLOAD
    }

# Stems is optional and too big to embed: the online installer fetches these from docs/install/stems/
# (same origin on GitHub Pages) and checks each against this sha256 before adding it.
def main() -> None:
    import hashlib

    files = payload_files(ROOT)
    stems_dir = ROOT / "docs/install/stems"
    stems = {f"stems/{n}": hashlib.sha256((stems_dir / n).read_bytes()).hexdigest() for n in ("hs-stems", "hs-stems-ui.so", "spl4h.bin")}
    html = (ROOT / "installer/template.html").read_text()
    html = (html.replace("__SAMPQOL_HASHES__", json.dumps(SAMPQOL))
                .replace("__SAMPQOL_VERSION__", "/".join(SAMPQOL.values()))
                .replace("__ADDON_FILES__", json.dumps(files))
                .replace("__STEMS_FILES__", json.dumps(stems)))
    # dist/ = the release download; docs/install/ = the online installer (GitHub Pages)
    for out in (ROOT / "dist/henryskillz-mpc-sample-installer.html", ROOT / "docs/install/index.html"):
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(html)
        print(f"{out.relative_to(ROOT)}  {out.stat().st_size // 1024} KB, {len(files)} add-on files")


if __name__ == "__main__":
    main()
