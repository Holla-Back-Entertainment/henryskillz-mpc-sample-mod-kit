#!/usr/bin/env python3
"""Convert an image to a 320x240, 32-bit BGRX MPC Sample splash."""

from __future__ import annotations

import argparse
from pathlib import Path

WIDTH, HEIGHT = 320, 240


def convert(source: Path, output: Path, preview: Path | None = None) -> None:
    try:
        from PIL import Image
    except ImportError as error:
        raise SystemExit("Install Pillow first: python3 -m pip install Pillow") from error

    image = Image.open(source).convert("RGB")
    image.thumbnail((WIDTH, HEIGHT), Image.Resampling.LANCZOS)
    canvas = Image.new("RGB", (WIDTH, HEIGHT), (0, 0, 0))
    canvas.paste(image, ((WIDTH - image.width) // 2, (HEIGHT - image.height) // 2))
    rgb = canvas.tobytes()
    raw = bytearray(WIDTH * HEIGHT * 4)
    for index in range(WIDTH * HEIGHT):
        red, green, blue = rgb[index * 3 : index * 3 + 3]
        raw[index * 4 : index * 4 + 4] = bytes((blue, green, red, 0))
    output.write_bytes(raw)
    if preview:
        canvas.save(preview)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--preview", type=Path)
    args = parser.parse_args()
    convert(args.source, args.output, args.preview)
    print(f"Wrote {args.output} (307200 bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
