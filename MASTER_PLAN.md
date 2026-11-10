# AURELIA OS master plan

Priority order from the product directive, with the state after build 10
(build 11 is the re-test of the two open live defects).

| # | Priority | State |
|---|---|---|
| 1 | Bootable foundation — Ubuntu 26.04 base, live-build/Casper, hybrid ISO | **Working.** Clean build, BIOS boot to desktop, 19/19 payload checks. UEFI reaches GRUB and splash only. |
| 2 | Real desktop — KWin Wayland, plasmashell, panels, dock, launcher | **Working on build 10.** SDDM autologin, `kwin_wayland` + `plasmashell` running, custom look-and-feel applied. |
| 3 | System functionality — Files, Settings, Terminal, network, audio, display, power | **Partial.** Wired networking and DNS verified on build 11. Audio/display/power paths exist through Plasma but are untouched on hardware. |
| 4 | Application ecosystem — Software (Discover), built-in apps, Flatpak | **Present, not exercised.** Discover/Flatpak wiring and package lists exist; no store installation has been run end to end. |
| 5 | Brand and visual polish — logo, themes, icons, wallpapers | **Present.** Solstice look-and-feel, SDDM theme, Plymouth theme, wallpaper set, applet theme; KDE Welcome Center removed in build 11. |
| 6 | Browser — NOVAIR on Qt WebEngine | **Blocked.** Build 11 aborts without a render node (EGL/GBM); XWayland fallback is in build 12 and unverified. |
| 7 | AI — assistant, provider architecture, permissions | **Interface and provider plumbing exist.** No provider key, no end-to-end run. |
| 8 | Gaming — GameMode, MangoHud, Gamescope, profiles, metrics | **Integrated at package/service level only.** Unverifiable on TCG; no numbers claimed. |
| 9 | Backup, recovery, security | **Backup/recovery implemented and self-tested**; recovery menu and GRUB entries present. Full restore from real media not run. |
| 10 | Release — compatibility, QA, ISO testing, documentation | **In progress.** Installer corrected in source; no disk installation or installed boot has been run. |

## Immediate sequence

1. Build 11: verify wired DHCP + DNS and NOVAIR in the live session.
2. Installer: run Calamares against a virtual disk, then boot the installed
   system and verify SDDM, the AURELIA account and GRUB.
3. UEFI: confirm the desktop under OVMF, not just GRUB/splash.
4. Physical hardware pass on the reference laptop (Wi-Fi, audio, suspend,
   graphics) before any compatibility claim.
