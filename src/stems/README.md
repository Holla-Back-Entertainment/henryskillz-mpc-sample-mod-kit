# Stems (roadmap #8)

Splits a WAV into Vocals / Drums / Bass / Other on the MPC Sample itself, from the browser or from a pad.

## Use
Two places to start it:
- **Browser** (SAMPLE SELECT), Preview on (the stock default): highlight a WAV so it previews, hold
  SHIFT: B1 reads **Stems** (it only repeats Back there in stock). With Preview off no path is known
  and SHIFT+B1 stays stock Back. Stock SHIFT+B2 Eject and SHIFT+B3 Delete are untouched.
- **Sample screen** (any tab): hold SHIFT: B3 reads **Stems** (stock SHIFT+B1 is Loop Lock, SHIFT+B2
  Normalize, SHIFT+B3 does nothing). It stems the selected pad's sample.

Either opens the chooser: the wheel (or -/+) moves, a wheel push ticks a stem, B2 switches
**Separate** (one WAV per ticked stem) and **Sum** (the ticked stems mixed into one WAV), B3 **Go**,
B1 **Cancel**. The ticks and Sum/Separate stay as last used until the app restarts. Leaving the screen
(or picking another pad) closes the chooser. While it runs, B1 (**Cancel**) stops it and removes what
this run wrote. The "Saved" box clears after 4 s or on any control.

Out (16-bit, 44.1 kHz stereo):
- browser: `<name> Stems/` next to the file, `<name>-Vocals.wav` ... or `<name>-Bass+Other.wav` (Sum).
- pad: `MPC-Sample/Samples/Stems/<sample> Stems/` on the card (External > Samples > Stems in the browser).

Which file a pad plays: the pad label under the tabs ("A06", or "S03 T01 A06" with SampQOL) is read off
the screen, then the app's autosave (/data/TempProjects/Temp-Project.xpj, written ~10 s after a change)
gives that track's pad sample, a WAV in Temp-Project_[ProjectData]. So a pad just loaded or edited may
show "Waiting for the autosave..." for up to 13 s. Stems come from the whole sample file: Trim
start/end, tune and other pad settings are not applied. A pad with layers uses its first layer. A
track that isn't a drum program says so.

Input: 44.1 kHz WAV, 16/24/32-bit or float, mono or stereo. Other rates show an error in the box.
Speed on the real Sample: about real time (30 s of audio in ~29 s on 2 cores, SCHED_IDLE), ~320 MB RAM.
Sum costs the same as Separate (all four nets always run). Quality: Spleeter 4stems; nothing above
11 kHz in any stem. For HQ stems use the Mac chop.py (htdemucs_ft).

## Files
| Card | Source | Build |
|---|---|---|
| stems/hs-stems | hs_stems.c (Spleeter 4stems inference in C + OpenBLAS sgemm) | build.sh |
| stems/hs-stems-ui.so | hs_stems_ui.c (hooks, chooser, job) + stems_pad.c (pad label, autosave) + font.h + pad_glyphs.h | build.sh |
| stems/spl4h.bin | Spleeter 4stems checkpoint (Deezer, MIT) -> export_weights.py, fp16 | see below |
| qol-run.sh | copies hs-stems-ui.so to /tmp, adds it to LD_PRELOAD, sets HS_STEMS_DIR | - |

`hs-stems WEIGHTS IN.wav OUTDIR [PREFIX [PICK]]`: PICK is letters from VDBO (default all four); a
trailing `+` sums them into one file. Summed masks are exact: the iSTFT is linear (lab: Bass+Other sum
= Bass + Other within 3 LSB of dither).

font.h: DejaVu Sans Mono via make_font.py. pad_glyphs.h: make_glyphs.py, cut from lab shots of the pad
label in test/pad/ (stock and SampQOL layouts, every bank A-H and pad 01-16); the script re-reads every
shot and fails on a miss.

Weights: dump the checkpoint to .npz with the official spleeter package (python:3.10 arm64 container,
`tf.train.load_checkpoint` on pretrained_models/4stems), then `export_weights.py w4.npz out/spl4h.bin`.
hs-stems matches official Spleeter output at 54-77 dB SNR per stem with the fp16 weights (64-84 dB fp32).

Checks: `build.sh` runs check_ui: naming, browser/SHIFT-layer detection, the pad label on lab shots
(both layouts, SHIFT held, negatives), the autosave walker on a fixture (escapes, submix tracks,
layers, path names refused), picks and output names.
Benchmarks: BENCH.md. Screen mocks shown to Henry: preview/ (stems-preview.png: browser, where B3 became
B1 since stock SHIFT+B3 is Delete there; pad-stems-preview.png: the pad flow as built).
