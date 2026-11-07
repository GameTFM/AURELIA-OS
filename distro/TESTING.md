# AURELIA testing plan and status

## Current status

| Check | Status |
|---|---|
| Static validator | **PASS** (see [VERIFICATION.md](VERIFICATION.md)) |
| Ubuntu `resolute` package names | **PASS** (107/107 found in archive indexes) |
| Original artwork generated | **PASS** (verified dimensions in validator) |
| legacy AURELIA kernel build | **PASS** (not part of the Ubuntu image) |
| live-build ISO | **PASS** in GitHub Actions (`ubuntu:26.04`) |
| QEMU live boot | **PASS** under BIOS and UEFI; see [VERIFICATION.md](VERIFICATION.md) |
| Plasma runtime/QML render | **PASS** for the live desktop; first-login flow still unrun |
| Calamares install | **NOT RUN** |
| Installed GRUB/SDDM/SDDM autologin cleanup | **NOT RUN** |

The static checks cannot prove a QML component renders, a Plasma applet loads,
Calamares writes a disk correctly, or GRUB boots the installed system.

## 1. Static checks

```bash
cd AURELIA-OS/distro
python3 scripts/validate.py
```

It covers:

- JSON parsing for applet, global-theme and wallpaper metadata;
- YAML parsing for every Calamares settings/module/branding file;
- INI parsing for KDE, SDDM and `.desktop` files;
- `bash -n` for live-build entrypoints, hooks and helpers;
- Python compilation for artwork/validation/build tooling plus the NOVAIR
  and AURELIA Setup payloads;
- shell syntax for the NOVAIR/setup launchers and the installed-system guard;
- `node --check` for the Plasma layout;
- QML delimiter validation for the clock, calendar and SDDM greeter;
- required payload and non-empty image checks, including NOVAIR's Qt
  WebEngine source, its icon and the setup autostart guard;
- Calamares 3.3 `style` map, required settings keys, `unpackfsc` flat schema and
  required install steps;
- package-name checks against the downloaded `resolute` indexes when present;
- the lean package policy: no forbidden packages, no duplicate entries and
  recommends disabled.

## 2. Build test

On Ubuntu 26.04:

```bash
cd AURELIA-OS/distro
sudo scripts/install-build-deps.sh
sudo scripts/build-iso.sh
```

Expected:

- `lb config` accepts the profile;
- `lb build` completes and creates `live-image-amd64.hybrid.iso`;
- the ISO is >0 bytes and a SHA-256 file exists;
- `build/live-build.log` contains no `Hook ... failed`, `E:`, or Calamares
  configuration warnings;
- `xorriso -indev ... -toc` shows a bootable El Torito image.

If the build fails, inspect the first error in the log before changing the
profile. Do not remove validation or hooks merely to make the build pass.

## 3. Live boot test

```bash
cd AURELIA-OS/distro
scripts/test-iso.sh
```

This boots the ISO in QEMU (KVM if available, software otherwise). The script
uses a BIOS CD-ROM boot; UEFI needs OVMF firmware and a separate invocation.

Verify:

1. GRUB/Casper reaches the live session.
2. SDDM auto-logs in as `aurelia` on the Plasma Wayland session.
3. Wallpaper, top bar, large clock, left calendar, right Notes and floating dock
   are present.
4. The clock changes and shows the current local date.
5. Calendar arrows change the month and today is highlighted only in the current
   month.
6. Kickoff opens, shows pinned applications, searches, launches Dolphin/Konsole,
   and exposes working power/session actions.
7. Notes accepts text; Media Controller is empty until a real MPRIS player
   starts; Elisa playback appears if an audio sink exists.
8. Open apps show real windows; minimize/maximize/close work in KWin.
9. Flatpak setup either registers Flathub or fails visibly without hanging.
10. The installer launcher opens Calamares on Wayland; no black screen or
    "Failed to connect to Wayland display".
11. `snapd` is absent and Discover exposes only the APT and Flatpak sources.

## 4. Installer test

Use a disposable QEMU disk, not a host disk:

```bash
qemu-img create -f qcow2 /tmp/aurelia-test.qcow2 32G
qemu-system-x86_64 -enable-kvm -m 4096 -smp 2 -machine q35 \
  -drive file=/tmp/aurelia-test.qcow2,if=virtio \
  -cdrom live-image-amd64.hybrid.iso -boot d
```

Verify:

- guided/erase and manual partition paths are usable;
- EFI partition is created on UEFI installs;
- `unpackfsc` completes without rsync/xattr errors;
- Calamares creates the chosen user and password;
- installed initramfs updates without live-config diversions;
- live-only autologin, `/etc/casper.conf`, installer launcher and Flathub
  autostart state are correct for an installed system;
- GRUB installs and boots the new system;
- first login applies the AURELIA layout once;
- installed user is not auto-logged-in;
- `sudo` works for the created user, root login is disabled;
- Flatpak remote exists or the setup helper fails cleanly after prompting.

## 5. Visual review

The references are target compositions. Check the actual rendered desktop at
1920×1080 and at a second logical resolution:

- clock/weekday dominance and legibility against the wallpaper;
- no overlap between clock, calendar, notes, media card, dock and top bar;
- panel transparency and floating dock offsets;
- contrast of AURELIA color scheme in Kirigami/Qt apps and system dialogs;
- SDDM login fields at 1080p;
- Calamares sidebar/readability with the custom stylesheet;
- GRUB theme text on the background.

## 6. Clean-install regression checks

- Create a note, reboot, confirm persistence.
- Start media in Elisa, confirm Media Controller shows it.
- Change wallpaper/panel color, reboot, confirm the first-login helper does not
  reapply and overwrite the change.
- Delete the layout marker and rerun the helper manually to confirm recovery.
- Test a non-US keyboard layout in Calamares.
- Test UEFI and BIOS installs separately.
- Test with no network: Flatpak setup should not hang or block login.
- On the installed system, confirm AURELIA Setup appears once after the first
  login, applies the chosen KDE settings, and does not appear again after
  finishing. Confirm the live ISO never launches it.
- Launch NOVAIR, open two tabs, use the new-tab page, add/remove a bookmark,
  inspect history, start a download, open a private window, change a setting
  and reload the browser.
- Confirm the top-bar launcher, weather widget and system monitor load without
  QML errors; the weather widget should show its real empty state until a
  location is configured.

## CI

The GitHub Actions workflow builds the ISO inside `ubuntu:26.04` with
`--privileged` for live-build's loop mounts. It uploads the ISO, checksum and
log for seven days. Completed runs produce a roughly 2.4 GB hybrid image. The
downloaded artifact has been booted in QEMU under both BIOS and UEFI, and the
live Plasma desktop was captured; see [VERIFICATION.md](VERIFICATION.md). The
Calamares install and installed-system boot are still open tests.
