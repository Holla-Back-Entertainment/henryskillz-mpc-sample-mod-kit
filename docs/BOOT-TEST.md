# MPC Sample boot test

Use a **spare exFAT SD card**. Do not use your main card for this first test.

## Before you start

- [ ] Save the current project and switch the MPC Sample off normally.
- [ ] Back up the spare SD card to the computer.
- [ ] Confirm the MPC Sample is running firmware **1.3.0 / 1.3.0.12**.
- [ ] Download **SampQOL OS 0.2.0** from [Snare Bes's official release page](https://github.com/snarebes/SampQOL-OS/releases).
- [ ] Open the [HenrySkillz online installer](https://hmukhwana.github.io/henryskillz-mpc-sample-mod-kit/install/), click **Choose zip**, give it the official SampQOL ZIP and download `autorun-hs.zip`. The file chooser is verified in Chrome and Safari; drag-and-drop is not yet certified.
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
- [ ] On HS Pump, HS Heat and HS Crush, the **middle screen button** moves through their pages (3 on Pump, 6 on Heat, 3 on Crush).
- [ ] The insert list shows exactly nine HS effects: Saturator, Channel Strip, Heat, Tube Strip (Harmonic); Bus Compressor, Pump, Crush (Dynamics); Para EQ, Bass Focus (EQ/Filter). HS Air Exciter, HS Drive Sat and the stock (non-HS) Bus Compressor are not listed.
- [ ] A project saved with the **stock Bus Compressor** on an insert still opens with it loaded.
- [ ] A project saved with **HS Drive Sat** or **HS Air Exciter** on an insert (from v0.3.0) still opens with that effect loaded and passing audio.
- [ ] The EQ page shows the dB/Hz grid.
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
