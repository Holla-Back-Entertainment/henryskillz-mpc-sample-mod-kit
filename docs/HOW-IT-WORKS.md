# How it works (for builders)

## The chain

1. **Firmware 1.3.0** runs `autorun/autorun.sh` from the SD card, but only if `autorun.check` matches.
   `autorun.check` = hex `sha256(autorun.json + autorun.sh + firmware salt)`.
   The method and salt were published by [orrpunk-art/mpc-sample-root-ssh](https://github.com/orrpunk-art/mpc-sample-root-ssh).
2. **SampQOL's `autorun.sh`** checks its own files against a `PINS` list of sha256 hashes, then starts `qol-run.sh`.
3. **`qol-run.sh`** restarts the MPC app with `LD_PRELOAD=mpcqol.so`. It falls back to stock after two quick crashes.
4. **These add-ons** are more `LD_PRELOAD` libraries. Most go ahead of `mpcqol.so` (`HS_PRELOAD`). Two go after it (`HS_POSTLOAD`): `hs-chains.so`, so its "Chain loaded" box draws on top of SampQOL's screens, then `hs-stems-ui.so`, so its screen copy sees every other drawing.
   Most of them hook symbols that SampQOL exports (`overlay_update`, `mixview_frame`, `fxmap_get_row`, `eq_spectrum_draw`), which is why they're tied to one SampQOL version. Each one that does checks the symbol's offset in `mpcqol.so` and steps aside on any other build.

## What the installer changes

On your own copy of SampQOL, in your browser ([installer/template.html](../installer/template.html)):
- `qol-run.sh`: one line sources `hs/hs-addons.sh`, and `LD_PRELOAD="$SO"` becomes `LD_PRELOAD="$HS_PRELOAD$SO$HS_POSTLOAD"`.
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
| `src/seq/` | The desk-style SEQ page and the METERS page on B2 (`hs-seq.so`) |
| `src/chains/` | Track chains and their "Chain loaded" box (`hs-chains.so`), the VOX, DRUMS and BASS chains |
| `src/chrome/` | Styling for SampQOL's list screens (`hs-chrome.so`) |
| `src/mixer/` | The graphic Ch. Mixer and Pad Mixer (`hs-mixer.so`) |
| `src/warp/` | Key and BPM tag in the browser (`hs-browser.so`) |
| `src/stems/` | Stems: the engine (`hs-stems`), the screen hook (`hs-stems-ui.so`) and the model converter |
| `addon/hs/` | `hs-addons.sh` (copies the add-ons to RAM, sets `HS_PRELOAD` and `HS_POSTLOAD`) and `hs-splash.raw` |
| `starter-card/` | The splash-only boot-test card |
| `tools/` | Command-line helpers: sign an autorun folder, make a splash |
| `installer/` | Page template, `make_installer.py` (embeds the add-ons), and `test_installer.mjs` (self-check) |

## The menu row limit

SampQOL 0.2.0 builds the insert menu in a fixed list of 40 rows, and 32 are stock effects. The picker (`src/fx/hs_fx_picker.c`) takes the stock Bus Compressor row out of that list, which leaves nine rows, and adds its rows only when they all fit, so more than nine HS rows hides the whole HS menu. It only takes the stock row out when the HS rows go in. Every effect stays in the library and the catalog, and the stock Bus Compressor stays in the MPC, so saved projects reopen them; `in_menu()` picks which nine get a menu row, and `check-picker.sh` fails if that count isn't `MENU_ROWS`.

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
