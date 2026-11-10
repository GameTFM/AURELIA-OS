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
| NOVAIR process | PASS — survives 45 s, no Wayland disconnect | same |
| NOVAIR window | **FAIL** — no window in either screenshot | `v12-screen.png`, `v12-late.png` |
| Desktop panel / dock | **UNRESOLVED** — absent in both captures ~4 min apart; build 10's were captured much later, so timing is not excluded | same |

## Build 11

| Test | Result |
|---|---|
| Clean ISO build | PASS, 2,473,328,640 bytes, `7456f31f…` |
| Payload verifier | PASS 21/21 |
| Networking / DNS | PASS |
| KDE Welcome window | PASS — absent |
| NOVAIR | FAIL — exit 255, Wayland connection broke |

## Build 10

| Test | Result |
|---|---|
| Clean ISO build | PASS, 2,475,753,472 bytes, `351c8949…` |
| Payload verifier | PASS 19/19 |
| SDDM + Plasma desktop (panel, dock, calendar) | PASS, `live10-screen.png` |
| Wired link / DNS | FAIL (`ens3 DOWN`, `DNS_FAIL`) |
| NOVAIR | FAIL (EGL abort) |

## Not run

Calamares install to a disk, installed-system boot, UEFI desktop, Wi-Fi, audio,
suspend, external displays, AI providers, gaming frame metrics, backup to real
media. These are unverified and must not be reported as working.
