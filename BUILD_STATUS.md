# Build status

## Current: build 12 — PASS

| Field | Value |
|---|---|
| Command | `wsl -d AURELIA-BUILD -u root -- bash -lc 'cd /mnt/c/Users/karth/Downloads/AURELIA-OS && bash build/ci/final-build.sh'` |
| Result | `BUILD_EXIT=0` |
| Started | 2026-11-10T16:37:46-05:00 |
| ISO written | 2026-11-10 17:04 |
| Size | 2,473,328,640 bytes |
| SHA-256 | `0988eaaa7e02271a1d4980910ead9887b2708fa539cab97fbd00ccca2691bb23` |
| Build log | `build/wsl-iso-build-12.log` |
| Payload verifier | 24/24 PASS (`build/ci/verify-12.log`) |
| Live transcript | `build/ci/guest-session-v12.log` |
| Screenshots | `build/ci/v12-screen.png`, `build/ci/v12-late.png` |
| Artifact | `build/ci/artifact/aurelia-1.0-solstice.iso` |

Build 12 also fixed a build-system defect: `final-build.sh` did not sync
`distro/scripts/validate.py`, so the first attempt ran the previous validator and
failed on the old `doAutologin: true` assertion. The script now syncs the
validator and asserts the installer tokens it depends on.

## Build history

| Build | Size (bytes) | Verifier | Notable |
|---|---|---|---|
| 12 | 2,473,328,640 | 24/24 | Installer corrected; NOVAIR survives but no window; panel/dock absent |
| 11 | 2,473,328,640 | 21/21 | Wired networking + DNS; no Welcome window; NOVAIR exit 255 |
| 10 | 2,475,753,472 | 19/19 | Desktop with panel/dock; network down; NOVAIR EGL abort |

## Build pipeline

1. `build/ci/final-build.sh` syncs the distro tree, the validator, the
   NetworkManager policy, the NOVAIR launcher, the desktop package list and the
   Calamares modules into `/opt/aurelia/distro`.
2. `distro/scripts/build-iso.sh` runs `lb build` (live-build 3.0~a57) and writes
   `live-image-amd64.hybrid.iso`.
3. `build/ci/after-build-12.sh` copies the ISO, writes its SHA-256, runs
   `verify-iso.sh`, then one `live-verify.sh` boot.

## Next: build 13

Scope is limited to the two open defects:

- panel/dock containers not visible;
- NOVAIR window absent.

Build 13 must not regress the 24 verified payload checks, the wired/DNS path or
the absence of the KDE Welcome window.
