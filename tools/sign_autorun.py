#!/usr/bin/env python3
"""Create or verify autorun.check for MPC Sample firmware 1.3.0/1.3.0.12."""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path

FIRMWARE_SALT = b"f7bed1c9-7e90-4731-9194-c996666b4331"


def calculate(folder: Path) -> str:
    json_bytes = (folder / "autorun.json").read_bytes()
    shell_bytes = (folder / "autorun.sh").read_bytes()
    return hashlib.sha256(json_bytes + shell_bytes + FIRMWARE_SALT).hexdigest()


def write(folder: Path) -> str:
    digest = calculate(folder)
    (folder / "autorun.check").write_text(digest, encoding="ascii")
    return digest


def verify(folder: Path) -> bool:
    expected = (folder / "autorun.check").read_text(encoding="ascii").strip()
    return expected == calculate(folder)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("folder", type=Path, help="folder containing autorun.json and autorun.sh")
    parser.add_argument("--verify", action="store_true", help="verify instead of writing")
    args = parser.parse_args()
    if args.verify:
        ok = verify(args.folder)
        print("VALID" if ok else "INVALID")
        return 0 if ok else 1
    print(write(args.folder))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
