# AURELIA OS Legacy Alpha — Windows installer

This directory builds a real per-user Windows `.exe` installer for the
**legacy AURELIA kernel**. It is a working, bootable operating system packaged
with QEMU, not a mock desktop and not the Ubuntu/KDE release.

Download the built installer from the
[v1.0.0-legacy-alpha release](https://github.com/GameTFM/AURELIA-OS/releases/tag/v1.0.0-legacy-alpha).

> The Ubuntu 26.04 + KDE image is still source-only. It has not been built or
> boot-tested, so this installer does not claim to install it.

## What it installs

- `AURELIA OS` in `%LOCALAPPDATA%\AURELIA OS`
- a desktop shortcut and Start Menu shortcut
- the verified 5.5 MiB legacy kernel ISO
- a trimmed QEMU 11.1 runtime with BIOS files
- no administrator rights, services, drivers or Windows modifications

The shortcut launches QEMU with the AURELIA ISO. The guest is the real legacy
kernel: top bar, clock, dock, launcher, window manager and built-in apps.

## Build

From the repository root on Windows:

```bash
python scripts/build.py
python windows-installer/build-installer.py
```

The build requires `toolchain/qemu/` and `build/aurelia.iso`. Output:

```text
build/windows-installer/AURELIA-Setup.exe
build/windows-installer/AURELIA-Setup.exe.sha256
```

## Test

```bash
build/windows-installer/AURELIA-Setup.exe --silent \
  --dir=build/windows-installer/test-install \
  --no-launch --no-shortcuts

build/windows-installer/test-install/AureliaLauncher.exe --test-boot
grep 'entering desktop event loop' build/windows-installer/test-install/boot.log
```

The second command boots the installed image headlessly for 20 seconds, writes
`boot.log`, and exits.

## Limits

- This is the **Legacy Alpha**, not the Ubuntu/KDE distro.
- It runs in a virtual machine. It does not install to bare metal.
- No network, persistence, package manager or disk installation.
- The installer is unsigned, so Windows SmartScreen may warn.
- The Ubuntu ISO build is a separate CI/build problem documented in
  [`../distro/VERIFICATION.md`](../distro/VERIFICATION.md).
