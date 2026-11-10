# Verification record

**Date:** 2026-11-07  
**Host:** Windows 11 + Git Bash, with an Ubuntu 26.04 WSL distribution
(`AURELIA-BUILD`) that runs `live-build` and `debootstrap` itself. ISOs can also
be produced by GitHub Actions on `ubuntu:26.04`.

## Current image: WSL full clean build

| Check | Result |
|---|---|
| Full clean `scripts/build-iso.sh` in WSL | **PASS** — hooks print `AURELIA payload verification OK`, build ends `AURELIA ISO ready` |
| AURELIA Plymouth splash at boot | **PASS** — theme renders instead of Ubuntu's logo |
| Graphical session | **FAIL on the 18:55 build** — the SDDM start job was dropped again (root cause 6); conflict cleared in source, awaiting re-test |
| Payload verifier (`build/ci/verify-iso.sh`) | **16/16 PASS** on the 18:13 and 18:55 builds |
| NOVAIR window | **PASS** — branded toolbar, tab strip, address bar, status bar render |
| NOVAIR navigation | **FAIL, fixed in source and awaiting re-test** — `TypeError: native Qt signal is not callable` |
| Live networking | **FAIL** — `ens3` stayed down and DNS did not resolve |
| Session stability under QEMU (no GPU) | **FAIL** — KWin aborted with "The Wayland connection broke" |
| Calamares install / installed boot / hardware | **NOT RUN** |

### Root cause 1: the display manager never started

The live ISO reached `graphical.target` with no display manager. SDDM was
`enabled`, listed in `Wants=graphical.target`, and its unit and alias links were
valid inside the SquashFS, yet `systemctl status sddm` said `inactive (dead)`,
`journalctl -b -u sddm` said `-- No entries --`, and `systemctl list-jobs`
reported no jobs. A unit that is wanted, valid and never queued is dropped by
systemd's conflict resolution: `sddm.service` declares
`Conflicts=getty@tty1.service`, and Casper starts an explicit `getty@tty1` job
for the live console, so SDDM lost and its start job vanished without a log
entry.

Fixes, both enforced by the build:

- `/etc/systemd/system/getty@tty1.service` is masked (`/dev/null`), so nothing
  competes with SDDM for tty1; tty2–tty6 stay available for consoles.
- `/etc/systemd/system/sddm.service.d/10-aurelia.conf` resets `After=` and drops
  the ordering against `plymouth-quit-wait.service`, whose helper
  (`plymouth --wait`, `TimeoutSec=0`) can block forever when plymouthd does not
exit, silently stalling the display manager again.

Evidence after the fix: the 17:12 ISO boots through SDDM autologin into Plasma
and renders the AURELIA Solstice wallpaper, calendar, dock and system tray.

### Root cause 2: the initramfs carried Ubuntu's theme

Ubuntu's Plymouth initramfs hook only copies the theme named in
`plymouthd.defaults`, so the AURELIA theme in the SquashFS was never used at
boot. The initramfs hook now writes `/etc/plymouth/plymouthd.conf`, installs
`/etc/initramfs-tools/hooks/aurelia-plymouth` to copy the theme, its script
module and fonts, and fails the build if the theme is missing from the result.

### Root cause 3: no ISO checksum manifest

`casper-md5check` failed every boot because live-build never wrote `/md5sum.txt`
for this profile. `build-iso.sh` now generates it from the finished tree and
excludes `boot/grub/grub_eltorito`, which xorriso patches with its boot info
table while writing the ISO.

### Root cause 4: NOVAIR crashed when switching to a loading tab

`_current_changed` called `tab.loadProgress()`, which is a Qt signal, not a
getter, raising `TypeError: native Qt signal is not callable` and aborting the
navigation. `BrowserTab` now caches the latest value in `progress_value` and the
window reads that.

### Root cause 5: live session had no name resolution or GPU-less fallback

`/etc/resolv.conf` is the `systemd-resolved` stub symlink, but the resolver unit
was never enabled, so DHCP results never became DNS answers. The branding hook
now enables `systemd-resolved.service`. Separately, systems with no DRM render
node could not create an EGL context, which killed KWin; 
`/etc/xdg/plasma-workspace/env/10-aurelia-software-gl.sh` now selects Mesa's
software rasteriser and the Qt Quick software backend only when
`/dev/dri/renderD128` is absent.

### Root cause 6: SDDM still lost the boot transaction

The 18:55 build booted through Plymouth but never started the display manager,
even with `getty@tty1` masked. Measured on the live guest: `graphical.target`
`active`, `systemctl is-system-running` `running`, zero failed units, no queued
jobs, `sddm` `LoadState=loaded`, `UnitFileState=enabled`, `ActiveState=inactive`,
and `journalctl -b` contained no SDDM line at all. `sddm.service` also ships
`Conflicts=plymouth-quit-wait.service`; that unit is always part of a live-image
boot transaction, and when a start job conflicts with one already queued,
systemd deletes it without a default-level journal entry. The drop-in in
`/etc/systemd/system/sddm.service.d/10-aurelia.conf` now clears the inherited
conflict list (`Conflicts=`) and re-declares only `Conflicts=getty@tty1.service`,
which is moot because tty1 is masked. `0990-aurelia-verify.hook.chroot` fails the
build if that reset is ever removed.

Build 10 is the first image that proves the fix on real hardware emulation. Its
live guest reported `systemctl is-active sddm` → `active`, `systemctl
is-system-running` → `running`, zero failed units, `kwin_wayland` and
`plasmashell --no-respawn` running, and the captured screen
(`build/ci/live10-screen.png`) shows the AURELIA wallpaper, the Solstice calendar,
the tray clock and the dock on the Plasma 6 desktop. The verifier is 19/19 PASS.

### Root cause 8: the live session never had a wired link

Build 10's desktop worked, but `ip -brief addr` showed `ens3 DOWN` and
`getent hosts example.com` failed. NetworkManager was installed, enabled and
running. The cause was Ubuntu's own policy file:

```
/usr/lib/NetworkManager/conf.d/10-globally-managed-devices.conf
[keyfile]
unmanaged-devices=*,except:type:wifi,except:type:gsm,except:type:cdma
```

That policy exists because netplan owns Ethernet on Ubuntu Server and cloud
images. AURELIA ships no netplan and `/etc/network/interfaces` is empty, so
NetworkManager treated every wired device as unmanaged and never created a DHCP
profile. The distribution now ships the same path with `unmanaged-devices=`
(empty), so wired devices are managed and NetworkManager's default wired profile
runs DHCP. `0990` fails the build if the Ubuntu policy returns, and
`verify-iso.sh` checks the shipped file.

### Root cause 9: NOVAIR aborted without a DRM render node

Launched from a login shell on a GPU-less VM, NOVAIR printed
`libEGL warning: egl: failed to create dri2 screen` three times and exited
without a window. KWin survived the same environment only because
`/etc/xdg/plasma-workspace/env/10-aurelia-software-gl.sh` exports software GL into
the Plasma session. NOVAIR's launcher only set Chromium's swiftshader flags, so
Qt itself still failed before Chromium started. `/usr/bin/novair` now exports
`LIBGL_ALWAYS_SOFTWARE`, `QT_QUICK_BACKEND=software` and `QT_OPENGL=software`
when `/dev/dri/renderD128` is absent, and keeps hardware acceleration when a
render node exists.

Build 11 measured the network fix and the welcome fix, and showed that the NOVAIR
change was not sufficient: `ens3:ethernet:connected` with `10.0.2.15/24`, a
default route via `10.0.2.2`, and `example.com` → `2606:4700:10::ac42:93f3`
through `systemd-resolved`; no `plasma-welcome` process and the package absent.
NOVAIR still printed four `libEGL warning: egl: failed to create dri2 screen`
lines followed by `The Wayland connection broke` and exited 255 while KWin
(990/997) and plasmashell (1101) kept running. Qt's native Wayland EGL path needs
a GBM device, which `LIBGL_ALWAYS_SOFTWARE` does not provide; build 12 makes the
launcher switch to XWayland (Mesa llvmpipe) in that case. That change is not yet
verified.

Diagnostics that made this measurable: the regenerated `boot/grub/grub.cfg` had
lost `console=ttyS0`, so every boot after the GRUB rewrite produced an empty
`serial.log`; `build-iso.sh` now appends `console=ttyS0,115200` to every kernel
line. With a console, `build/ci/boot-serial.sh` exposes an interactive guest
shell over a TCP serial port and `build/ci/guest-run.py` drives it without
reconnecting mid-boot.

### Root cause 7: a recovery unit's display-manager conflict deleted SDDM's start job

Build 9 still came up without a display manager. Booting its own kernel with
`systemd.log_level=debug` on the serial console showed the transaction directly:

```
graphical.target: Trying to enqueue job graphical.target/start/isolate
sddm.service: Looking at job sddm.service/start conflicted_by=no
sddm.service: Looking at job sddm.service/stop conflicted_by=yes
sddm.service: Fixing conflicting jobs sddm.service/start,sddm.service/stop by deleting job sddm.service/start
graphical.target: Changed dead -> active
graphical.target: Job 1 graphical.target/start finished, result=done
```

`aurelia-recovery.service` is `WantedBy=multi-user.target` and declared
`Conflicts=display-manager.service` so the recovery console could own the screen.
systemd resolves conflicts while it builds the boot transaction, **before** a
unit's `ConditionKernelCommandLine` is evaluated. The stop job was therefore
added on every normal boot, SDDM's start job was discarded as conflicting, and
`graphical.target` became active with no display manager, no failed unit and no
journal entry at the default log level — which is why earlier checks looked
clean. `sddm` itself was enabled, `/etc/X11/default-display-manager` named
`/usr/bin/sddm`, and autologin for `aurelia` was configured, so nothing about
SDDM was wrong.

The conflict is now expressed at runtime instead of in the transaction: the
recovery GRUB entry boots `multi-user.target` with `systemd.mask=sddm.service`,
and the service stops the display manager from `ExecStartPre` if it is started
manually from a running desktop. `0990-aurelia-verify.hook.chroot` fails the
build if any AURELIA unit reintroduces `Conflicts=display-manager.service`, if
the recovery entry loses the mask, or if a shipped unit file is not mode `0644`.
The payload arrives from the Windows worktree as `0777`; a world-writable
systemd unit is a local root-escalation path and systemd warns when loading it,
so `0960` normalises the modes.

## Historical CI builds (earlier revisions)

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
PASS: 194 static checks
```

Covered:

- 5 JSON documents (two custom applets, global theme, two wallpapers);
- Calamares YAML: settings, branding and every module config;
- INI/desktop/color-scheme files for KDE, SDDM, NOVAIR, setup and installer;
- shell entrypoints, hooks and live helpers via `bash -n`, including the
  sudo/pkexec fallback paths for the installer, Flatpak and setup helpers;
- Python tools and payloads via compilation, including NOVAIR, AURELIA Setup,
  migration, backup and the privileged OOBE helper;
- migration/backup backend self-tests against fake trees, including no-overwrite
  and SHA-256 verification;
- boot, recovery, GRUB, Plymouth and update-guard payload checks;
- the Plasma layout via `node --check`;
- QML delimiter checks for the clock, calendar and SDDM greeter;
- required payload files, including NOVAIR and setup, plus the three
  live-build entrypoints;
- Calamares 3.3 `style` map, required install steps, flat `unpackfsc` schema;
- 5 generated image dimensions;
- all **118** package names in the lists, including the explicit runtime list;
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
