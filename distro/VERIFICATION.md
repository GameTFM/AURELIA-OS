# Verification record

**Date:** 2026-11-07  
**Host:** Windows, Git Bash; no WSL distribution, Docker, `live-build` or
`debootstrap` available locally. The ISO is built by GitHub Actions on
`ubuntu:26.04` and downloaded here for inspection and QEMU boot tests.

## CI build and live boot

| Check | Result |
|---|---|
| GitHub Actions ISO build | **PASS** — completed `ubuntu:26.04` runs upload a hybrid ISO |
| CI run | `42400142245`, commit `d1c9d47dc0d05221c040c71d072754c42f0cb774` |
| Artifact | `13585238671` (`aurelia-os-1.0-solstice`) |
| ISO size | 2,408,841,216 bytes (2.4 GB), latest verified artifact |
| SHA-256 | `b76f0b2144d7b51d11372374ad1e3613f354342270819701c32081031ff0f04b` |
| initramfs contents | **PASS** — 1,087 modules, overlayfs present, patched Casper call |
| BIOS QEMU boot | **PASS** — the `d1c9d47` ISO reaches the Plasma live desktop |
| UEFI QEMU boot | **PASS to GRUB and the graphical boot splash**; pflash OVMF reaches the live entry |
| UEFI desktop | **NOT YET CONFIRMED** — the UEFI harness stopped at a black graphical screen |
| Calamares full install | **NOT RUN** |
| Installed-system boot | **NOT RUN** |
| Hardware / Secure Boot / USB hybrid | **NOT RUN** |

The live desktop evidence is `build/ci/installer-1.png` from the final
`d1c9d47` ISO: wallpaper, thin top bar, large centered
`SATURDAY / 18:37 / 7 NOVEMBER 2026` clock, calendar, notes and media cards,
weather configuration card, system monitor and the floating dock. The live
AURELIA Setup window is correctly absent. KDE's Welcome window still appears
on the live session despite the `Hidden=true` autostart override and was
closed by hand; that override needs another pass. `build/ci/live-clean.png`
is the equivalent desktop from the previous `ec75090` build.

The Casper initrd needed a real fix: `/etc/casper.conf` sets
`BUILD_SYSTEM="AURELIA"`, which leaves `MP_QUIET` empty and turns Casper's
`modprobe "${MP_QUIET}" -b overlay` into a failing empty-module call. The image
now patches that call and stashes the hook-built initramfs so live-build cannot
replace it with a module-less one. The downloaded ISO was checked with
`build/ci/verify-initrd.py`: 1,087 modules, including
`usr/lib/modules/7.0.0-43-generic/kernel/fs/overlayfs/overlay.ko.zst`, and the
patched Casper line.

The UEFI harness now loads OVMF through `pflash` with a writable variable store
instead of passing a combined firmware file to `-bios`. That fixed the firmware
load failure and the live GRUB entry starts; the remaining black-screen result
is recorded above rather than hidden.

## Static distribution validation

Command:

```bash
cd AURELIA-OS/distro
python3 scripts/validate.py
```

Result:

```text
PASS: 117 static checks
```

Covered:

- 5 JSON documents (two custom applets, global theme, two wallpapers);
- Calamares YAML: settings, branding and every module config;
- INI/desktop/color-scheme files for KDE, SDDM, NOVAIR, setup and installer;
- shell entrypoints, hooks and live helpers via `bash -n`, including the
  sudo/pkexec fallback paths for the installer, Flatpak and setup helpers;
- Python tools and payloads via compilation, including NOVAIR and AURELIA
  Setup;
- the Plasma layout via `node --check`;
- QML delimiter checks for the clock, calendar and SDDM greeter;
- required payload files, including NOVAIR and setup, plus the three
  live-build entrypoints;
- Calamares 3.3 `style` map, required install steps, flat `unpackfsc` schema;
- 5 generated image dimensions;
- all **107** package names in the lists, including the explicit runtime list;
- the lean package policy: no forbidden bloat packages, no duplicate entries and
  `--apt-recommends false` in `auto/config`.

## Ubuntu archive package check

The package lists were compared against the real `resolute`
`main`, `restricted`, `universe` and `multiverse` amd64 indexes downloaded from
`archive.ubuntu.com` on 2026-11-07.

- Requested unique packages: **107**
- Absent from the archive: **0**
- APT recommends are disabled (`--apt-recommends false`) so optional KDE
  components cannot be pulled back in.
- `runtime.list.chroot` declares 27 required supplements for graphics, Wi-Fi,
  portals, power management, image/archive support, audio firmware and Secure
  Boot. Discover's APT/Flatpak metadata stack is already a dependency of
  `plasma-discover`.
- Removed from the target for the lean image: `libreoffice-writer`,
  `libreoffice-calc`, `kdeconnect`, `filelight`, `breeze-gtk-theme`,
  `kde-config-gtk-style`, `plasma-discover-notifier`, and build-only `xorriso`.
  `linux-generic` was replaced with `linux-image-generic` to avoid shipping
  development headers in the live image. `bluedevil` is retained so the
  enabled Bluetooth stack has a real KDE control surface.
- `snapd` and `plasma-discover-backend-snap` are not in the target lists, and
  the Snap backend cannot arrive through recommends because recommends are
  disabled. No Snap package is requested by this image.
- Selected resolved versions:
  - `plasma-desktop` `4:6.6.4-0ubuntu1` (updates repo has 6.6.6)
  - `plasma-session-wayland` `4:6.6.4-0ubuntu2` (provides
    `/usr/share/wayland-sessions/plasma.desktop`)
  - `calamares` `3.3.14-0ubuntu25`
  - `casper` `26.04.2`
  - `sddm` `0.21.0+git20250502.4fe234b-2ubuntu3`
  - `fonts-inter` `4.1+ds-1`
  - `grub-efi-amd64-signed` `1.215+2.14-2ubuntu1`
  - `plasma-discover-backend-flatpak` `6.6.4-0ubuntu1`
  - `kde-config-sddm` `4:6.6.4-0ubuntu1`
  - `sddm-theme-breeze` `4:6.6.4-0ubuntu1`

The index cache lives under the ignored `build/package-index/` directory. If it
is absent, `validate.py` skips only the archive-name check and still runs all
other checks.

## Real applet/package verification

The Plasma applet IDs used by the layout were checked against Ubuntu 26.04 file
lists:

- `org.kde.plasma.notes` — provided by `plasma-widgets-addons`
  (`/usr/lib/x86_64-linux-gnu/qt6/plugins/plasma/applets/org.kde.plasma.notes.so`);
- `org.kde.plasma.mediacontroller` — provided by `plasma-workspace`;
- `org.kde.plasma.systemmonitor` — provided by `plasma-workspace`;
- the Wayland session entry is `plasma.desktop`, from the separate
  `plasma-session-wayland` package, and SDDM's autologin value is `plasma`
  (without the `.desktop` suffix);
- SDDM 0.21's greeter defaults to X11, so `xserver-xorg` is an explicit target
  package. The user session itself remains Wayland. SDDM's experimental
  Wayland greeter is not enabled;
- `org.kde.plasma.appmenu`, `digitalclock`, `systemtray` — provided by
  `plasma-workspace`;
- `calamares` 3.3.14 depends on `libkpmcore13`, so the package list names that
  real library rather than the nonexistent `kpmcore`;
- the screenshot app package in 26.04 is `kde-spectacle`, not `spectacle`.

## Artwork verification

Generated by `scripts/generate-artwork.py` with Pillow:

- `aurelia-solstice.png` — 3840×2160, 157 KiB;
- `aurelia-ivory.png` — 3840×2160, 110 KiB;
- SDDM/Calamares/GRUB backgrounds — 1920×1080;
- the AURELIA hexagonal network PNG mark and the separate NOVAIR icon;
- Playfair Display variable font and OFL license installed in the payload and
  each applet package.

The validator confirms all expected image dimensions. Visual UI rendering has
not been seen because no Plasma runtime is available here.

## Legacy kernel build

The legacy bespoke-kernel track was repaired after the partial visual edit by
adding a `draw_clock_widget` compatibility wrapper that composes the new clock,
calendar and system card. It still builds on this host:

```text
compiled 50 objects in 0.55s
linked build/aurelia.elf (1739.1 KiB)
limine BIOS stage 2 installed
ISO: build/aurelia.iso (5.5 MiB)
build OK
```

This is evidence only for the legacy kernel, not the Ubuntu-based AURELIA ISO.

## Checks not run

The following are **not verified**:

- the UEFI live desktop (GRUB and the splash are reached, but the first harness
  ended at a black graphical screen);
- Calamares launch on Wayland, partitioning and a complete disk install;
- GRUB boot of an installed system, installed SDDM login and first-login setup;
- SDDM greeter rendering (live autologin reaches Plasma);
- Flatpak/Flathub registration;
- Notes/Media Controller interaction and NOVAIR browsing beyond launch;
- Secure Boot, BIOS USB hybrid boot and a hardware matrix.

The ISO is built and the BIOS live desktop is boot-tested. It is **not** yet a
fully install-tested release; the gaps above are still open.
