# AURELIA OS architecture

## Layers

```
base system      Ubuntu 26.04 LTS "resolute", Linux 7.0 kernel, Debian packages
init             systemd; Casper live boot; Plymouth AURELIA theme
display manager  SDDM with the AURELIA greeter, autologin on the live image
session          KWin on Wayland + plasmashell, AURELIA Solstice look-and-feel
shell plugins    org.aurelia.clock, org.aurelia.calendar (QML plasmoids)
applications     NOVAIR (PyQt6 + Qt WebEngine), AURELIA Setup/OOBE, Files/Dolphin,
                 Discover, Terminal, system monitors
system services  NetworkManager + systemd-resolved, power-profiles-daemon,
                 udisks2, accounts-daemon, aurelia-firstboot,
                 aurelia-boot-guard, aurelia-boot-success, aurelia-recovery
recovery         GRUB recovery/safe-mode entries, aurelia-recovery console
build            live-build profile in distro/, driven by scripts/build-iso.sh
```

## Build pipeline

`build/ci/final-build.sh` (inside WSL `AURELIA-BUILD`) syncs a declared list of
files from the Windows worktree into `/opt/aurelia/distro`, strips CRLF,
syntax-checks hooks/scripts/Python, runs source assertions, wipes `chroot`,
`binary`, `cache` and the old ISO, then runs `scripts/build-iso.sh`, which:

1. runs `lb config` with the AURELIA profile and package lists;
2. runs `lb build`, executing `distro/config/hooks/*.hook.chroot` in order:
   branding and services (0100), initramfs/Plymouth (0950), migration/backup/OOBE
   units (0960), payload verification (0990);
3. injects `console=ttyS0,115200` into every GRUB kernel line;
4. builds `casper/filesystem.squashfs`, the patched Casper initramfs, the
   `md5sum.txt` manifest and the hybrid BIOS/UEFI El Torito image.

## Boot transaction

`graphical.target` wants `sddm.service` (aliased `display-manager.service`);
`sddm` starts `plasma.desktop` on Wayland through `startplasma-wayland`, and KWin
starts `plasmashell`. `aurelia-firstboot.service` runs before the display manager
and writes the autologin drop-in until the OOBE marker exists.
`aurelia-boot-guard.service` counts unsuccessful boots;
`aurelia-boot-success.service` clears the counter when the graphical boot
completes. `aurelia-recovery.service` is gated by `aurelia.recovery=1` and is
never allowed to conflict with the display manager in a normal transaction.

## Graphics policy

`/etc/xdg/plasma-workspace/env/10-aurelia-software-gl.sh` selects Mesa software
rendering and the Qt Quick software backend only when `/dev/dri/renderD128` is
absent. `/usr/bin/novair` applies the same detection for Qt and Chromium so the
browser works when launched outside the Plasma session environment.

## Networking policy

NetworkManager is the only network manager (no netplan): it owns wired and
wireless devices, and `systemd-resolved` provides DNS through the
`/etc/resolv.conf` stub. The Ubuntu server policy that leaves wired devices
unmanaged is overridden in the image.

## Source layout

```
distro/config/              live-build configuration, package lists, hooks
distro/config/includes.chroot/  every file that lands in the image
distro/scripts/             build-iso.sh and build helpers
build/ci/                   local build, verification and guest harnesses
build/ci/artifact/          published ISO and SHA-256
```
