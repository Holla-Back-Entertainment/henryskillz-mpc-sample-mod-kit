from __future__ import annotations

import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "addon" / "hs" / "hs-addons.sh"
LIBS = ("hs-flavor", "hs-eq", "hs-fx", "hs-seq", "hs-browser", "hs-chrome", "hs-mixer", "hs-chains")


def preload(marker: bool, stems: bool = False, sampqol_split: bool = False) -> str:
    with tempfile.TemporaryDirectory() as name:
        hs = Path(name) / "autorun" / "hs"
        hs.mkdir(parents=True)
        for lib in LIBS:
            (hs / f"{lib}.so").write_bytes(b"x")
        (hs / "hs-pro-fx-ladspa.so").write_bytes(b"x")
        (hs / "VOX.xfx").write_bytes(b"x")
        if marker:
            (hs / "list-styling.on").write_bytes(b"")
        if stems:
            (hs.parent / "stems").mkdir()
            (hs.parent / "stems" / "hs-stems-ui.so").write_bytes(b"x")
        if sampqol_split:
            (hs.parent / "split").mkdir()
            (hs.parent / "split" / "mpcsplit").write_bytes(b"x")
        out = subprocess.run(
            ["sh", "-c", f'DIR="{Path(name) / "autorun"}"; . "{SCRIPT}"; echo "$HS_PRELOAD|$HS_POSTLOAD"'],
            capture_output=True, text=True, check=True,
        )
        return out.stdout.strip()


def preload_only(marker: bool) -> str:
    return preload(marker).split("|")[0]


class ListStylingOptInTests(unittest.TestCase):
    def test_off_by_default(self) -> None:
        loaded = preload_only(False)
        self.assertNotIn("hs-chrome", loaded)
        self.assertIn("hs-mixer", loaded)
        self.assertTrue(loaded.endswith("/tmp/hs-seq.so:"))

    def test_opt_in_marker_loads_it(self) -> None:
        loaded = preload_only(True)
        self.assertIn("/tmp/hs-chrome.so:", loaded)
        self.assertIn("hs-mixer", loaded)
        self.assertTrue(loaded.endswith("/tmp/hs-seq.so:"))



class StemsTests(unittest.TestCase):
    def test_stems_load_without_sampqol_split(self) -> None:
        self.assertIn("/tmp/hs-stems-ui.so", preload(False, stems=True).split("|")[1])

    def test_stems_off_when_sampqol_has_its_own(self) -> None:
        # SampQOL 0.4.0+ ships autorun/split/ with Stems Separation on the same SHIFT + B3
        post = preload(False, stems=True, sampqol_split=True).split("|")[1]
        self.assertNotIn("hs-stems-ui", post)
        self.assertIn("/tmp/hs-chains.so", post)


if __name__ == "__main__":
    unittest.main()
