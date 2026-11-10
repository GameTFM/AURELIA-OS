# AURELIA OS — project status

Last updated: 2026-11-08, after build 11 (build 12 in progress).

## What AURELIA is

A real, installable Linux desktop distribution: Ubuntu 26.04 LTS "Resolute"
foundation, KDE Plasma 6 on Wayland, AURELIA's own Plasma look-and-feel, SDDM
greeter, Plymouth splash, wallpapers, icons, the NOVAIR browser, the AURELIA
setup/OOBE, migration and backup tools, recovery entries and a hybrid BIOS/UEFI
ISO built with live-build + Casper.

## Current artifact — build 11

| Field | Value |
|---|---|
| ISO | `build/ci/artifact/aurelia-1.0-solstice.iso` |
| Size | 2,473,328,640 bytes |
| SHA-256 | `7456f31f36223ea59e49bf4797399de7e6ec10497a5e6a0437e9203911d1dff5` |
| Build | build 11, `BUILD_EXIT=0` |
| Payload verifier | **21/21 PASS** (`build/ci/verify-11.log`) |
| Live transcript | `build/ci/guest-session-v11.log` |
| Build log | `build/wsl-iso-build-11.log` |

## Verified on build 11

| Area | State | Evidence |
|---|---|---|
| Clean ISO build in WSL | PASS | `BUILD_EXIT=0`, 2.47 GB image |
| Payload assertions | PASS 21/21 | `build/ci/verify-11.log` |
| SDDM autologin → Plasma Wayland | PASS | `sddm` active, `kwin_wayland` 990/997, `plasmashell` 1101 |
| System state | PASS | `is-system-running` → `running`, zero failed units |
| Wired networking | PASS | `ens3:ethernet:connected`, `10.0.2.15/24`, default via `10.0.2.2` |
| DNS | PASS | `example.com` → `2606:4700:10::ac42:93f3`; resolver on `ens3`, `10.0.2.3` |
| Stray KDE Welcome window | PASS — none | `NO_WELCOME_WINDOW`, package absent |
| NOVAIR in the session | **FAIL** | 4× `libEGL warning: egl: failed to create dri2 screen`, then `The Wayland connection broke`, exit 255 |

## Build 12 (in progress)

- Installer corrected: branding mirrored into Calamares' search path, installer
  autologin disabled, login-name preset that collided with the live account
  removed, target cleanup deletes the live account/sudoers/casper packages,
  `grubcfg` added, EFI entry renamed to AURELIA.
- NOVAIR launcher falls back to XWayland when no render node exists. **Not yet
  verified** — the build-11 failure was measured with the previous launcher.

## Not yet verified

- Calamares installation to a disk and first boot of the installed system.
- UEFI desktop (UEFI reaches GRUB and the splash; desktop not confirmed).
- Wi-Fi, audio, suspend, external displays, physical GPUs.
- AI providers end to end (needs a user key).
- Gaming Mode with real games and measured frame data.
- Backup/restore against a real restic repository.

Nothing in the "not yet verified" list may be reported as working.
