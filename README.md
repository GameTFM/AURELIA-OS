# AURELIA OS 1.0 “Solstice”

**Repository:** [GameTFM/AURELIA-OS](https://github.com/GameTFM/AURELIA-OS)

AURELIA OS is an original **Ubuntu 26.04 LTS-based desktop distribution** with
KDE Plasma 6.6 on Wayland, a centered Solstice clock and calendar, a thin top
bar, a clean floating dock, and a branded SDDM/Calamares/GRUB path. It uses
APT and Flatpak, GRUB, the Ubuntu Linux kernel, and live-build/Casper.

The earlier bespoke AURELIA kernel and software desktop are preserved as the
legacy research track. They are not the base of the current distribution.

> **Status: the Ubuntu/KDE image builds in CI and boots to the live Plasma
> desktop under BIOS QEMU; UEFI reaches GRUB and the graphical boot splash.**
> The 2.4 GB hybrid ISO is produced by the `ubuntu:26.04` GitHub Actions
> workflow. Calamares is configured but a complete disk installation and
> installed-system boot have not been exercised yet. Read
> [`distro/VERIFICATION.md`](distro/VERIFICATION.md) for the exact evidence and
> open gaps before calling this a finished release.

## Stack

| Component | Choice |
|---|---|
| Base | Ubuntu 26.04 LTS `resolute`, amd64 |
| Desktop | KDE Plasma 6.6.x |
| Session | Wayland (`plasma.desktop`); SDDM greeter uses Xorg |
| Packages | APT + Flatpak/Flathub |
| Bootloader | GRUB 2 (BIOS/UEFI packages, `shim-signed`) |
| Installer | Calamares 3.3.14 with `unpackfsc` |
| Browser | NOVAIR 1.0, a PyQt6 interface on Qt WebEngine |
| First login | AURELIA Setup, 13 local setup screens |
| Kernel | Ubuntu `linux-image-generic` |
| Tooling | C++, Python, Bash, QML/JavaScript |
| ISO builder | live-build with Casper |

## Get the image

The Ubuntu/KDE ISO is published as a split GitHub release:
[AURELIA OS 1.0 “Solstice”](https://github.com/GameTFM/AURELIA-OS/releases/tag/v1.0.0-solstice).
Download every `aurelia-1.0-solstice.iso.part-*` file plus `JOIN-THE-ISO.txt`,
join the parts, and verify the result before writing it to USB:

```bash
cat aurelia-1.0-solstice.iso.part-* > aurelia-1.0-solstice.iso
sha256sum -c aurelia-1.0-solstice.iso.sha256
```

The verified SHA-256 is
`b76f0b2144d7b51d11372374ad1e3613f354342270819701c32081031ff0f04b`
(2,408,841,216 bytes). The Windows downloader stages an existing local build,
or fetches an ISO from a URL you supply while verifying its SHA-256. It will
not silently fall back to the legacy demo.

```powershell
.\download-aurelia.cmd
.\download-aurelia.cmd -Url <published-iso-url> -Sha256 <sha256>
.\download-aurelia.cmd -Legacy
```

The third form copies the 5.5 MB legacy custom-kernel image from `build/` and
is explicitly labelled as the legacy demo, not the Ubuntu/KDE release.

## Windows installer (Legacy Alpha)

The verified legacy AURELIA kernel is available as a per-user Windows VM
installer:

[AURELIA-Setup.exe](https://github.com/GameTFM/AURELIA-OS/releases/download/v1.0.0-legacy-alpha/AURELIA-Setup.exe)

It installs to `%LOCALAPPDATA%\AURELIA OS`, includes QEMU 11.1 and the 5.5 MB
kernel ISO, creates desktop and Start Menu shortcuts, and launches a real
AURELIA desktop in a virtual machine. It is unsigned and is **not** the
Ubuntu/KDE release. There is no bare-metal installation yet.

## Build

The image profile only builds on Ubuntu 26.04:

```bash
cd distro
sudo scripts/install-build-deps.sh
sudo scripts/build-iso.sh
```

Expected artifacts:

```text
distro/live-image-amd64.hybrid.iso
distro/live-image-amd64.hybrid.iso.sha256
distro/build/live-build.log
```

The current Windows host cannot run this build. The GitHub Actions workflow at
[`.github/workflows/aurelia-iso.yml`](.github/workflows/aurelia-iso.yml) builds
inside an `ubuntu:26.04` container and uploads the ISO, checksum and build log.
Several runs have completed; the latest artifact is the one described in
[`distro/VERIFICATION.md`](distro/VERIFICATION.md).

Static validation:

```bash
cd distro
python3 scripts/validate.py
```

## Desktop

- **Wallpaper** — original 3840×2160 Solstice mountains with framing pines and a
  low champagne sun, plus an ivory variant.
- **Top bar** — AURELIA launcher, Plasma Global Menu, system tray and digital
  clock.
- **Clock** — custom `org.aurelia.clock` applet: live weekday, time and date in
  Playfair Display, centered like the references.
- **Calendar** — custom `org.aurelia.calendar` applet with working month
  navigation and real today highlighting.
- **Right notes / lower-left media** — KDE Notes and Media Controller widgets.
  They show real notes/media and empty states, never fake data.
- **Weather / system monitor** — stock KDE Weather and System Monitor widgets
  on the right side. Weather stays empty until a location is configured.
- **NOVAIR** — an original tabbed browser with a new-tab page, search,
  bookmarks, history, downloads, private windows, settings, real permission
  prompts and a tracker deny-list, built on Qt WebEngine.
- **First login** — installed systems open the 13-screen AURELIA Setup flow;
  the live ISO skips it and uses Calamares instead.
- **Start menu** — Kickoff in grid mode with real pinned apps and native
  power/session actions.
- **Dock** — floating, centered, fit-content Plasma panel with Icon Tasks.
- **Instance branding** — AURELIA SDDM greeter, Calamares installer, GRUB theme
  and first-login global theme application.
- **Installer** — Calamares with `unpackfsc`, GRUB, SDDM, initramfs refresh and
  live-cleanup steps.

The image is deliberately lean. It does not preinstall an office suite, KDE
Connect, Filelight, a GTK theme bridge, an update notifier or build-only ISO
tools. Plasma Discover is the single APT/Flatpak software center; no Snap
package or Snap backend is requested, and optional apps can be installed from
there.

The Start menu is standard Kickoff, not a bespoke replacement. Weather is the
real KDE Weather widget and remains empty until configured; there is no stock
Reminders widget, so the release does not invent one. Battery, volume, Wi-Fi,
Bluetooth and quick settings come from the Plasma system tray. See
[`distro/DESIGN.md`](distro/DESIGN.md) for the visual mapping and
[`distro/README.md`](distro/README.md) for limitations.

## Repository layout

```text
windows-installer/  per-user Windows VM installer for the legacy kernel
distro/        current distribution: live-build, Plasma theme/applets,
               SDDM, Calamares, GRUB, Flatpak, artwork, docs and tests
kernel/        legacy AURELIA freestanding kernel and its desktop/apps
bootloader/    legacy Limine configuration
scripts/       legacy kernel build and QEMU drivers
tools/         legacy image/font/account/QEMU tooling
assets/        legacy kernel assets
documentation/ legacy kernel status, limitations and milestone evidence
build/         legacy kernel build output (ignored)
```

## Legacy AURELIA kernel

The custom higher-half kernel, Limine BIOS ISO, software compositor, window
manager and fifteen in-kernel applications remain in the repository as a
separate research track. It still builds:

```bash
python scripts/build.py
python scripts/run.py
```

The legacy demo login is `aurelia` / `solstice`; it is unrelated to the
installed Ubuntu user's password. The legacy documents — including its ring-3
test, paging work and BIOS-only limitations — are in
[`documentation/STATUS.md`](documentation/STATUS.md),
[`documentation/LIMITATIONS.md`](documentation/LIMITATIONS.md) and
[`documentation/MILESTONE-REPORT.md`](documentation/MILESTONE-REPORT.md).

## Design identity

Midnight `#090A0E` · Champagne Gold `#D6B878` · Ivory `#F4F0E7` · Graphite
`#202127` · Warm Gray `#85838A`. The artwork, AURELIA mark, theme and applets are
original work, not copies of Windows assets. Ubuntu, KDE, Calamares and Breeze
remain the property of their upstream projects; AURELIA is not affiliated with
them.
