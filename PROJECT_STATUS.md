# AURELIA OS — project status

Last updated: 2026-11-10, after build 12.

## What AURELIA is

A real, installable Linux desktop distribution: Ubuntu 26.04 LTS "Resolute"
foundation, KDE Plasma 6 on Wayland, AURELIA's own Plasma look-and-feel, SDDM
greeter, Plymouth splash, wallpapers, icons, the NOVAIR browser, the AURELIA
setup/OOBE, migration and backup tools, recovery entries and a hybrid BIOS/UEFI
ISO built with live-build + Casper.

## Current artifact — build 12

| Field | Value |
|---|---|
| ISO | `build/ci/artifact/aurelia-1.0-solstice.iso` |
| Size | 2,473,328,640 bytes |
| SHA-256 | `0988eaaa7e02271a1d4980910ead9887b2708fa539cab97fbd00ccca2691bb23` |
| Build | build 12, `BUILD_EXIT=0` |
| Payload verifier | **24/24 PASS** (`build/ci/verify-12.log`) |
| Live transcript | `build/ci/guest-session-v12.log` |
| Screenshots | `build/ci/v12-screen.png`, `build/ci/v12-late.png` |
| Build log | `build/wsl-iso-build-12.log` |

## Verified on build 12

| Area | State | Evidence |
|---|---|---|
| Clean ISO build in WSL | PASS | `BUILD_EXIT=0`, 2.47 GB image |
| Payload assertions | PASS 24/24 | `build/ci/verify-12.log` |
| Installer branding on Calamares' search path | PASS | verifier + `0990` |
| Installer does not autologin the installed user | PASS | verifier |
| Installer removes the live account from the target | PASS | verifier |
| Calamares sequence writes the target GRUB config | PASS (`grubcfg`) | verifier |
| SDDM + Plasma Wayland | PASS | `sddm` active, `kwin_wayland` 1199, `plasmashell` 1302 |
| Wired networking | PASS | `10.0.2.15/24`, default via `10.0.2.2` |
| DNS | PASS | `getent hosts example.com` succeeded |
| KDE Welcome window | PASS — absent | transcript + screenshots |

## Defects found on build 12

| Area | State | Evidence |
|---|---|---|
| NOVAIR window | **FAIL** | with the XWayland fallback the process now survives (PID 1715 after 45 s, no "Wayland connection broke", no exit 255), but **no window appears** in either screenshot |
| Desktop panel and dock | **UNRESOLVED** | `v12-screen.png` and `v12-late.png` (~4 min apart) show the Solstice calendar and clock but no top panel or dock. Build 10's full desktop was captured much later in its session, so confirm whether this is look-and-feel apply timing before calling it a regression. |

## Build 13 must

1. Find why the panel/dock containers are not created (inspect
   `plasma-org.kde.plasma.desktop-appletsrc` and plasmashell startup).
2. Make NOVAIR present a window; process survival is not a working browser.

## Not yet verified

- Calamares installation to a disk and first boot of the installed system.
- UEFI desktop (UEFI reaches GRUB and the splash; desktop not confirmed).
- Wi-Fi, audio, suspend, external displays, physical GPUs.
- AI providers, gaming metrics, backup to real media.

Nothing in the "not yet verified" list may be reported as working.
