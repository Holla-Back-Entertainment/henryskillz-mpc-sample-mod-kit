#!/usr/bin/env python3
"""Turn a user-supplied MPC Sample autorun package into a ready-to-copy card folder."""

from __future__ import annotations

import argparse
import hashlib
import re
import shutil
import sys
import tempfile
import zipfile
from pathlib import Path

from sign_autorun import write as sign_autorun

RAW_SIZE = 320 * 240 * 4
PIN_LINE = re.compile(rb"(?<![0-9a-fA-F])[0-9a-fA-F]{64}([ \t]+qol-splash\.raw)")


class BuildError(RuntimeError):
    pass


def safe_extract(archive: Path, destination: Path) -> None:
    with zipfile.ZipFile(archive) as zf:
        root = destination.resolve()
        for item in zf.infolist():
            target = (destination / item.filename).resolve()
            if target != root and root not in target.parents:
                raise BuildError(f"Unsafe path in ZIP: {item.filename}")
        zf.extractall(destination)


def find_autorun(root: Path) -> Path:
    matches = [
        path
        for path in root.rglob("autorun")
        if path.is_dir() and (path / "autorun.json").is_file() and (path / "autorun.sh").is_file()
    ]
    if root.name.lower() == "autorun" and (root / "autorun.json").is_file() and (root / "autorun.sh").is_file():
        matches.insert(0, root)
    unique = list(dict.fromkeys(path.resolve() for path in matches))
    if len(unique) != 1:
        raise BuildError(f"Expected one autorun folder, found {len(unique)}")
    return unique[0]


def apply_splash(folder: Path, splash: Path, *, required: bool = True) -> bool:
    raw = splash.read_bytes()
    if len(raw) != RAW_SIZE:
        raise BuildError(f"Splash must be exactly {RAW_SIZE} bytes, got {len(raw)}")
    target = folder / "qol-splash.raw"
    if not target.exists():
        if required:
            raise BuildError("This package has no qol-splash.raw adapter yet; building without --splash is supported")
        return False
    target.write_bytes(raw)
    digest = hashlib.sha256(raw).hexdigest().encode("ascii")
    script = folder / "autorun.sh"
    before = script.read_bytes()
    after, count = PIN_LINE.subn(lambda match: digest + match.group(1), before)
    if count != 1:
        raise BuildError("Could not safely update the qol-splash.raw checksum pin")
    script.write_bytes(after)
    return True


def build(
    source: Path,
    output: Path,
    splash: Path | None = None,
    *,
    require_splash: bool = True,
) -> Path:
    source = source.expanduser().resolve()
    output = output.expanduser().resolve()
    if output == source or source in output.parents:
        raise BuildError("Output must not be inside the source package")

    with tempfile.TemporaryDirectory(prefix="henryskillz-mpc-") as temp_name:
        temp = Path(temp_name)
        if source.is_file() and zipfile.is_zipfile(source):
            safe_extract(source, temp)
            autorun = find_autorun(temp)
        elif source.is_dir():
            autorun = find_autorun(source)
        else:
            raise BuildError("Choose an OS ZIP or an extracted folder")

        staged = temp / "staged-autorun"
        if autorun == staged or staged in autorun.parents:
            raise BuildError("Unexpected package layout")
        shutil.copytree(autorun, staged)
        if splash is not None:
            apply_splash(staged, splash.expanduser().resolve(), required=require_splash)
        sign_autorun(staged)

        if output.exists():
            if not (output / ".henryskillz-build").is_file():
                raise BuildError(f"Refusing to replace a folder not created by this tool: {output}")
            shutil.rmtree(output)
        output.parent.mkdir(parents=True, exist_ok=True)
        shutil.copytree(staged, output)
        (output / ".henryskillz-build").write_text(
            "Generated locally by HenrySkillz MPC Sample Mod Kit.\n",
            encoding="utf-8",
        )
    return output


def choose_source() -> Path | None:
    try:
        import tkinter as tk
        from tkinter import filedialog

        root = tk.Tk()
        root.withdraw()
        selected = filedialog.askopenfilename(
            title="Choose the official MPC Sample OS ZIP",
            filetypes=[("ZIP packages", "*.zip"), ("All files", "*")],
        )
        root.destroy()
        return Path(selected) if selected else None
    except Exception:
        value = input("Drag the official OS ZIP here, then press Enter: ").strip().strip('"')
        return Path(value) if value else None


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", nargs="?", type=Path, help="official OS ZIP or extracted package")
    parser.add_argument("--output", type=Path, help="output autorun folder")
    parser.add_argument("--splash", type=Path, help="optional 320x240 BGRX raw splash")
    args = parser.parse_args()
    source = args.source or choose_source()
    if source is None:
        print("No package selected.")
        return 1
    output = args.output or source.expanduser().resolve().parent / "HenrySkillz-Card" / "autorun"
    splash = args.splash  # the OS keeps its own splash unless you pass one
    try:
        result = build(source, output, splash, require_splash=True)
    except (BuildError, OSError, zipfile.BadZipFile) as error:
        print(f"BUILD FAILED: {error}", file=sys.stderr)
        return 1
    print("\nCARD READY")
    print(result)
    print("Copy the whole autorun folder to the top level of an exFAT SD card.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
