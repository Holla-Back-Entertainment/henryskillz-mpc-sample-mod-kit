# HenrySkillz MPC Sample Mod Kit

Beginner-friendly, no-AI tools for personalising an **Akai MPC Sample** from an SD card.

Built from the hands-on experiments documented by Henry Skillz. Nothing here flashes the MPC or replaces its internal firmware. Remove the SD card and the MPC starts normally.

> Tested on one MPC Sample running firmware **1.3.0 / 1.3.0.12**. Back up your projects first. This is unofficial community software, not affiliated with or supported by Akai Professional or inMusic Brands.

## Pick the easy path

### A. HenrySkillz Starter — copy it and boot

Use this if you want the simplest possible test.

1. Download `henryskillz-mpc-sample-starter-v0.1.0.zip` from [Releases](https://github.com/hmukhwana/henryskillz-mpc-sample-mod-kit/releases/latest).
2. Unzip it.
3. Put an **exFAT** SD card in your computer.
4. Back up any existing folder named `autorun` on that card.
5. Copy the included `autorun` folder to the top level of the SD card.
6. Safely eject the card, put it in the MPC Sample and power on.

You should briefly see the HenrySkillz screen, then the normal MPC software. To return to stock, switch off normally and remove the card (or rename `autorun` to `autorun-off`).

The card should look exactly like this:

```text
SD card/
└── autorun/
    ├── autorun.check
    ├── autorun.json
    ├── autorun.sh
    └── henryskillz-splash.raw
```

### B. Bring your own SampQOL (or another autorun OS)

This repository does **not** redistribute SampQOL OS. Its license says to download it from the official source and share the link, not the files.

1. Download the official OS ZIP yourself. For SampQOL, use [the official SampQOL releases](https://github.com/snarebes/SampQOL-OS/releases).
2. Download this repository and unzip it.
3. Double-click `BUILD-MAC.command` on macOS or `BUILD-WINDOWS.cmd` on Windows.
4. Choose the official OS ZIP when asked.
5. The tool creates `HenrySkillz-Card/autorun` beside the ZIP.
6. Copy that new `autorun` folder to the top level of your exFAT SD card.

The builder runs entirely on your computer. It does not need Claude, Codex, a GitHub account or an internet connection after you have downloaded the two ZIP files. It never edits the original OS ZIP. For SampQOL it applies the included HenrySkillz splash automatically; advanced users can pass `--keep-original-splash` instead.

If the OS contains a file named `qol-splash.raw`, the builder can replace it with a 320×240 BGRX `.raw` splash and updates its SHA-256 pin. For other/future autorun packages it preserves the package and regenerates the MPC firmware check. Package-specific changes may still need a new adapter—open an issue with the package name and public download link.

## Make your own splash

The MPC Sample splash format is exactly **320×240, BGRX, 307,200 bytes**. If Python and Pillow are installed:

```sh
python3 -m pip install Pillow
python3 tools/make_splash.py your-image.png your-splash.raw
python3 tools/easy_builder.py path/to/official-os.zip --splash your-splash.raw
```

The image is fitted inside the screen without stretching and padded with black.

## If it does not boot

1. Hold the power button until the unit switches off.
2. Remove the SD card.
3. Boot normally and confirm the stock MPC still works.
4. Check the firmware version, folder layout and that the card is exFAT.
5. Do not merge two different `autorun` folders.

See [Troubleshooting](docs/TROUBLESHOOTING.md) before opening an issue.

## What is and is not included

- Included: HenrySkillz starter scripts/art, a local card builder, signature/check tooling, tests and plain-English instructions.
- Not included: Akai firmware, SampQOL binaries, Crate Source code, private SSH keys, device backups or paid/proprietary material.
- Crate Source may be shown in tutorials as part of Henry's workflow, but this kit does not teach, bundle or expose its internals.

## Support the work

If this saved you time, visit [HenrySkillz](https://www.henryskillz.com/) or support the wider project by shopping [Spacely](https://www.spacely.clothing/). Direct-donation options can be added when a public payment link is ready.

## Credits and attribution

The packaging, beginner workflow, starter splash and tooling in this repository are by **Henry Skillz**. Community research made this possible. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for clear credit and links.

Released under the [MIT License](LICENSE). Third-party software keeps its own license.
