# Undo and recovery

Read this before you install. Everything here is safe and needs no tools.

## The one rule

The mod lives **only on the SD card**, in a folder called `autorun`. It is loaded into memory when the MPC starts and gone when it switches off.
Nothing is written to the MPC's own storage.

**No card, or no `autorun` folder = a stock MPC Sample.**

## Back up first

1. Put the SD card in your computer.
2. Copy **everything** on it to a folder on your computer, e.g. `MPC card backup 2026-11-10`.
3. Do it again before any update.

SampQOL's own manual warns that a failed project load can delete samples. A backup is the only real protection.

## Undo the mod

Pick one:
- **Fastest:** switch off, take the SD card out, switch on. Stock MPC.
- **Keep the card in:** put the card in your computer and delete the `autorun` folder (or rename it to `autorun-off`). Stock MPC.
- **Back to plain SampQOL:** copy the `autorun` folder from the original SampQOL zip back onto the card.

## What the MPC does on its own

- **A wrong or edited `autorun` folder:** the MPC ignores it and starts stock. That's the firmware's own check.
- **Wrong firmware (not 1.3.0):** SampQOL doesn't start the mod. You get stock.
- **The app crashes twice in a row:** SampQOL's launcher gives up and starts the stock app.

## Report a problem

[Open an issue](https://github.com/Holla-Back-Entertainment/henryskillz-mpc-sample-mod-kit/issues) and include:
- Your MPC firmware version and SampQOL version.
- What you did and what you saw.
- If there's a `logs` folder inside `autorun` on the card, attach `events.log` and `last-exit.log`.
