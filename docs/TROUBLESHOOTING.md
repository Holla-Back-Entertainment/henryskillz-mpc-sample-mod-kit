# Troubleshooting

## The MPC starts normally and no splash appears

- Confirm the folder is named exactly `autorun` and is at the top level of the SD card.
- Confirm `autorun.json`, `autorun.sh` and `autorun.check` are directly inside it.
- Use an exFAT card and safely eject it before putting it in the MPC.
- Confirm the MPC Sample is running firmware 1.3.0 / 1.3.0.12.
- Re-download the release rather than editing the files in Word or TextEdit.

## The MPC hangs or stays black

Hold power until it switches off, remove the SD card and boot stock. A forced shutdown can lose unsaved work, which is why projects must be backed up before testing.

If stock boot works, delete the test `autorun` folder from the card and start again from a fresh release. Do not merge files from different mods.

## The builder says it found zero or multiple autorun folders

Use the original release ZIP from the OS author, not a ZIP containing several versions or a whole Downloads folder.

## The builder rejects my splash

The raw file must be exactly 307,200 bytes: 320 × 240 pixels × four bytes per pixel in BGRX order. Use `tools/make_splash.py` to convert a normal PNG or JPEG.

## I need help

Open a GitHub issue and include:

- MPC model and exact firmware version;
- operating system (macOS or Windows);
- which kit/release version you used;
- the exact error message;
- a screenshot of the SD card folder layout.

Do not upload private keys, copyrighted OS ZIPs, device backups or project/sample files.
