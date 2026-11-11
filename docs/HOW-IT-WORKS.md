# How it works (for builders)

## The chain

1. **Firmware 1.3.0** runs `autorun/autorun.sh` from the SD card, but only if `autorun.check` matches.
   `autorun.check` = hex `sha256(autorun.json + autorun.sh + firmware salt)`.
   The method and salt were published by [orrpunk-art/mpc-sample-root-ssh](https://github.com/orrpunk-art/mpc-sample-root-ssh).
2. **SampQOL's `autorun.sh`** checks its own files against a `PINS` list of sha256 hashes, then starts `qol-run.sh`.
3. **`qol-run.sh`** restarts the MPC app with `LD_PRELOAD=mpcqol.so`. It falls back to stock after two quick crashes.
4. **These add-ons** are more `LD_PRELOAD` libraries placed ahead of `mpcqol.so`.
   Most of them hook symbols that SampQOL exports (`overlay_update`, `fxmap_get_row`, `eq_spectrum_draw`), which is why they're tied to one SampQOL version.

## What the installer changes

On your own copy of SampQOL, in your browser ([installer/template.html](../installer/template.html)):
- `qol-run.sh`: one line sources `hs/hs-addons.sh`, and `LD_PRELOAD="$SO"` becomes `LD_PRELOAD="$HS_PRELOAD$SO"`.
- `autorun.sh`: the `qol-run.sh` pin gets the new hash.
- `autorun.check`: re-signed.
- `qol-run.sh` also shows `hs/hs-splash.raw` for 2 s after SampQOL's own splash.
- `hs/`: our files are added.

It refuses to run if `mpcqol.so` isn't a supported version, if any pinned file was already changed, or if the add-ons are already installed.

## Files

| Path | What |
|---|---|
| `src/fx/` | The 10 LADSPA effects (`hs-pro-fx-ladspa.so`), the effect list and live displays (`hs-fx.so`), and the firmware-1.3.0 hooks (`hs-flavor.so`, `probe.cc`) |
| `src/eq/` | The EQ page grid (`hs-eq.so`) |
| `src/seq/` | The desk-style SEQ page and B2-toggleable stereo meters (`hs-seq.so`) |
| `addon/hs/` | `hs-addons.sh` (copies the add-ons to RAM, sets `HS_PRELOAD`) and `hs-splash.raw` |
| `starter-card/` | The splash-only boot-test card |
| `tools/` | Command-line helpers: sign an autorun folder, make a splash |
| `installer/` | Page template, `make_installer.py` (embeds the add-ons), and `test_installer.mjs` (self-check) |

## The eight-row menu limit

SampQOL 0.2.0 builds the insert menu in a fixed list of 40 rows, and 32 are stock effects. The picker (`src/fx/hs_fx_picker.c`) adds its rows only when they all fit, so more than eight HS rows hides the whole HS menu. Every effect stays in the library and the catalog, so saved projects reopen them by UniqueID; `in_menu()` picks which eight get a menu row, and `check-picker.sh` fails if that count isn't `MENU_ROWS`.

## Build

Needs Docker (on Apple Silicon the `gcc:12` image runs natively, and it's the same Debian 12 / GCC 12 the MPC uses).

    ./build.sh                                  # builds addon/hs/*.so, then dist/henryskillz-mpc-sample-installer.html
    node installer/test_installer.mjs            # self-check on a synthetic SampQOL folder (runs in CI)
    node installer/test_installer.mjs <SampQOL autorun folder> <out folder>   # real download, real add-ons (bootable; run ./build.sh first)

The FX self-checks (`src/fx/check-*.c`, `bench-*.c`) build on the same image.

## Supporting a new SampQOL version

1. Add its `mpcqol.so` sha256 to `SAMPQOL` in `installer/make_installer.py`.
2. Check the two `qol-run.sh` lines the installer looks for still exist.
3. Check the hooked symbols still exist (`nm -D mpcqol.so`).
4. **Test on a real MPC Sample before releasing.**
