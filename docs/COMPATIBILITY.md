# Compatibility and endorsement status

This page separates what has been tested from what might work. “Tested” means the named combination was actually built and run. It does not mean its developer endorses this kit.

| Environment | Status | What works | Evidence / limit |
|---|---|---|---|
| MPC Sample firmware 1.3.0.12 + SampQOL 0.2.0 | **Hardware tested (v0.7.0); no longer offered by the installer** | Full installer, effects, displays, presets, chains, browser tags, mixer and optional stems | v0.7.0 passed on a real MPC Sample on 15 Nov 2026: effect editors and mixer during playback, chains, browser tags, Stems, M/S, a project with the stock Bus Compressor, and a power-off cold boot. The installer is also tested against the pristine SampQOL 0.2.0 package and booted in the virtual MPC. |
| MPC Sample firmware 1.3.0.12 + SampQOL 0.3.0 | **Supported (latest). Installer tested offline, hardware pending** | Installer accepts only this version | The installer patches the pristine official 0.3.0 package (pins, signature, splash frames checked offline). Not yet booted on a real MPC Sample. |
| MPC Sample firmware 1.3.0.12, stock app | **HS Tools: hardware tested** | SEQ desk/meters, browser key/BPM and optional stems | The v0.6.1 HS Tools card booted on a real MPC Sample on 15 Nov 2026, and its SEQ desk, meters, browser tags and Stems all worked. Use the separate HS Tools release. The LADSPA effects and track chains require SampQOL. |
| MPC Boot Thingy v1.1.6 | **HS Tools (no Stems): hardware tested via zip install** | SEQ desk/meters and browser key/BPM tags | On a real MPC Sample on 15 Nov 2026, `henryskillz-mpc-sample-hs-tools-boot-thingy.zip` dropped in `autorun/apps/` installed itself at power-on and ran from the menu with every tool working. Stems is left out: Boot Thingy caps an app at 64 MiB and a file at 32 MiB, and the stems model is 79 MB. The full kit with Stems also booted when copied by hand into `autorun/apps/<name>/autorun/`, but it needs the user's own SampQOL files, so it is not packaged. Not listed yet: do not describe it as available through Boot Thingy. |
| Gridzilla | **Not tested** | Unknown | Do not claim co-loading. A launcher integration needs its developer's agreement and a real boot test. |
| AM Mod, KAOSAMPLE, mpc-minimod | **Not tested** | HS Tools may be portable; full effects are unknown | Each project needs a separate integration and test. Open source does not by itself prove runtime compatibility. |
| SXL or any replacement MPC application | **Not tested** | Unknown | These hooks target the stock MPC Sample application and exact SampQOL offsets. Treat replacement applications as unsupported until proven otherwise. |
| New Akai firmware or a SampQOL version other than 0.3.0 | **Unsupported until retested** | Unknown | The installer rejects unknown SampQOL binaries. Never add a version hash without the checks in [How it works](HOW-IT-WORKS.md#supporting-a-new-sampqol-version). |

## Language to use publicly

- **Compatible / tested:** only after the relevant row has a completed test record.
- **Endorsed:** only after that project's developer explicitly approves that wording.
- **Available through Boot Thingy:** only after the listing or package is live.
- **May be portable:** acceptable for an untested engineering possibility, with the limitation beside it.

The project is currently unofficial and unendorsed. Technical compatibility and a developer's approval are separate claims.

## Release gate

For a release that changes firmware hooks, LADSPA registration, startup order or screen drawing:

1. Build from a clean checkout and run the automated tests.
2. Patch each pristine official SampQOL package the installer accepts (0.3.0) with the generated installer.
3. Boot that exact output in the virtual MPC and complete the affected feature checks.
4. Complete [BOOT-TEST.md](BOOT-TEST.md) on a spare card and record the commit.
5. Publish compatibility wording that matches the completed evidence; contact other developers in parallel, without implying their endorsement.
