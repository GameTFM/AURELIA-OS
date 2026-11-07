# AURELIA OS 1.0 “Solstice” — Ubuntu/KDE image profile

AURELIA OS is now an **Ubuntu 26.04 LTS-based desktop distribution** built with
live-build, Casper, KDE Plasma 6.6, Wayland, SDDM, GRUB, Calamares, APT and
Flatpak. The earlier bespoke AURELIA kernel is preserved as the legacy research
track; it is no longer the base of the desktop release.

> **Build status:** the source profile, 4K artwork and static validator are
> complete, and CI produces a 2.4 GB BIOS/UEFI hybrid ISO on Ubuntu 26.04. The
> live image has booted to the Plasma desktop under BIOS QEMU, while UEFI
> reaches GRUB and the graphical boot splash. The
> Calamares install path and the installed system have **not** been run yet. The
> local host is Windows with no WSL distribution, Docker, `live-build` or
> `debootstrap`, so builds happen in GitHub Actions. See [Build](#build),
> [Testing](TESTING.md) and [Verification](VERIFICATION.md) for the evidence.

## Stack

| Component | Implementation |
|---|---|
| Base | Ubuntu 26.04 LTS `resolute`, amd64 |
| Desktop | KDE Plasma 6.6.4 (6.6.6 available in `resolute-updates`) |
| Display server | Wayland session (`plasma.desktop`); SDDM greeter uses Xorg |
| Packages | APT + Flathub/Flatpak |
| Bootloader | GRUB 2, BIOS + UEFI packages, `shim-signed` |
| Installer | Calamares 3.3.14 + `unpackfsc` |
| Browser | NOVAIR 1.0, original PyQt6 interface on Qt WebEngine |
| Kernel | Ubuntu `linux-image-generic` |
| Languages | C++, Python, Bash (theme QML/JS + Python build tooling) |
| ISO builder | live-build with Casper initramfs |
| Live user | `aurelia`, automatic SDDM login |

Package names were checked against the real `resolute` `main`, `restricted`,
`universe` and `multiverse` indexes. All **107** package entries exist,
including the 27 explicit runtime supplements.

## Lean by design

The target package set is intentionally small: no office suite, phone
integration, disk-usage utility, GTK theme bridge, update notifier or build-only
`xorriso`. APT recommends are disabled so optional packages cannot creep back
in; the 27 required runtime supplements are declared explicitly. Plasma
Discover remains the single APT/Flatpak software center, and no Flatpak
applications are preinstalled. No Snap package or Snap backend is requested;
APT and Flatpak are the software sources. Install optional applications from
Discover or APT when needed.

## Build

This profile only builds on Ubuntu 26.04 LTS. It cannot run on Windows, and it
cannot use a 24.04 build host to resolve `resolute` packages.

```bash
cd AURELIA-OS/distro
sudo scripts/install-build-deps.sh
sudo scripts/build-iso.sh
```

`build-iso.sh`:

1. refuses non-26.04 hosts;
2. runs `scripts/validate.py`;
3. regenerates artwork from code;
4. runs `lb build` for a plain ISO and logs to `build/live-build.log`;
5. rebuilds the BIOS El Torito and EFI boot images with the host's GRUB 2.14,
   because live-build 3.0~a57's GRUB2 helpers predate it, call `grub-mkimage`
   without a prefix and never create a UEFI boot image;
6. assembles `live-image-amd64.hybrid.iso` with xorriso, including a hybrid
   MBR, a GPT EFI partition, and both BIOS and UEFI El Torito entries, then
   writes the SHA-256 and reports the ISO path.

The expected output is:

```text
distro/live-image-amd64.hybrid.iso
distro/live-image-amd64.hybrid.iso.sha256
distro/build/live-build.log
```

GitHub Actions builds the same target in an `ubuntu:26.04` container via
[`aurelia-iso.yml`](../.github/workflows/aurelia-iso.yml), with privileged loop
mounts for live-build, and uploads the ISO, SHA-256 and build log. See
[Verification](VERIFICATION.md) for the latest completed run and the boot
screenshots.

## First boot and installation

The live session auto-logs in as `aurelia` on Plasma Wayland. Open the
application menu and search for **Install AURELIA OS**. The launcher grants
Calamares root access to the live Wayland socket, prefers the live user's
passwordless `sudo`, and falls back to `pkexec` when needed. Calamares offers erase, alongside
and manual partitioning, creates a sudo-capable user with a required password,
installs GRUB, removes live-only SDDM autologin/installer state, and refreshes
the installed initramfs.

The live image uses Casper. Calamares unpacks
`/cdrom/casper/filesystem.squashfs` with `unpackfsc`; it deliberately does not
use rsync-based `unpackfs`, which is known to hit xattr `RERR_FILEIO` failures.

### First-login setup

On an installed system, the first KDE login opens **AURELIA Setup**, a real
13-screen PyQt6 setup flow. It writes the chosen region and keyboard layout to
KDE configuration, applies the Midnight or Ivory colour scheme, stores the
selected accent, can copy a profile picture and can change the hostname through
polkit. Its account, network and privacy buttons open the real KDE settings
modules; it does not expose controls that do nothing.

The autostart guard skips `/run/live`, so the flow never covers the live
installer. A completed setup writes `~/.config/aurelia-setup-complete`; closing
it early leaves the marker absent so it appears again at the next login.

## What the desktop actually is

| Surface | Implementation | Status |
|---|---|---|
| Top bar | Plasma panel: AURELIA launcher, Global Menu, expanding spacer, system tray, digital clock | Real Plasma widgets |
| Large clock | Custom `org.aurelia.clock` applet: live weekday, 24-hour time, date | Live system time |
| Left calendar | Custom `org.aurelia.calendar` applet: current month, today highlight, previous/next month controls | Live system date, working navigation |
| Right notes | Stock KDE Notes applet `org.kde.plasma.notes` | Real editable notes |
| Lower-left media | Stock KDE Media Controller `org.kde.plasma.mediacontroller` | Real MPRIS media when playing; empty otherwise |
| Right weather | Stock KDE Weather `org.kde.plasma.weather` | Real widget; empty until a location is chosen |
| Right system monitor | Stock KDE System Monitor `org.kde.plasma.systemmonitor` | Real sensors |
| Browser | NOVAIR 1.0: PyQt6/Qt WebEngine tabs, new-tab page, bookmarks, history, downloads, private profile, settings, permissions and tracker blocklist | Real Qt WebEngine session |
| First login | AURELIA Setup, 13 local screens on installed systems only | Real configuration writes; live ISO uses Calamares |
| Start menu | Kickoff in grid mode with pinned real applications | Stock KDE menu; power/account actions real |
| Taskbar | Floating, centered, fit-content Plasma panel with Icon Tasks | Real windows and launchers |
| Wallpaper | Original generated 4K mountain-and-pine scene + ivory variant | Installed image packages |
| Sign-in | Custom AURELIA SDDM greeter with real `sddm.login()` | Live autologin reaches Plasma; greeter UI untested |
| Installer | AURELIA-branded Calamares 3.3.14 | Static configuration validated, full install untested |
| Boot menu | AURELIA GRUB theme and installed-system branding | GRUB menu reached in BIOS/UEFI QEMU |
| Flatpak | Flathub registered once by a first-login helper with sudo/pkexec | Functional, network-dependent |

The Start menu is KDE Kickoff, not a bespoke launcher; its grid/pinned content
is configured by the layout script. The Notes, Media Controller, Weather and
System Monitor cards are KDE's real widgets, not fake reference screenshots.

### NOVAIR

NOVAIR is an original browser shell, not a renamed copy of an existing browser.
Qt WebEngine provides the Chromium-derived renderer and network stack; NOVAIR
provides the tabs, address/search bar, new-tab page, bookmarks, history,
download manager, private window profile, settings, website-permission prompts,
context menu and developer inspection surface. The new-tab page shows real
bookmarks, recent history and a live local clock. Its search engine, homepage,
theme, pop-up policy and tracker blocking setting are stored in
`~/.config/novair/settings.json`; history and bookmarks live in
`~/.local/share/novair/novair.db`.

The browser does not claim Chrome-extension support, a password vault, reader
mode or in-page translation. Those features are not currently implemented and
are not shown as placeholder controls.

### Applying the layout

A per-user first-login helper runs once and applies the
`org.aurelia.solstice` global theme, including the Plasma layout. To reapply it
manually after changing desktop settings:

```bash
plasma-apply-lookandfeel --apply org.aurelia.solstice
```

The layout is also available at
`/usr/share/plasma/look-and-feel/org.aurelia.solstice/contents/layouts/org.kde.plasma.desktop-layout.js`.
Widget geometry scales from `screenGeometry(0)`: thin top bar, centered clock at
about 42% screen width, calendar at left, notes at right, media card below the
calendar, and a floating centered dock.

## Artwork and theme

- `assets/wallpapers/aurelia-solstice.png` — 3840×2160 midnight mountains with
  framing pines and a low champagne sun.
- `assets/wallpapers/aurelia-ivory.png` — 3840×2160 ivory variant.
- `Aurelia.colors` — full Midnight/Ivory/Champagne palette.
- `AureliaSolstice` and `AureliaIvory` Plasma wallpaper packages.
- Original AURELIA hexagonal network mark (PNG), Calamares/SDDM/GRUB
  backgrounds, and a separate NOVAIR orbital N icon.
- Playfair Display for the editorial clock; Inter for UI; both OFL-compatible.
  The bundled Playfair license is installed with the font.

## Validation

```bash
cd AURELIA-OS/distro
python3 scripts/validate.py
```

The validator checks JSON, Calamares YAML, INI/desktop files, shell syntax,
Python compilation, `node --check` on the Plasma layout, QML delimiter balance,
required payload files, image dimensions, the Calamares install sequence, the
unpackfsc schema, the package names and the lean package policy. It does not prove that Plasma renders
the QML or that Calamares can install a disk. See [TESTING.md](TESTING.md).

## Known limitations and honest gaps

- **The local Windows host still cannot build the image.** Builds run in the
  GitHub Actions `ubuntu:26.04` container; the local workspace only boots and
  inspects downloaded artifacts.
- **Only the live session is runtime-tested.** Plasma renders, SDDM auto-login
  reaches the desktop, and the Solstice layout loads in BIOS and UEFI QEMU.
  First-login setup, Calamares partitioning/install, installed GRUB and
  installed-system SDDM remain to be exercised.
- **The Start menu is standard Kickoff.** It is configured in grid mode, but its
  popup dimensions/appearance are KDE's, not a new custom launcher.
- **Weather, notes, media and the system monitor are real widgets.** Weather
  remains empty until a location is configured. There is no stock Reminders
  widget, so the release does not invent one. Battery, network and volume live
  in the Plasma system tray.
- **NOVAIR implements the core browser, not every item in the wish list.** It
  has tabs, bookmarks, history, downloads, private browsing, settings, find,
  permissions, pop-up blocking, tracker blocking and developer inspection. It
  does not implement Chrome extensions, a password vault, reader mode or
  in-page translation, and does not display those as working features.
- **The first-login setup is installed-system only.** It deliberately does not
  run on the live ISO, where Calamares is the setup path. The setup's 13 screens
  were rendered through Qt offscreen during development, but the KDE/plasma
  integration still needs a booted installed-system test.
- **The live/BIOS USB hybrid path is not fully guaranteed.** The build uses the
  documented `isohybrid` stub to get past live-build's GRUB2 false failure. The
  resulting El Torito ISO is intended for VM/DVD/UEFI boot; raw BIOS USB hybrid
  boot needs a separate real test.
- **`efiBootloaderId` is `ubuntu`.** Ubuntu's signed GRUB/shim has the
  `/EFI/ubuntu` prefix compiled in. This is reliable but the UEFI firmware entry
  is not named AURELIA.
- **Flathub setup is first-login/network-dependent.** No Flatpak applications
  are preinstalled, so the ISO remains smaller and does not download during the
  image build.
- **Only x86_64** is supported. No ARM, Secure Boot validation, Plymouth splash
  redesign, installer slideshow, recovery environment or signing/update channel
  is included.
- **The Calamares/Plasma versions here are statically matched to `resolute` 
  package metadata; package updates can change runtime behavior.**

## Source layout

```text
distro/
  auto/                         live-build config/build/clean
  config/package-lists/         Ubuntu package sets
  config/hooks/normal/          service, live-config and payload checks
  config/includes.chroot/       AURELIA payload copied into the squashfs
    etc/calamares/              installer settings, modules, branding
    etc/sddm.conf.d/            SDDM theme + live autologin
    etc/skel/.config/           first-login KDE defaults
    etc/xdg/autostart/          first-login and Flatpak setup entries
    usr/share/plasma/           global theme + clock/calendar applets
    usr/share/sddm/themes/      custom greeter
    usr/share/wallpapers/       Plasma wallpaper packages
    usr/libexec/aurelia/        first-login, installer, Flatpak and setup helpers
    usr/share/novair/           NOVAIR PyQt6/Qt WebEngine browser
    usr/share/aurelia-setup/    13-screen first-login setup
    usr/bin/                    novair and aurelia-setup launchers
  scripts/                      artwork, validation, build, QEMU boot
  assets/                       generated source artwork, font, license
```

## Licensing

AURELIA theme/applet/installer code is GPL-3.0-or-later; generated artwork is
CC-BY-SA-4.0; the bundled Playfair Display font is under the SIL Open Font
License 1.1. See [`NOTICE`](NOTICE) and [`assets/fonts/OFL.txt`](assets/fonts/OFL.txt).
Ubuntu, KDE, Calamares and Breeze remain the property of their respective
projects. AURELIA is not affiliated with them.
