# HenrySkillz MPC Sample Mod Kit

Studio effects for the **Akai MPC Sample**, made by Henry Skillz in Melbourne.
They run on top of [SampQOL OS](https://github.com/snarebes/SampQOL-OS) by Snare Bes.

**No coding, no terminal, no AI tools, nothing to install.** You need a web browser and an SD card.

> Unofficial. Not made, endorsed or supported by Akai Professional, inMusic or the SampQOL author.
> Everything runs from the SD card. Take the card out and the MPC starts as stock.

**Status: beta.** The installer is tested against real SampQOL packages on a computer, and the effects have run on Henry's own MPC Sample.
This exact v0.2.0 build has not had its first boot on hardware yet. Back up before you try it.

## What you get

| Effect | Where | What it does |
|---|---|---|
| HS Saturator | Harmonic | Saturation for drums and bass |
| HS Air Exciter | Harmonic | Adds brightness to the top end |
| HS Channel Strip | Harmonic | Console-style channel: filters, 4-band EQ, compressor, gate |
| HS Drive Sat | Harmonic | Saturation with drive and shape controls |
| HS Bus Compressor | Dynamics | Bus compressor with a sidechain high-pass |
| HS Para EQ | EQ / Filter | Parametric EQ with a response-curve display |

You also get live displays for the stock effects, a dB/Hz grid on the EQ page, and a HenrySkillz screen at start-up (after SampQOL's own).

## Before you start

- An **MPC Sample** on firmware **1.3.0** (1.3.0.12).
- An **exFAT SD card**.
- A computer with a web browser (Mac or Windows).
- **A backup.** Copy everything on the SD card to your computer first. See [docs/RECOVERY.md](docs/RECOVERY.md).

## Install (5 steps)

1. **Get SampQOL 0.2.0.** Download `SampQOL-OS-v0.2.0.zip` from the [official SampQOL page](https://github.com/snarebes/SampQOL-OS/releases).
   The installer only accepts that exact version.
2. **Get the installer.** Download `henryskillz-mpc-sample-installer.html` from [Releases](https://github.com/hmukhwana/henryskillz-mpc-sample-mod-kit/releases/latest).
3. **Open the installer** by double-clicking it (it opens in your browser). Drop the SampQOL zip onto the page.
   If your Mac already unzipped the download, click **Choose folder** and pick the `SampQOL-OS-v0.2.0` folder.
4. **Save `autorun-hs.zip`** when the page offers it, then unzip it. You now have a folder called `autorun`.
5. **Copy that `autorun` folder to the top level of the SD card**, replacing any old one. Eject the card safely, put it in the MPC and switch it on.
   When it asks *Start SampQOL OS?*, press **B1**.

The new effects show up wherever you pick an insert effect. Their names start with **HS**.

**Why it works this way:** SampQOL's license says its files can't be shared, so this kit doesn't include them.
You download SampQOL yourself, and the installer adds the HenrySkillz files to your copy, inside your browser. Nothing is uploaded.

## Optional: boot test first

Not sure your card and MPC are set up right? The `henryskillz-mpc-sample-starter` zip on the Releases page is a tiny test card.
**It only shows the HenrySkillz screen for two seconds, then starts the normal MPC. It does not include the effects.**
Copy its `autorun` folder to the card, boot, and if you see the screen, your card works. Then do the install above.

## If something goes wrong

- **MPC boots to stock:** the card's `autorun` folder didn't pass its check. Run the installer again on a fresh SampQOL download.
- **The app restarts twice and comes back stock:** that's SampQOL's safety net. Delete the `autorun` folder from the card and [open an issue](https://github.com/hmukhwana/henryskillz-mpc-sample-mod-kit/issues).
- **Anything else:** switch off and take the SD card out. The MPC is stock without it. Full steps: [docs/RECOVERY.md](docs/RECOVERY.md) and [docs/TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md).

Don't contact Akai or inMusic support about this mod.

## Crate Source

Digging for samples? **[Crate Source](https://crate-source.app)** is Henry's crate-digging app: find a record, chop it, and load it onto your MPC.
This kit doesn't include it or teach how it's built.

## Support the work

If this saved you time, support it by shopping [Spacely](https://www.spacely.clothing/) or visiting [HenrySkillz](https://www.henryskillz.com/).
The kit is free and always will be. Support is optional.

## For builders

How it works, how to build it and how to add a new SampQOL version: [docs/HOW-IT-WORKS.md](docs/HOW-IT-WORKS.md).
The `tools/` folder has command-line helpers for signing an `autorun` folder and making your own splash.

## Credits

The effects, displays, installer and tooling are by **Henry Skillz**. This kit only exists because of SampQOL OS by Snare Bes.
See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

Released under the [MIT License](LICENSE). SampQOL OS has its own license and is not part of this repository.
