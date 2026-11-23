# D.S.B. 0.2 sound development

## Real instruments

**Electric piano.** The 1979 Rhodes manual describes the tine, tone bar, hammer, damper, and magnetic pickup. Pickup alignment changes the balance of harmonics, and pickup distance changes output and response. Pfeifle's DAFx model explains the resulting nonlinearity in terms of tine movement, flux, and induced voltage. The FM-1 build uses a reduced model: a tuned fundamental and four pickup harmonics, with register- and velocity-dependent attack and held profiles. It interpolates the measured anchors, blends toward the held spectrum after the strike, and damps on note-off. MIDI CC64 holds released notes until pedal-up. Equal temperament is retained for playing with other instruments.

The reference is Jeffrey Learman's 1977 **Rhodes Mark I Stage 73**, recorded at the harp connector with his preferred EQ and later noise reduction. Its low-mid scoop and treble emphasis are part of this specific reference. It is one voiced and processed instrument. The direct recordings provide no universal Rhodes transfer function. The current engine fits magnitudes, with synthesized phase and release, and uses a bounded profile in the hardest bass register for polyphonic headroom. The drive control adds an asymmetric soft knee. The tremolo is opposite-phase stereo amplitude modulation, in the spirit of a Suitcase setup.

- [Rhodes Service Manual, Chapter 4](https://www.fenderrhodes.com/org/manual/ch4.html)
- [Florian Pfeifle, real-time physical model, DAFx 2017](https://www.dafx17.eca.ed.ac.uk/papers/DAFx17_paper_79.pdf)
- [Jeffrey Learman, jRhodes3d](https://github.com/sfzinstruments/jlearman.jRhodes3d/tree/4508215a0ec6613a2af19d6f4dc7b329e0e7bba0) · [Sample license and credit](https://github.com/sfzinstruments/jlearman.jRhodes3d/blob/4508215a0ec6613a2af19d6f4dc7b329e0e7bba0/LICENSE)

**Drum machine.** The default kit uses four 1994 recordings of a real Roland TR-808 by Michael Fischer, obtained from the TidalCycles CC0 mirror. The ROM uses one recording each for kick, snare, closed hat, and cowbell. The files were trimmed, peak-scaled, and faded at the end. Kick and bell are stored at 22.05 kHz. Snare and hat retain 44.1 kHz for transient detail. The service notes give typical generator frequencies, while the CSV reports the dominant whole-hit FFT peak of each specific recording. The two measurements describe different things. There is no circuit emulation or continuous drum tuning in this kit.

- [Michael Fischer, Roland TR-808 Rhythm Composer Sound Sample Set](https://github.com/tidalcycles/sounds-tr808-fischer/tree/85fbecf1bec32553395625ea659e2a56dfd7c0e1) · [CC0 mirror license](https://github.com/tidalcycles/sounds-tr808-fischer/blob/85fbecf1bec32553395625ea659e2a56dfd7c0e1/LICENSE)
- [Roland TR-808 service notes](https://el34world.com/charts/Schematics/files/Roland/Roland_tr_808_service_manual.pdf)
- [Werner, Abel and Smith, measured and modelled TR-808 bass-drum circuit](https://www.dafx.de/paper-archive/2014/dafx14_kurt_james_werner_a_physically_informed%2C_ci.pdf)

## Measurements and reproducibility

`rhodes-mark-i-measurements.csv` contains 65 measured takes across 15 recorded MIDI pitches. The jRhodes SFZ maps suffix 5 to the lightest velocity range and suffix 1 to the hardest. Frequency was estimated around equal temperament from a Hann-windowed 70–500 ms segment. Harmonic magnitudes were measured relative to each take's fundamental at 18–95 ms and 230–410 ms. The late comparison window was 1.18–1.36 s, approximately one second after the earlier held window. Very weak partials can be close to the source noise floor.

The raw Rhodes recordings **are credited to Jeffrey Learman** and are used to fit the profile rather than embedded in firmware. The drum ROM derives from **Michael Fischer's TR-808 Sound Sample Set**. To regenerate from local checkouts at the revisions linked above, install Python with NumPy and FFmpeg and run:

```sh
python tools/analyze_tines.py /path/to/jRhodes3d/jRhodes3d-mono > research/rhodes-mark-i-measurements.csv
python tools/tine_profile.py research/rhodes-mark-i-measurements.csv > firmware/src/tine_profile.h
python tools/drum_kit.py /path/to/sounds-tr808-fischer firmware/src/drum_kit.h research/tr808-kit-measurements.csv
```

The A/B comparison uses firmware audio at default tone with drive and tremolo turned down. Its error metric is the absolute difference from the reference attack's second harmonic relative to the fundamental. This tests how well the fitted model reproduces one tonal feature at measured notes. It is an in-sample calibration check and does not establish authenticity across unmeasured notes, phases, cabinet acoustics, or release noises.

In the 14 available recording/pitch/velocity combinations in this A/B, the median absolute error dropped from **7.9 dB** to **1.3 dB**. See [the per-note comparison](tine-calibration-ab.csv). The PCM replay check also measured an A4 at 440.08 Hz, its neutral-pitch chop at 440.08 Hz, and its +12-semitone chop at 880.17 Hz.

## 0.3-lab · Warm Cab and pocket studies

The approved piano was fitted primarily to the pickup and attack spectrum. Its stage recording includes the owner's EQ, and the existing calibration doesn't describe a speaker or room. This experiment adds a separate Warm Cab setting in the Keys section. Press ENV to switch between Direct and Warm Cab. The setting shapes the modeled pickup with input coupling, a low-mid emphasis, a soft peak knee, and a two-pole speaker roll-off. The blend ramps over 256 frames. Resampling captures the selected voicing before tremolo. The response is a filter design for listening evaluation. It has yet to be calibrated against cabinet recordings or a particular amplifier.

The listening comparison uses the same original 75 BPM MIDI sequence in Direct and Warm Cab. The groove take adds staggered drum MIDI with a later backbeat and alternating hat velocities. All three recordings are rendered from the firmware's WebAssembly build using the same playback gain. They illustrate separate variables: electrical tone shaping, keyboard touch and voicing, and drum placement. The neo-soul reference is a direction for feel and production.

- [D’Angelo on the recording of Voodoo](https://www.youtube.com/watch?v=2rcih7mwLvc)
- [Rhodes 1979 service manual](https://www.fenderrhodes.com/service/manual.html)
