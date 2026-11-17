# Continuation checkpoint

Written 2026-11-10 after publishing build 12.

## Current task

Resolve the two build-12 defects before any further feature work:

1. **Panel and dock absent.** The AURELIA look-and-feel layout script creates a
   top panel (Kickoff, app menu, spacer, system tray, clock) and a bottom
   floating dock (Kickoff, icon tasks). Its desktop widgets *do* render, so the
   script runs; the panels are created but not visible.
2. **NOVAIR presents no window.** The process stays alive with four
   `libEGL warning: egl: failed to create dri2 screen` lines. The previous probe
   exported only `WAYLAND_DISPLAY`; the launcher's XWayland fallback requires
   `DISPLAY` to be set, so it never engaged.

## In flight when this file was written

`build/ci/diag-v12.sh` (background PID recorded in the session) boots the build-12
ISO once and runs:

```bash
f=$HOME/.config/plasma-org.kde.plasma.desktop-appletsrc
grep -c "plugin=org.kde.panel" "$f"          # how many panel containers exist
grep -n "^\[Containments\]" "$f" | head -8   # container ids/config groups
DISPLAY=:0 QT_QPA_PLATFORM=xcb nohup novair https://example.com >/tmp/n2.log 2>&1 &
sleep 45; tail -8 /tmp/n2.log; pgrep -a -f "[n]ovair"
```

plus `xwininfo -root -tree` when available and a framebuffer screenshot
(`build/ci/v12-diag2.png`).

## Next exact actions

1. Read `build/ci/diag-v12.log` and `build/ci/v12-diag2.png`.
   - If panels exist in the appletsrc but are invisible, check panel visibility
     config (`[Containments][N][General]`, `panelVisibility`) and whether the
     shell applied the layout before the panel containment was ready.
   - If NOVAIR shows a window with `QT_QPA_PLATFORM=xcb` and `DISPLAY=:0`, the
     launcher must discover XWayland's display itself instead of relying on
     `DISPLAY` being exported (for example parse `~/.Xauthority`/`:0` from the
     running KWin, or set the platform in the desktop entry with
     `QT_QPA_PLATFORM=xcb` only for an XWayland session).
2. Apply the smallest source fix for each, keep `distro/scripts/validate.py` in
   sync, and make `build/ci/final-build.sh` sync every changed file.
3. Run build 13, then `verify-iso.sh` and one `live-verify.sh` boot.
4. Only after the desktop and browser pass: run Calamares against a spare virtual
   disk and boot the installed system.

## Commands that matter

```bash
# build (from Windows, ~30 min under TCG)
wsl -d AURELIA-BUILD -u root -- bash -lc 'cd /mnt/c/Users/karth/Downloads/AURELIA-OS && bash build/ci/final-build.sh'
# payload + live boot
bash build/ci/after-build-12.sh
# stop a stuck VM
taskkill //IM qemu-system-x86_64.exe //F
```

## Known constraints

- No KVM: one boot is 5–8 minutes, one ISO 27–35 minutes.
- QEMU's TCP serial accepts one client, so a diagnostic must attach before boot.
- `wsl ... bash -lc '...$VAR...'` strips argument variables; use literal paths or
  a script file.
- Host administrator rights are unavailable; nothing outside the workspace and
  the WSL distro is modified.

## State to preserve

- Git `main` at `b252de9`, pushed to `origin`.
- Build 12 ISO and its SHA-256 in `build/ci/artifact/`.
- `PROJECT_STATUS.md`, `TEST_REPORT.md`, `KNOWN_ISSUES.md`, `BUILD_STATUS.md`
  describe build 12; update them together with any new build.
