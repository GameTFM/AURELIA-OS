# Development status

Updated 2026-11-10 after build 12. This is the phase-level view; measured test
results live in `TEST_REPORT.md`, and the exact resume point is in
`CONTINUATION.md`.

## Phase position

| Phase (directive) | State | Evidence |
|---|---|---|
| 0 — Environment and initialization | **Done** | `DEVELOPMENT_ENVIRONMENT.md`, git history, CI harness |
| 1 — Architecture | **Done** | `ARCHITECTURE.md`, `DECISIONS.md`, `DESIGN_SYSTEM.md` |
| 2 — Bootable Linux foundation | **Done** | Build 12 ISO boots to a live session under BIOS |
| 3 — Hardware and system services | **Partial** | Wired networking and DNS verified; audio, display, power, Bluetooth unexercised |
| 4 — Aurelia desktop (Plasma) | **Partial** | SDDM, KWin and plasmashell run; the top panel and dock were absent at capture |
| 5 — Core applications | **Present, not exercised** | Dolphin, Konsole, Kate, Discover, Gwenview, Ark, KCalc, Haruna, Kamoso, KOrganizer, Elisa, System Monitor installed |
| 6–8 — JARVIS (AI) | **Interface and provider plumbing only** | No provider key, no end-to-end inference run |
| 9 — Browser (Apex) | **Blocked** | Process survives under the fallback; no window appears |
| 10 — Gaming | **Packages/services only** | GameMode/MangoHud/Gamescope wired; no measurement possible under TCG |
| 11 — Installer and migration | **Configured, not exercised** | Payload verifier passes 24/24; no disk installation run |
| 12 — Application ecosystem | **Partial** | Discover/Flatpak present; no store transaction run |
| 13 — Security and recovery | **Partial** | Recovery entries and backup self-test exist; no real restore, no encryption test |
| 14 — Visual polish | **In progress** | Solstice look-and-feel, SDDM/Plymouth/GRUB branding applied |
| 15 — Release engineering | **In progress** | Reproducible WSL build, SHA-256, verifier; no signed release |

## Active work

- **Build 13 scope:** make the panel/dock appear, and make Apex present a real
  window on a machine without a DRM render node.
- Diagnosis in flight: one live boot that dumps the panel containers from
  `plasma-org.kde.plasma.desktop-appletsrc` and launches Apex with
  `DISPLAY=:0 QT_QPA_PLATFORM=xcb` (the environment a desktop-launched app gets,
  which the previous probe shell never provided).

## Last known good

| Item | Value |
|---|---|
| Build | 12 |
| ISO SHA-256 | `0988eaaa7e02271a1d4980910ead9887b2708fa539cab97fbd00ccca2691bb23` |
| Payload checks | 24/24 PASS |
| Live session | Plasma Wayland, wired network, DNS, no KDE Welcome |
| Last install test | **never run** |

## Blocking defects

1. Apex window never appears (process alive, EGL fallback warnings).
2. Panel and dock absent in build 12 captures.
3. No Calamares installation has ever been completed.

## Honest summary

A real, buildable, bootable distribution exists. It is not yet a usable daily
desktop: the browser does not present a window, the panel layout is unresolved,
and the installer has not installed anything.
