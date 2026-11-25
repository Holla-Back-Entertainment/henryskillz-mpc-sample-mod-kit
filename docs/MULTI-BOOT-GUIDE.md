# Already running another OS? Or want more than one? (D.S.B Boot guide)

For MPC Sample owners who **already have a custom OS on their SD card** (SampQOL, MPC Simple, or anything else), who want **two or more operating systems on one card**, or who are setting up a **fresh card** with more than one.

**D.S.B Boot** is the picker. At power-on it shows a menu: each OS is a folder on the card, and you choose one. Take the card out and the Sample is stock again.

Follow the steps for your case. Don't skip the backup.

---

## Read this first

1. **Back up the whole SD card** to your computer before you change anything. A failed project load can lose samples.
2. **Firmware 1.3.0 (1.3.0.12) only.** On any other firmware the picker steps aside and the Sample starts stock.
3. **One `autorun` folder at the top of the card.** The Sample reads that one only. With D.S.B Boot, that folder is the **picker**, and your OSes sit **inside** it, in `autorun/os/`.
4. **Never extract the D.S.B Boot zip on top of a card that already has an `autorun`.** Rename the old one first (steps below). Renaming is your way back.
5. **The card must be exFAT.** Most are.
6. **Don't put one boot loader inside another.** A loader such as SampleBoot or Boot Thingy and D.S.B Boot do the same job. Use one per card.

---

## What the picker can run

Put each OS in its own folder: `autorun/os/<Folder>/`. Two kinds work:

| Kind | How to add it |
|---|---|
| **An OS's own release `autorun` folder, unchanged** (it contains `autorun.json`) | Copy that folder into `autorun/os/` and rename it, for example `SampQOL-0.3.0`. Don't edit anything inside. |
| **An HS Boot folder** (`os.json` plus a start script) | For OS authors. See `autorun/os/README.txt` on the card. |

Good to know:
- Folders show in name order, up to 37.
- Folder names `hs-akai`, `hs-fx` and `stock` are reserved.
- **Stock** is always in the menu. Pick it to run the normal MPC.
- Each OS keeps its own checks. If an OS's files don't match its own signature, or the firmware isn't the tested one, the picker starts stock instead of that OS.
- **Back to the menu from inside an OS:** hold **SHIFT + STOP** for about 2 seconds (for OSes with no way out of their own).
- Optional menu art and names: add `dsb-record.png` and `dsb-record.txt` to an OS folder. The details are in `autorun/os/README.txt`.

---

## What the card should look like

**Right: the picker at the top, each OS in its own folder inside `autorun/os/`.**

```text
SD CARD (top level)
├── autorun/                    <- the D.S.B Boot picker. The only autorun at the top.
│   ├── autorun.json
│   ├── autorun.sh
│   ├── autorun.check
│   ├── dsb-boot.conf
│   ├── boot/                   <- picker files. Don't touch.
│   ├── hs-fx/                  <- picker files. Don't touch.
│   └── os/                     <- YOUR OSes go here, one folder each
│       ├── README.txt
│       ├── SampQOL-0.3.0/      <- a complete release "autorun" folder, renamed
│       │   ├── autorun.json    <- must be here, directly inside
│       │   ├── autorun.sh
│       │   ├── autorun.check
│       │   ├── qol-run.sh
│       │   ├── mpcqol.so
│       │   └── ...             <- the rest of that release, unchanged
│       └── MPC-Simple/         <- another OS, same idea
│           ├── autorun.json
│           └── ...
├── HS-OS/
│   └── Templates/
└── (your MPC projects and samples)
```

**Before and after (Case B: one OS already on the card).**

```text
BEFORE                          AFTER
SD CARD                         SD CARD
└── autorun/    <- old OS       ├── autorun/            <- new: the picker
                                │   └── os/
                                │       └── SampQOL-0.3.0/   <- a COPY of your old OS
                                │           └── autorun.json ...
                                └── autorun-old/        <- your old autorun, renamed. Backup.
```

**Wrong. These all fail:**

```text
X  autorun/os/SampQOL-0.3.0/autorun/autorun.json     one folder too deep.
                                                     autorun.json must sit directly in SampQOL-0.3.0/.
X  autorun/os/SampQOL-OS-v0.3.0/SampQOL-0.3.0/...    you copied the zip's outer folder, not the autorun folder inside it.
X  os/SampQOL-0.3.0/                                 os/ at the card's top level. It must be inside autorun/.
X  autorun/ (picker) + autorun/ (old OS) merged      two OSes mixed into one folder. Rename the old one to
                                                     autorun-old first.
X  autorun/os/stock/   or   autorun/os/hs-fx/        reserved names. Pick another folder name.
```

**Quick check before you eject the card:** open `autorun/os/<your folder>/` and make sure you can see `autorun.json` right there, without opening another folder.

---

## Case A. Fresh card, one or more OSes

1. Get a fresh **exFAT** SD card.
2. Download the **D.S.B Boot card** (`dsb-boot-card.zip`) from the [D.S.B Boot release](https://github.com/Holla-Back-Entertainment/henryskillz-mpc-sample-mod-kit/releases/tag/dsb-boot-v1.0.1). Unzip it onto the **top level** of the card, so the card has an `autorun` folder.
3. Download each OS **from its own developer's official page** on your computer and unzip it. For SampQOL that is [Snare Bes's releases](https://github.com/snarebes/SampQOL-OS/releases).
4. For each OS, copy its **complete `autorun` folder** into the card's `autorun/os/` folder and rename the copy (`SampQOL-0.4.2`, `MPC-Simple`, and so on). Check that `autorun.json` is inside each one.
5. Eject the card, put it in the Sample, switch on, and pick an OS in the menu.

> Shortcut for SampQOL: the [installer page](https://holla-back-entertainment.github.io/henryskillz-mpc-sample-mod-kit/install/) can package a SampQOL zip together with D.S.B Boot for you. Extract that download to a **fresh** card.

---

## Case B. I have one custom OS on my card already (made by someone else)

Example: SampQOL, MPC Simple, or HS Tools on its own.

1. **Back up the card.**
2. On the card, find the top-level **`autorun`** folder. That is your current OS.
3. Rename it **`autorun-old`**. Don't delete it.
4. Download `dsb-boot-card.zip` and unzip it onto the card's top level. You now have a new `autorun` (the picker).
5. Move your old OS into the picker:
   - Open `autorun-old` and check it contains `autorun.json`.
   - Copy the **whole folder** into `autorun/os/` and rename it to the OS name (`SampQOL-0.3.0`, say).
   - **Leave `autorun-old` where it is** as your backup until you have booted it successfully.
6. Add any other OS the same way (Case A, steps 3 and 4).
7. Eject, switch on, pick your OS.

**To undo:** switch off, take the card out, delete the new `autorun` folder, and rename `autorun-old` back to `autorun`. You're back to exactly what you had.

---

## Case C. My card already has two or more OSes under another boot loader

Examples: SampleBoot, Boot Thingy, or your own folders.

D.S.B Boot follows the same idea (one OS per folder), but **we have not tested moving a card from another loader.** Do it on a spare card first.

1. Back up the card.
2. Keep your current loader's `autorun` as `autorun-old` (rename it).
3. Install D.S.B Boot as in Case B, step 4.
4. For each OS you had, copy **its own release `autorun` folder** (the one from its developer) into `autorun/os/`. Don't copy the other loader's wrapper files. If an OS only works inside that other loader, leave it there and use that loader on its own card.
5. Pick each OS once and check it boots.

---

## Case D. I already have D.S.B Boot

- **Add an OS:** copy its release `autorun` folder into `autorun/os/` and rename it. No reinstall.
- **Update the picker only:** first copy your `autorun/os/` folder to your computer as a backup. Then swap in the new `autorun` from the release. The release notes say your `os/` folders stay as they are, but check they are still in `autorun/os/` before you boot, and copy them back from your backup if not.
- **Update an OS:** replace its folder in `autorun/os/` with the new release's `autorun` folder, renamed the same way.

---

## The HS effects on a multi-boot card

The HS effects only work on **SampQOL 0.3.0** (the installer adds them). They do **not** work on SampQOL 0.4.x yet.

- **Verified:** the installer's SampQOL 0.3.0 output, used **by itself** as the card's `autorun`, passed a cold boot on a real MPC Sample (18 Nov 2026).
- **Not verified:** putting that installer output **inside** `autorun/os/` under D.S.B Boot. It should be treated as a spare-card experiment until a boot test is logged.
- **Not verified:** any HS effects on SampQOL 0.4.x, with or without the picker.
- If you want both the effects and a picker today, test on a spare card first, and keep your single-OS card as it is.

---

## Not tested

We have not tested these with D.S.B Boot, so don't expect them to work: Gridzilla, SXL, AM Mod, KAOSAMPLE, and any OS that replaces the MPC app. See [COMPATIBILITY.md](COMPATIBILITY.md).

---

## If something goes wrong

| What you see | Do this |
|---|---|
| The Sample starts **stock**, no menu | The firmware isn't 1.3.0.12, or the `autorun` folder isn't at the **top** of the card, or the card isn't exFAT. |
| The menu shows, but an OS isn't listed | Its folder isn't in `autorun/os/`, or it has no `autorun.json`. Check it's the **release `autorun` folder**, not the zip's outer folder. |
| You pick an OS and it drops to **stock** | The OS's files were edited, or its own checks failed. Use a fresh copy from its developer. |
| "SD card is busy" in SD Card Access | Use the current D.S.B Boot release (v1.0.1 fixed this) and pick the OS before you open SD Card Access. |
| You're stuck inside an OS | Hold **SHIFT + STOP** for 2 seconds. If that doesn't work, switch off and take the card out. |
| Anything else | Switch off, take the card out: the Sample is stock. Then see [RECOVERY.md](RECOVERY.md) or [open an issue](https://github.com/Holla-Back-Entertainment/henryskillz-mpc-sample-mod-kit/issues). |

Please take problems with another developer's OS to that developer, and problems with the picker or the HS effects to our issues page.
