# D.S.B. for FM-1

A standalone M‑VAVE FM‑1 companion for Drums. Sample. Bass. Play a tine electric piano, capture a phrase to eight playable chops, and program drums on the same instrument. The FM‑1 can also take MIDI from an MPC Sample and send its stereo audio back to the MPC's inputs.

## First session

1. Press **HOME** and play the piano. **ARP** toggles four-note HS Chord voicings.
2. Press **PLAY** for the starter beat. In **SEQ**, the first 16 keys toggle steps for the selected drum. **SEL** selects a drum. Hold **REC** and play the first four keys to finger-drum and enter hits at the current step.
3. From **HOME** or **FX**, press **REC**, play a phrase, then press **REC** again to finish. Capture takes the piano before tremolo and leaves drums and chops out of the recording. It stops automatically at four seconds.
4. Press **FX** and play the first eight keys for slices. The encoders select the slice and adjust start, end, and pitch. **EDIT** toggles live chop entry into the running pattern. Return to **HOME** to play over it.
5. **GLO** opens the three-section mixer. With transport stopped, **SAVE** saves piano and mixer settings, tempo, swing, and the drum grid. Recordings and chop patterns are session RAM.

| Section | Knob 1 | Knob 2 | Knob 3 | Knob 4 |
| --- | --- | --- | --- | --- |
| Keys | Tone | Drive | Tremolo depth | Tremolo rate |
| Chops | Slice | Start | End | Pitch |
| Drums | Drum | Drum section level | Swing | BPM |
| Mixer | Keys level | Chops level | Drums level | |

Incoming MIDI channel 2 triggers chops on notes 36–43. Channel 10 triggers drums (36 kick, 38 or 40 snare, 42 or 44 hat, 46 or 49 cowbell). Other channels play the electric piano with MIDI velocity. The Foma panel API supplies key edges, so local keys use a fixed velocity in this build.

## Build and preview

Run `tools/build.sh` with Git, Python 3, and Docker. The script fetches the pinned [Foma](https://github.com/konsumer/foma) source and runs its pinned toolchain image. It produces:

- `dist/dsb-0.2.1.fwsc` — experimental FM‑1 firmware package.
- `dist/web/` — Foma browser test bench built from the same C source. Serve it with `python3 -m http.server 8000 --bind 127.0.0.1 --directory dist/web`, open [localhost:8000](http://localhost:8000), and select **Switch on**.

Open the `.fwsc` itself in [fm1-emu](https://joakimeriksson.github.io/fm1-emu/) for chip-level preview. Hardware confirmation is pending. The current build uses a recording-calibrated fixed-point tine piano and four sampled TR-808 hits. See the [sound research and sample credits](research/README.md). External MIDI piano channels support sustain on CC64. The four-second mono resample buffer is 16-bit at 22.05 kHz. USB audio capture, sample flash storage, and A/B scenes are future milestones.

The FM‑1 display and test bench use [HenrySkillz’s original D.S.B. mark](https://github.com/Holla-Back-Entertainment/henryskillz-mpc-sample-mod-kit/blob/44c20d5bfd5a1b6c6b6ffa0e9881de4292df4ce9/src/seq/dsb_logo.h).

## Warm Cab experiment

Run `tools/build.sh` to build the experimental `dist/dsb-0.3-lab.fwsc` and `dist/web-lab/`. In Keys, **ENV** toggles Direct and Warm Cab. The approved 0.2.1 package and its browser test bench are separate at the paths above. See the [sound development notes](research/README.md#03-lab--warm-cab-and-pocket-studies).

The [hosted test bench](https://holla-back-entertainment.github.io/henryskillz-mpc-sample-mod-kit/fm1/) serves the built firmware and an offline laptop bundle.
