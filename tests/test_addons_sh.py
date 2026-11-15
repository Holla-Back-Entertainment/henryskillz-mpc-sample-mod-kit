from __future__ import annotations

import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "addon" / "hs" / "hs-addons.sh"
LIBS = ("hs-flavor", "hs-eq", "hs-fx", "hs-seq", "hs-browser", "hs-chrome", "hs-mixer", "hs-chains")


def preload(marker: bool) -> str:
    with tempfile.TemporaryDirectory() as name:
        hs = Path(name) / "autorun" / "hs"
        hs.mkdir(parents=True)
        for lib in LIBS:
            (hs / f"{lib}.so").write_bytes(b"x")
        (hs / "hs-pro-fx-ladspa.so").write_bytes(b"x")
        (hs / "VOX.xfx").write_bytes(b"x")
        if marker:
            (hs / "list-styling.on").write_bytes(b"")
        out = subprocess.run(
            ["sh", "-c", f'DIR="{Path(name) / "autorun"}"; . "{SCRIPT}"; echo "$HS_PRELOAD"'],
            capture_output=True, text=True, check=True,
        )
        return out.stdout.strip()


class ListStylingOptInTests(unittest.TestCase):
    def test_off_by_default(self) -> None:
        loaded = preload(False)
        self.assertNotIn("hs-chrome", loaded)
        self.assertIn("hs-mixer", loaded)
        self.assertTrue(loaded.endswith("/tmp/hs-seq.so:"))

    def test_opt_in_marker_loads_it(self) -> None:
        loaded = preload(True)
        self.assertIn("/tmp/hs-chrome.so:", loaded)
        self.assertIn("hs-mixer", loaded)
        self.assertTrue(loaded.endswith("/tmp/hs-seq.so:"))


if __name__ == "__main__":
    unittest.main()
