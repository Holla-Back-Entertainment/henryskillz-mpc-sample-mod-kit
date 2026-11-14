# Build your own: add to SampQOL, or make your own OS

This is for people who want to go further than installing the HS effects: write their own add-ons, or build their own MPC Sample mod from scratch.
It's more technical than the [install guide](INSTALL-GUIDE.md), but it still starts with the rules.

## First: Snare Bes's rules for SampQOL OS

SampQOL OS is Snare Bes's work. Its license (`LICENSE.txt`, in every SampQOL download) is short. Read it before you touch anything. In plain English:

**You may:**
- use SampQOL on an MPC Sample you own or may use, for anything, including commercial work and live shows;
- study it, decompile it and change it **for your own use**.

**You may not, without his written permission:**
- upload, host or share SampQOL, or a changed version of it, anywhere. Share **the link to his official download**, never the files;
- sell it, rent it, or charge for it or for access to it;
- remove his license or copyright notice, or **present SampQOL as your own work**.

**Also:**
- It's an unofficial mod with no warranty. Using it may affect your support from Akai and inMusic.
- Never contact Akai or inMusic support about it. Remove the `autorun` folder and check the problem also happens on stock first.
- If you break these terms, your right to use SampQOL ends straight away.
- If you want to share a modified version or host a mirror, his license says to ask him on his GitHub page.

His license is the one that counts. If anything here seems to disagree with it, the license wins: [SampQOL OS on GitHub](https://github.com/snarebes/SampQOL-OS).

**How this kit follows those rules:** it never contains SampQOL. Each user downloads SampQOL from Snare Bes, and the installer changes their own copy on their own computer.
His license file, name and start screen stay in place. The HS effects are entirely our own code. Copy this model if you share your own add-ons.

---

## How an MPC Sample mod starts

All of this works on firmware **1.3.0 (1.3.0.12)** only.

1. At start-up the firmware looks for an `autorun` folder on the SD card.
2. It reads `autorun.json`, which names the script to run (normally `autorun.sh`).
3. It runs that script as root, **but only if `autorun.check` matches**.
   `autorun.check` is the SHA-256 (as hex text) of `autorun.json` + `autorun.sh` + a fixed firmware salt.
   The formula was published by [orrpunk-art/mpc-sample-root-ssh](https://github.com/orrpunk-art/mpc-sample-root-ssh).
   `tools/sign_autorun.py` in this repository writes it for you.
4. If the check fails, the firmware skips the whole folder and the MPC starts stock. That's your safety net.

Everything runs from the card and from RAM. Nothing is written to the MPC's internal storage unless your own script does it, **so don't**.

## Option A: add your own effects to SampQOL (for your own use)

This is what the HS kit does, and you can do the same on your own card.

1. Start from a fresh, official SampQOL download.
2. Put your libraries in a folder of your own inside `autorun` (this kit uses `hs/`).
3. In `qol-run.sh`, load them ahead of SampQOL's module: `LD_PRELOAD="your-libs:$SO"`. See [installer/template.html](../installer/template.html) for the exact two-line change.
4. `autorun.sh` holds a `PINS` list of SHA-256 hashes for SampQOL's own files. Update the hash for any file you changed (`shasum -a 256 qol-run.sh`).
5. Re-sign: `python3 tools/sign_autorun.py path/to/autorun`.
6. Copy to the card and boot. If it comes up stock, a hash or the signature is wrong.

**Sharing it:** don't share the folder, because it contains SampQOL. Share **your own files plus a way to apply them**: an installer like this one, a script or written steps. Users then get SampQOL from the official link.
Hooking SampQOL's internal functions ties your add-on to one SampQOL version. Expect to update it whenever he releases a new one.

## Option B: make your own OS from scratch

You don't need SampQOL at all. Your own `autorun` folder can start anything you write.

The smallest working mod is the starter card in this repo ([starter-card/autorun](../starter-card/autorun)). It shows a picture on the screen and restarts the normal MPC app:

```text
autorun/
├── autorun.json     names the script
├── autorun.sh       your script (runs as root)
├── autorun.check    sign_autorun.py writes this
└── your files
```

From there, a full OS usually:
1. stops the stock app (`systemctl stop ac50.service`);
2. starts the app again (`/usr/bin/MPC Sample`) with your own libraries in `LD_PRELOAD`, hooking the app's functions;
3. **falls back to stock** if the app crashes twice in a row (`systemctl start ac50.service`). Never leave someone with a black screen.

Rules of thumb:
- Write your own code. Don't copy SampQOL's scripts or code into a project you share: that's his work, under his license.
- Stay in RAM (`/tmp`, `/run`). Never write to the MPC's system partitions.
- Check the firmware version before you hook anything, and exit quietly if it's not one you tested.
- Test on a spare SD card (or an emulator) before your main one.
- Akai Professional, MPC and MPC Sample are inMusic's trademarks. Don't put their logos in your splash or make your mod look official.

## Make your own splash

Any picture, converted to the screen's format (320 × 240, BGRX, 307,200 bytes):

```sh
python3 -m pip install Pillow
python3 tools/make_splash.py my-logo.png my-splash.raw --preview check.png
```

Open `check.png` to see exactly what the MPC will show.

## Share what you build

Open a [discussion or issue](https://github.com/Holla-Back-Entertainment/henryskillz-mpc-sample-mod-kit/issues) and show it off. Credit whoever's work yours builds on, the way this kit credits SampQOL and orrpunk-art.
