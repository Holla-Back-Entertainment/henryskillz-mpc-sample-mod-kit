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

### The installer says "This is not SampQOL 0.2.0"
You have a different version or a changed file. Download `SampQOL-OS-v0.2.0.zip` again from the [official page](https://github.com/snarebes/SampQOL-OS/releases) and use that.

### The installer says a file "was changed"
Something in that SampQOL copy was edited. Use a fresh download, not a folder that has been on an SD card.

### The installer says the add-ons are "already installed"
You gave it a folder it already patched. Start again from the original SampQOL zip.

### SampQOL released a new version. Can I use it?
Not until this kit is updated for it. Every new SampQOL version is checked and tested on a real MPC Sample before the installer accepts it.
Keep using 0.2.0 with the HS effects, or plain new SampQOL without them. Watch this repository (**Watch > Custom > Releases**) to hear when it's updated.

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
