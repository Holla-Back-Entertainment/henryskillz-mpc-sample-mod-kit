# Third-party notices

This project is independent community work. It is not affiliated with, endorsed by or supported by Akai Professional or inMusic Brands, Inc. Akai Professional, MPC and MPC Sample are trademarks of their respective owners.

## SampQOL OS

SampQOL OS is copyright © 2026 Snare Bes and is distributed under its own license. It is **not included** here. Download it only from the [official SampQOL OS releases](https://github.com/snarebes/SampQOL-OS/releases). This project preserves its license and copyright notices when processing a user-supplied copy.

## MPC Sample autorun research

Thanks to **puckcv / orrpunk-art** and the MPC-Forums community for publicly documenting the MPC Sample autorun mechanism and temporary USB SSH work. Their reference project is [mpc-sample-root-ssh](https://github.com/orrpunk-art/mpc-sample-root-ssh).

The firmware-check implementation here is independently written from the published formula and verified against Henry's own device experiments. No private keys, Akai binaries or third-party repository files are included.

## LADSPA

`src/fx/ladspa.h` is the LADSPA plugin API header (LGPL-2.1, Richard W.E. Furse, Paul Barton-Davis, Stefan Westerfeld).

## minimp3

`src/warp/minimp3.h` is minimp3 by lieff (https://github.com/lieff/minimp3), dedicated to the public domain under CC0 1.0. The browser key tag uses it to read MP3s.

## Stems (optional, only when "Add Stems" is ticked)

- **Spleeter** 4stems model by Deezer Research (https://github.com/deezer/spleeter), MIT License, copyright (c) 2019 Deezer. `docs/install/stems/spl4h.bin` is that model's weights converted to float16 by `src/stems/export_weights.py`. `src/stems/hs_stems.c` is an independent C implementation of the Spleeter network that runs it.
- **OpenBLAS** (https://github.com/OpenMathLib/OpenBLAS), BSD 3-Clause License, is linked into `hs-stems`, with **libgfortran** (GPL v3 with the GCC Runtime Library Exception).
- `hs-stems` is statically linked against **glibc** (LGPL 2.1 or later). Its full source and build script (`src/stems/build.sh`) are here, so you can rebuild and relink it yourself.
- `src/stems/font.h` is drawn from **DejaVu Sans Mono** (https://dejavu-fonts.github.io), Bitstream Vera and DejaVu font licenses (free to use, copy and modify). The track chains box (`src/chains/chains_toast.c`, in `hs-chains.so`) draws its text with the same font, whether or not Stems is installed.
- `src/stems/minimp3.h` is minimp3 (CC0), as in the browser tag.
