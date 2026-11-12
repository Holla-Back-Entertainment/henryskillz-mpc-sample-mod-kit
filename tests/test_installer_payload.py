from __future__ import annotations

import importlib.util
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "make_installer", ROOT / "installer" / "make_installer.py"
)
assert SPEC and SPEC.loader
make_installer = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(make_installer)


class InstallerPayloadTests(unittest.TestCase):
    def test_payload_uses_exact_allowlist(self) -> None:
        with tempfile.TemporaryDirectory() as name:
            root = Path(name)
            payload = root / "addon" / "hs"
            payload.mkdir(parents=True)
            for filename in make_installer.PAYLOAD:
                (payload / filename).write_bytes(filename.encode())
            (payload / "hs-pro-old.so").write_bytes(b"must not ship")

            files = make_installer.payload_files(root)

            self.assertEqual(
                set(files), {f"hs/{filename}" for filename in make_installer.PAYLOAD}
            )
            self.assertNotIn("hs/hs-pro-old.so", files)


if __name__ == "__main__":
    unittest.main()
