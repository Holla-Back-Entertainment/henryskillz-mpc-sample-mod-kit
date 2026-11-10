from __future__ import annotations

import hashlib
import tempfile
import unittest
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TOOLS = ROOT / "tools"

import sys

sys.path.insert(0, str(TOOLS))
from easy_builder import BuildError, build, safe_extract  # noqa: E402
from sign_autorun import calculate, verify, write  # noqa: E402


class ToolTests(unittest.TestCase):
    def test_starter_signature_is_valid(self) -> None:
        self.assertTrue(verify(ROOT / "starter-card" / "autorun"))

    def test_signer_round_trip(self) -> None:
        with tempfile.TemporaryDirectory() as name:
            folder = Path(name)
            (folder / "autorun.json").write_text("{}", encoding="utf-8")
            (folder / "autorun.sh").write_text("#!/bin/sh\n", encoding="utf-8")
            self.assertEqual(write(folder), calculate(folder))
            self.assertTrue(verify(folder))

    def test_zip_slip_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as name:
            root = Path(name)
            archive = root / "bad.zip"
            with zipfile.ZipFile(archive, "w") as zf:
                zf.writestr("../escape", "nope")
            with self.assertRaises(BuildError):
                safe_extract(archive, root / "out")

    def test_builder_preserves_package_and_resigns(self) -> None:
        with tempfile.TemporaryDirectory() as name:
            root = Path(name)
            source = root / "source" / "autorun"
            source.mkdir(parents=True)
            (source / "autorun.json").write_text("{}", encoding="utf-8")
            (source / "autorun.sh").write_text("#!/bin/sh\n", encoding="utf-8")
            (source / "NOTICE.txt").write_text("keep me", encoding="utf-8")
            output = root / "card" / "autorun"
            build(root / "source", output)
            self.assertEqual((output / "NOTICE.txt").read_text(), "keep me")
            self.assertTrue(verify(output))

    def test_builder_replaces_sampqol_splash_and_pin(self) -> None:
        with tempfile.TemporaryDirectory() as name:
            root = Path(name)
            source = root / "source" / "autorun"
            source.mkdir(parents=True)
            original = bytes(320 * 240 * 4)
            replacement = bytes([23]) * (320 * 240 * 4)
            old_digest = hashlib.sha256(original).hexdigest()
            new_digest = hashlib.sha256(replacement).hexdigest()
            (source / "autorun.json").write_text("{}", encoding="utf-8")
            (source / "autorun.sh").write_text(
                f'PINS="{old_digest}  qol-splash.raw"\n', encoding="utf-8"
            )
            (source / "qol-splash.raw").write_bytes(original)
            splash = root / "new.raw"
            splash.write_bytes(replacement)
            output = root / "card" / "autorun"
            build(root / "source", output, splash)
            self.assertEqual((output / "qol-splash.raw").read_bytes(), replacement)
            self.assertIn(new_digest, (output / "autorun.sh").read_text())
            self.assertTrue(verify(output))


if __name__ == "__main__":
    unittest.main()
