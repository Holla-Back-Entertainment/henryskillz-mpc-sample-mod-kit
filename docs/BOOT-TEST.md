# MPC Sample boot test

Use a **spare exFAT SD card**. Do not use your main card for this first test.

## Before you start

- [ ] Save the current project and switch the MPC Sample off normally.
- [ ] Back up the spare SD card to the computer.
- [ ] Confirm the MPC Sample is running firmware **1.3.0 / 1.3.0.12**.
- [ ] Download **SampQOL OS 0.2.0** from [Snare Bes's official release page](https://github.com/snarebes/SampQOL-OS/releases).
- [ ] Open the [HenrySkillz online installer](https://holla-back-entertainment.github.io/henryskillz-mpc-sample-mod-kit/install/), click **Choose zip**, give it the official SampQOL ZIP and download `autorun-hs.zip`. The file chooser is verified in Chrome and Safari; drag-and-drop is not yet certified.
- [ ] Unzip `autorun-hs.zip` and copy its `autorun` folder to the top level of the spare SD card.
- [ ] Safely eject the SD card from the computer.

## Cold boot

- [ ] With the MPC switched off, insert the spare SD card.
- [ ] Switch the MPC on from cold.
- [ ] When **Start SampQOL OS?** appears, press **B1**.
- [ ] Confirm the original **SampQOL** start screen appears first.
- [ ] Confirm the **HenrySkillz** start screen appears next.
- [ ] Confirm the normal MPC interface finishes loading.

If the unit hangs or stays black, hold power to switch it off, remove the card and boot stock. Do not continue the effect checks.

## Effects and display

Open the insert effects list and test each effect with audio playing. Load one effect at a time and confirm that audio still passes.

- [ ] **HS Saturator** loads and passes audio.
- [ ] **HS Channel Strip** loads and passes audio.
- [ ] **HS Bus Compressor** loads and passes audio.
- [ ] On HS Bus Compressor, turning **K2** steps the Ratio through 1.5:1, 2:1, 3:1, 4:1, 10:1, 20:1 and X (X = limiting). Note what Ratio and Threshold show when it first loads: the emulator showed X and 20 dB, but the intended default ratio is 2:1.
- [ ] **HS Para EQ** loads and passes audio.
- [ ] **HS Pump** loads and passes audio. On its first page, turning **K2** lights the ratio lamps from 1 to 20.
- [ ] **HS Heat** loads and passes audio. On its first page, turning **K1** moves the amber lamp across the 8 circuits; on its second page, **K1** moves the lamp across the 7 filter shapes.
- [ ] **HS Crush** loads and passes audio near the same level. On its first page, turning **K2** lights the ratio lamps from 1 to MAX, and the red GR DB lamps move with the music (they show NO METER if they cannot read it: note that).
- [ ] **HS Tube Strip** loads and passes audio near the same level (compressor idle at load). On the OPTO COMP page, turning **K1** (Threshold) down compresses and the amber OPTO GR DB lamps move with the music (note it if they show NO METER). The middle screen button moves through 7 pages; EQ First redraws the signal path as EQ > COMP.
- [ ] **HS Bass Focus** loads and passes audio at the same level with Bass at 0 dB. Turning **K2** down moves the curve below the crossover marker; **K3** switches the lamp between MONO and STEREO.
- [ ] On HS Pump, HS Heat and HS Crush, the **middle screen button** moves through their pages (6 on Pump: COMP, TIME, COLOUR, KEY, MOD, MOD TIME; 8 on Heat; 6 on Crush), and the tab row lights the page you're on.
- [ ] On Channel Strip, Drive Sat, Bus Compressor, Pump, Heat, Crush, Tube Strip or Para EQ, hold **B2** for half a second: PRESETS opens. A quick B2 still changes the normal page.
- [ ] The first slots show named starter presets (Heat: WARM TAPE, LOFI FILTER, FUZZ BASS, AUTO WAH). Select one and press **SHIFT+B3**: it says LOADED and the knobs move to it. On Para EQ, loading KICK draws a curve with four bands.
- [ ] A fresh **HS Heat** loads on **SATURATION**. A fresh **HS Bus Compressor** does not load on ratio X.
- [ ] With audio playing and no knob touched, the **Para EQ** shows a moving spectrum behind the curve, and the **Crush** and **Tube Strip** GR lamps move.
- [ ] After choosing HS Crush, the insert slot list reads **HS Crush**. A project saved with Crush by an earlier kit version still opens with it loaded.
- [ ] In PRESETS, **K1** and **DATA** select all eight slots. Hold **B3** to save a slot, change two controls, then press **SHIFT+B3**: both saved values return. Power-cycle and confirm the slot is still present.
- [ ] Briefly remove or rename `MPC-Sample/Presets` on the card, then save a preset: the folder is recreated and the completed slot file appears without a leftover `.new` file.
- [ ] HS Channel Strip has a **DESS** tab: with Range up, a bright vocal loses its "s" sounds; Mode **LISTEN** plays only what is being removed; Range 0 changes nothing.
- [ ] **MOD:** on HS Pump's KEY page, set Send to **A**. On a second track's HS Crush MOD page, set Source **A**, a Target and some Depth: that knob now moves with the first track's level.
- [ ] HS Drive Sat, HS Bus Compressor and HS Para EQ have no Bypass knob on their pages; the Bypass button above the screen still bypasses them.
- [ ] The insert list shows exactly nine HS effects: Saturator, Channel Strip, Heat, Tube Strip (Harmonic); Bus Compressor, Pump, Crush (Dynamics); Para EQ, Bass Focus (EQ/Filter). HS Air Exciter, HS Drive Sat and the stock (non-HS) Bus Compressor are not listed.
- [ ] A project saved with the **stock Bus Compressor** on an insert still opens with it loaded.
- [ ] A project saved with **HS Drive Sat** or **HS Air Exciter** on an insert (from v0.3.0) still opens with that effect loaded and passing audio.
- [ ] The EQ page shows the dB/Hz grid.
- [ ] On any HS effect with pages (Pump, Heat, Crush, Channel Strip), the box over the **middle screen button** shows **PAGE 1/N**; pressing it shows PAGE 2/N, and **SHIFT** + it goes back. No "2 PAGE" text inside the panel.
- [ ] The **SEQ** page shows the desk panel (logo, VU meter, tempo box, output LEDs). Tempo, bars, Rec Quant and swing still change with their knobs, and long values (128 Bars, Q 1/32T) are readable.
- [ ] With a sequence playing, the **VU needle and output LEDs move with the music** without pressing anything.
- [ ] Leave the SEQ page playing for **3 minutes**: the MPC does not freeze or restart.
- [ ] On the SEQ page, **B2** shows the meters page with LUFS M, S and I and PEAK, and they move with the music.
- [ ] Rename an empty track **VOX**: Channel Strip, Bus Compressor and Air Exciter load on its inserts, and a **Chain loaded / VOX** box shows on top of the track page for about 1.5 s, then clears. One **UNDO** takes back the name and the chain.
- [ ] Rename a track that already has inserts **DRUMS**: its inserts stay as they were, and the box says **Chain not loaded / Track already has effects**.
- [ ] Rename a dialled-in track **TEST#**: the box says **Chain saved / TEST: N effects**, the track is named **TEST** again (no #), `MPC-Sample/Chains/TEST.xfx` appears on the card, and a new track named TEST loads that chain. One **UNDO** goes back to the old name without a crash.
- [ ] In the browser, preview a WAV and an MP3: the highlighted row shows a key and BPM tag, and scrolling away hides it.
- [ ] (Card made with **Add Stems**) Preview a WAV, hold **SHIFT** and hold **B1** for 1.5 s: the Stems chooser opens. **Go** writes the stems into a "NAME Stems" folder next to the file. A quick SHIFT + B1 still does its stock job.
- [ ] (Add Stems) On the Sample screen, hold **SHIFT** and hold **B3** for 1.5 s on a pad with a sample, on a track whose bank has empty pads: the stems land in `MPC-Sample/Samples/Stems`, load onto the next empty pads of that bank, and the box says where ("On pads A02-A05"). Each pad plays its stem.
- [ ] (Add Stems) Press **UNDO** once per stem: each press shows **Load Sample** and takes one stem off its pad.
- [ ] (Add Stems) Open the chooser again, turn the wheel a few steps, then **Cancel**. Press **UNDO** once: note whether it shows **Set Start** (the emulator still shows one per wheel turn; the fix needs a hardware read to confirm).
- [ ] (Add Stems) On a track whose bank is full, a pad split keeps the files on the card and the box says **Bank full: not on pads**.
- [ ] Save the project, switch off and on, reopen it: the stem pads still play (no "Sample not found").
- [ ] SampQOL's list screens (PAD FX insert pages, the effect list) show a red bar at the top, a dark blue palette and a cyan edge on the selected row. Text stays readable.
- [ ] **PAD FX**, then **B2**: the Ch. Mixer shows as console strips (pan, M and S, fader, meter, output). **B2** again: the Pad Mixer in the same style. Faders and meters move with the music, M and S light when pressed.
- [ ] The controls respond without a crash, freeze or unexpected silence.

## Return to stock

- [ ] Save any work and switch the MPC off normally.
- [ ] Remove the spare SD card.
- [ ] Switch the MPC on again.
- [ ] Confirm it boots into the stock MPC software without a SampQOL prompt or HenrySkillz screen.

## Result

**Date:** ____________________  **Firmware:** ____________________

**Kit version/commit:** ____________________  **SampQOL version:** ____________________

- [ ] PASS: every check above passed.
- [ ] FAIL: stop testing and record the first failed step below.

**First failed step / what appeared on screen:**

______________________________________________________________________________

______________________________________________________________________________

**Audio or control notes:**

______________________________________________________________________________

______________________________________________________________________________
