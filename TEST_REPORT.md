# Test report

Environment: Windows host, WSL2 `AURELIA-BUILD` (Ubuntu 26.04) for builds, QEMU
TCG (no hardware virtualisation) for live boots.

## Build 11 — measured 2026-11-08

| Test | Result | Evidence |
|---|---|---|
| Clean ISO build | PASS (`BUILD_EXIT=0`) | `build/wsl-iso-build-11.log` |
| ISO size / hash | 2,473,328,640 bytes, `7456f31f…` | `build/ci/artifact/aurelia-1.0-solstice.iso.sha256` |
| Payload verifier | PASS 21/21 | `build/ci/verify-11.log` |
| SDDM | PASS (`active`) | `build/ci/guest-session-v11.log` |
| Plasma Wayland | PASS (`kwin_wayland` 990/997, `plasmashell` 1101) | same |
| System state | PASS (`running`, no failed units) | same |
| NetworkManager | PASS (`ens3:ethernet:connected`) | same |
| IPv4 / route | PASS (`10.0.2.15/24`, default via `10.0.2.2`) | same |
| DNS | PASS (`example.com` → `2606:4700:10::ac42:93f3`) | same |
| KDE Welcome window | PASS — absent | same |
| NOVAIR | **FAIL** — EGL abort, exit 255 | same |

## Build 10

| Test | Result |
|---|---|
| Clean ISO build | PASS, 2,475,753,472 bytes, `351c8949…` |
| Payload verifier | PASS 19/19 |
| SDDM + Plasma desktop | PASS, captured in `build/ci/live10-screen.png` |
| Wired link / DNS | FAIL (`ens3 DOWN`, `DNS_FAIL`) → root cause 8 |
| NOVAIR | FAIL (EGL abort) → root cause 9 |

## Not run

Calamares install to a disk, installed-system boot, UEFI desktop, Wi-Fi, audio,
suspend, external displays, AI providers, gaming frame metrics, backup to real
media. These are unverified and must not be reported as working.
