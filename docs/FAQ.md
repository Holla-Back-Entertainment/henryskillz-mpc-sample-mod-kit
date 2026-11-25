# FAQ

### Can this break my MPC?
It doesn't change anything inside the MPC. Everything runs from the SD card and is gone when you switch off.
Without the card, the MPC starts exactly as stock. The real risk is to your projects and samples, which is why the guide starts with a backup.

### Does it void my warranty?
It's an unofficial mod. Akai and inMusic don't support it, and using it may affect their support.
Before you contact them about any problem, take the SD card out and check the problem also happens on a stock MPC.

### Is it free?
Yes, and it always will be. If you want to support the work, see [Support](../README.md#support-the-work). It's optional and unlocks nothing.

### Why do I have to download SampQOL separately?
SampQOL OS is Snare Bes's work and his license says it's only shared from his official page. So this kit never includes it.
You download it from him, and the installer adds the HenrySkillz files to your copy on your computer.

### Who do I ask for help?
**For anything about the HS effects or this installer, ask here:** [open an issue](https://github.com/Holla-Back-Entertainment/henryskillz-mpc-sample-mod-kit/issues).
Please **don't** take HenrySkillz problems to the SampQOL author or to Akai. To check whether a problem is ours or SampQOL's, try plain SampQOL (from its official zip) without these add-ons.

### The installer says "This SampQOL build is not supported"
You have a different version or a changed file. For the HS effects you need SampQOL 0.3.0: download `SampQOL-OS-v0.3.0.zip` again from the [official page](https://github.com/snarebes/SampQOL-OS/releases/tag/v0.3.0) and use that. (Newer 0.4.x copies are only packaged with the boot picker, without HS effects.)

### The installer says a file "was changed"
Something in that SampQOL copy was edited. Use a fresh download, not a folder that has been on an SD card.

### The installer says the add-ons are "already installed"
You gave it a folder it already patched. Start again from the original SampQOL zip.

### HS Saturator and HS Tube Strip aren't in the insert menu
On SampQOL 0.3.0 the menu fits seven of the nine HS effects. Hide SampQOL's two AIR effects (AIR Lo-Fi and AIR Pitch Shifter) in its settings and they come back. Projects that already use them open as normal.

### I have SampQOL 0.4.x (0.4.0, 0.4.1, 0.4.2). Do the HS effects work?
**Not yet.** The HS effects hook into exact spots inside SampQOL, and those spots moved in 0.4.x. Support for them is still being built and has not been tested on a real MPC Sample.
- **To use the HS effects today:** download SampQOL **0.3.0** from the [official page](https://github.com/snarebes/SampQOL-OS/releases/tag/v0.3.0) and use the installer with that.
- **If you want to stay on 0.4.x:** the installer will still package your copy with the D.S.B Boot picker. That package has no HS effects, and it is not an effects install.
- Already on 0.3.0 with the HS effects? Keep it there. Don't swap in a 0.4.x folder.
Watch this repository (**Watch > Custom > Releases**) to hear when 0.4.x gets HS effects.

### The MPC starts normally, with no SampQOL and no HS screen
The MPC skipped the card's `autorun` folder. Check:
- the folder is called exactly `autorun` and is at the top level of the card;
- nothing inside it was opened or edited;
- the card is exFAT and was ejected safely.
If it still happens, run the installer again from a fresh SampQOL zip.

### The screen goes black or the MPC freezes
Hold the power button until it switches off, take the card out and switch on. It's stock again.
Then [open an issue](https://github.com/Holla-Back-Entertainment/henryskillz-mpc-sample-mod-kit/issues) with the details from [RECOVERY.md](RECOVERY.md#report-a-problem).

### The app restarted twice and came back as stock
That's SampQOL's safety net: after two crashes in a row it starts the stock app. Remove the card and report it.

### Does it work on the MPC One, Live, Key or X?
No. This kit is for the **MPC Sample** on firmware 1.3.0 only.

### Do I need the starter zip?
No. The `henryskillz-mpc-sample-starter` zip is an optional test card. It only shows the HenrySkillz screen and then starts the normal MPC. **It has no effects.**
Use it if you want to check your card works before installing.

### Is my data sent anywhere?
No. The installer page runs in your browser and never uploads your files. The online version only loads one helper library (JSZip, for zip files) from cdnjs.
