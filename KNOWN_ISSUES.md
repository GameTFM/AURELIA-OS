# Known issues

Status as of build 12 (2026-11-10).

## Open

1. **NOVAIR never maps a window on a machine without a DRM render node.**
   Measured on build 12: with `QT_QPA_PLATFORM=wayland` plus software GL the
   process stays alive and Qt logs its full menu construction, but no window
   appears. The XWayland alternative aborts for two measured reasons — the probe
   shell's `DISPLAY=:0` has no reachable authority, and Qt 6.5+ requires
   `libxcb-cursor0`, which the image lacks. The launcher now requires display,
   `XAUTHORITY` and `libxcb-cursor.so.0` before choosing xcb, and the package
   list ships `libxcb-cursor0`; window mapping itself is still unresolved.
2. **Desktop panel and dock never appear, and the layout is nondeterministic.**
   Konsole renders normally in the same VM, so this is not a capture artifact.
   `plasmashell` runs, yet the two scripted panels are absent in every capture,
   and the desktop widgets appear in some boots and not others.
   `plasma-org.kde.plasma.desktop-appletsrc` does not exist during the session,
   and Plasma 6.6 no longer exposes `PlasmaShell.evaluateScript`, so the panel
   containers cannot be queried through D-Bus. Investigate look-and-feel layout
   application timing and panel (layer-shell) creation on this compositor.
3. **Installer path untested.** Calamares configuration is corrected and the
   payload verifier checks it, but no installation to a disk has been run and no
   installed system has been booted.
4. **UEFI desktop unconfirmed.** UEFI reaches the live GRUB entry and the splash.
5. **Wi-Fi, audio, suspend, external displays, touchpad, webcam untested.** No
   physical hardware has been used.
6. **AI needs a provider.** No key is shipped; no provider exercised end to end.
7. **Gaming Mode unproven on real hardware.** No frame numbers are claimed.
8. **No GPU acceleration in the verification VM.** `card0` only, no `renderD128`.
9. **Backup/restore against a real repository.** Self-test passes; no real
   external destination used.

## Closed

- Live session without a wired link or DNS (build 11).
- KDE Welcome Center window (build 11).
- Installer autologin, colliding login-name preset, missing branding path,
  missing `grubcfg`, live account retained on the target (build 12).
- SDDM start job deleted on every boot (build 10, root cause 7).
- World-writable systemd units (build 10).
- Empty serial log after the GRUB rewrite (build 9).
