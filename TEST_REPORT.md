# Test report

Environment: Windows host, WSL2 `AURELIA-BUILD` (Ubuntu 26.04) for builds, QEMU
TCG (no hardware virtualisation) for live boots.

## Build 12 — measured 2026-11-10

| Test | Result | Evidence |
|---|---|---|
| Clean ISO build | PASS (`BUILD_EXIT=0`) | `build/wsl-iso-build-12.log` |
| ISO size / hash | 2,473,328,640 bytes, `0988eaaa…` | `build/ci/artifact/*.sha256` |
| Payload verifier | PASS 24/24 | `build/ci/verify-12.log` |
| Installer branding path | PASS | verifier |
| Installer autologin disabled | PASS | verifier |
| Installer removes live account | PASS | verifier |
| Installer generates target GRUB config | PASS (`grubcfg`) | verifier |
| SDDM / Plasma Wayland | PASS (`kwin_wayland` 1199, `plasmashell` 1302) | `build/ci/guest-session-v12.log` |
| Wired IPv4 / route | PASS (`10.0.2.15/24`, via `10.0.2.2`) | same |
| DNS | PASS (`getent hosts example.com`) | same |
| KDE Welcome window | PASS — absent | screenshots |
| Apex process | PASS — survives 45 s, no Wayland disconnect | same |
| Apex window | **FAIL** — no window in either screenshot | `v12-screen.png`, `v12-late.png` |
| Desktop panel / dock | **UNRESOLVED** — absent in both captures ~4 min apart; build 10's were captured much later, so timing is not excluded | same |

## Diagnostics — 2026-11-15 (build 12 image, no rebuild)

| Test | Result | Evidence |
|---|---|---|
| Control: ordinary KDE window renders | **PASS** — Konsole window visible | `build/ci/v12-diag4.png` |
| Apex, Wayland + software GL | Process alive, Qt builds menus, **no window** | `build/ci/v12-diag3.png`, `v12-diag3.log` |
| Apex, XWayland | **Aborts**: `could not connect to display :0`; xcb plugin needs `libxcb-cursor0` | `build/ci/v12-diag2.log` |
| Panel count via D-Bus | **Not available** — Plasma 6.6 has no `PlasmaShell.evaluateScript` | `build/ci/v12-diag3.log` |
| Applets config during session | **Absent** (`~/.config/plasma-org.kde.plasma.desktop-appletsrc`) | `v12-diag2.log`, `v12-diag4.log` |

## Build 11

| Test | Result |
|---|---|
| Clean ISO build | PASS, 2,473,328,640 bytes, `7456f31f…` |
| Payload verifier | PASS 21/21 |
| Networking / DNS | PASS |
| KDE Welcome window | PASS — absent |
| Apex | FAIL — exit 255, Wayland connection broke |

## Build 10

| Test | Result |
|---|---|
| Clean ISO build | PASS, 2,475,753,472 bytes, `351c8949…` |
| Payload verifier | PASS 19/19 |
| SDDM + Plasma desktop (panel, dock, calendar) | PASS, `live10-screen.png` |
| Wired link / DNS | FAIL (`ens3 DOWN`, `DNS_FAIL`) |
| Apex | FAIL (EGL abort) |

## Not run

Calamares install to a disk, installed-system boot, UEFI desktop, Wi-Fi, audio,
suspend, external displays, AI providers, gaming frame metrics, backup to real
media. These are unverified and must not be reported as working.
