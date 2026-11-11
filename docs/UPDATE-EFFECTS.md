# Update just the effects

Already installed the kit? You only need the new effect files. No SampQOL download, no reinstall, and your projects stay as they are.

## What you need

- Your SD card with the kit already on it (it has an `autorun/hs` folder).
- A computer with a web browser.

## Steps

1. **Switch the MPC off** normally, take the SD card out and put it in your computer.
2. **Back up** the `autorun/hs` folder: copy it to your desktop.
3. Open the [installer page](https://hmukhwana.github.io/henryskillz-mpc-sample-mod-kit/install/) and, under **Already installed? Update just the effects**, click **Download hs-effects-update.zip**.
4. **Unzip** it. You get an `hs` folder with four files:
   `hs-pro-fx-ladspa.so`, `hs-fx.so`, `hs-flavor.so`, `hs-eq.so`.
5. **Copy those four files** into `autorun/hs` on the SD card. Say yes to replace. Replace all four together: a mix of old and new files can hide the HS effects list.
6. **Eject** the card, put it in the MPC, switch on and press **B1**.

The new effects are in the insert effects list with the others. The menu has room for eight add-on effects, so HS Air Exciter and HS Drive Sat leave the menu when HS Crush and HS Bass Focus arrive. Projects that already use them still open with them.

## Why this is safe to do

The card's start-up check only covers SampQOL's own files and its start script. The four effect files in `autorun/hs` are copied to memory at start-up and are not part of that check, so replacing them needs no re-signing. Nothing is written to the MPC itself.

## If something goes wrong

- **The MPC boots stock** (no HenrySkillz screen), or **restarts twice and then runs stock**: switch off, put your backed-up `hs` folder back in `autorun`, and tell us on the [issues page](https://github.com/hmukhwana/henryskillz-mpc-sample-mod-kit/issues).
- **The HS effects are missing from the list**: you probably replaced only some of the files. Copy all four again.
- Still stuck? Switch off and take the card out: the MPC is stock again. See [RECOVERY.md](RECOVERY.md).

## Your projects

Every HS effect keeps the same internal ID between versions, so projects that use them open as before. New effects only add to the list.
