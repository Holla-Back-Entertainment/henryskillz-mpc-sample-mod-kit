# Compatibility and endorsement status

This page separates what has been tested from what might work. “Tested” means the named combination was actually built and run. It does not mean its developer endorses this kit.

| Environment | Status | What works | Evidence / limit |
|---|---|---|---|
| MPC Sample firmware 1.3.0.12 + SampQOL 0.2.0 | **Emulator tested** | Full installer, effects, displays, presets, chains, browser tags, mixer and optional stems | The public installer is tested against the pristine SampQOL 0.2.0 package and booted in the virtual MPC. The last recorded physical-device pass was v0.5; run the [v0.6+ hardware checklist](BOOT-TEST.md) before calling the current build hardware tested. |
| MPC Sample firmware 1.3.0.12, stock app | **Emulator tested: HS Tools only** | SEQ desk/meters, browser key/BPM and optional stems | Use the separate HS Tools release. The LADSPA effects and track chains require SampQOL. |
| MPC Boot Thingy | **Not packaged or tested yet** | Choose-at-boot may be possible | Do not describe this kit as listed or compatible until its package has been accepted and boot-tested. |
| Gridzilla | **Not tested** | Unknown | Do not claim co-loading. A launcher integration needs its developer's agreement and a real boot test. |
| AM Mod, KAOSAMPLE, mpc-minimod | **Not tested** | HS Tools may be portable; full effects are unknown | Each project needs a separate integration and test. Open source does not by itself prove runtime compatibility. |
| SXL or any replacement MPC application | **Not tested** | Unknown | These hooks target the stock MPC Sample application and exact SampQOL 0.2.0 offsets. Treat replacement applications as unsupported until proven otherwise. |
| New Akai firmware or a SampQOL version other than 0.2.0 | **Unsupported until retested** | Unknown | The installer rejects unknown SampQOL binaries. Never add a version hash without the checks in [How it works](HOW-IT-WORKS.md#supporting-a-new-sampqol-version). |

## Language to use publicly

- **Compatible / tested:** only after the relevant row has a completed test record.
- **Endorsed:** only after that project's developer explicitly approves that wording.
- **Available through Boot Thingy:** only after the listing or package is live.
- **May be portable:** acceptable for an untested engineering possibility, with the limitation beside it.

The project is currently unofficial and unendorsed. Technical compatibility and a developer's approval are separate claims.

## Release gate

For a release that changes firmware hooks, LADSPA registration, startup order or screen drawing:

1. Build from a clean checkout and run the automated tests.
2. Patch a pristine official SampQOL 0.2.0 package with the generated installer.
3. Boot that exact output in the virtual MPC and complete the affected feature checks.
4. Complete [BOOT-TEST.md](BOOT-TEST.md) on a spare card and record the commit.
5. Publish compatibility wording that matches the completed evidence; contact other developers in parallel, without implying their endorsement.
