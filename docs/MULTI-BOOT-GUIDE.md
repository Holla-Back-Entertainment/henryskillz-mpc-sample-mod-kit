# Multi-boot guide: more than one OS on your MPC Sample

Use this if your card already runs another OS, or you want several on one card. **D.S.B Boot** is the menu: pick an OS at power-on.

**Before you start:** back up the whole SD card. Firmware must be 1.3.0 (1.3.0.12). Card must be exFAT.

## What the card should look like

**Right:**

```text
SD CARD
├── autorun/                  <- D.S.B Boot menu (the only autorun at the top)
│   ├── autorun.json
│   ├── autorun.sh
│   ├── boot/
│   └── os/                   <- your OSes go here
│       ├── SampQOL-0.3.0/    <- one folder per OS
│       │   ├── autorun.json  <- must be right here
│       │   ├── autorun.sh
│       │   └── ...
│       └── MPC-Simple/
│           ├── autorun.json
│           └── ...
└── HS-OS/
```

**Wrong:**

```text
X  autorun/os/SampQOL-0.3.0/autorun/autorun.json     too deep: autorun.json must be directly inside SampQOL-0.3.0/
X  os/SampQOL-0.3.0/                                 os/ must be inside autorun/
X  two autorun folders merged into one               rename the old one to autorun-old first
X  folder named stock or hs-fx                       reserved names
```

## A. Fresh card

1. Format the card exFAT.
2. Download `dsb-boot-card.zip` from the [D.S.B Boot release](https://github.com/Holla-Back-Entertainment/henryskillz-mpc-sample-mod-kit/releases/tag/dsb-boot-v1.0.1). Unzip it onto the top of the card.
3. Download each OS from its developer's official page and unzip it.
4. Copy each OS's `autorun` folder into `autorun/os/` on the card and rename it (for example `SampQOL-0.3.0`).
5. Check `autorun.json` is directly inside each OS folder.
6. Eject, switch on, pick an OS.

## B. Card already has one OS

```text
BEFORE                    AFTER
SD CARD                   SD CARD
└── autorun/  (old OS)    ├── autorun/          <- new menu
                          │   └── os/
                          │       └── SampQOL-0.3.0/   <- copy of your old OS
                          └── autorun-old/      <- your old autorun. Keep it.
```

1. Back up the card.
2. Rename the top-level `autorun` to `autorun-old`.
3. Unzip `dsb-boot-card.zip` onto the top of the card.
4. Copy the whole `autorun-old` folder into `autorun/os/` and rename the copy (for example `SampQOL-0.3.0`). Leave `autorun-old` where it is.
5. Eject, switch on, pick your OS.

## C. Card already has two or more OSes (SampleBoot, Boot Thingy, or your own folders)

Not tested. Try it on a spare card first.

1. Back up the card. Rename its `autorun` to `autorun-old`.
2. Unzip `dsb-boot-card.zip` onto the top of the card.
3. For each OS, copy its original `autorun` folder from its developer into `autorun/os/` and rename it.
4. Pick each one once to check it boots.

## D. Already have D.S.B Boot

- **Add an OS:** copy its `autorun` folder into `autorun/os/` and rename it.
- **Update an OS:** replace its folder in `autorun/os/`.
- **Update the menu:** copy `autorun/os/` to your computer first, swap in the new `autorun`, then check your OS folders are still in `autorun/os/`.

## HS effects

The effects only work on **SampQOL 0.3.0**, not 0.4.x. Using them inside `autorun/os/` has not been tested yet: try it on a spare card first.

## Undo, and getting out

- **Back to your old setup:** delete the new `autorun`, rename `autorun-old` back to `autorun`.
- **Back to stock:** take the SD card out.
- **Back to the menu from inside an OS:** hold **SHIFT + STOP** for 2 seconds.
- **Stuck?** [Report a problem](https://github.com/Holla-Back-Entertainment/henryskillz-mpc-sample-mod-kit/issues).
