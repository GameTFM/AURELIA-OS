# Known issues

Status as of build 11; build 12 is building the installer corrections and the
NOVAIR XWayland fallback.

## Open

1. **NOVAIR does not start on a machine without a DRM render node.** Build 11
   measured `libEGL warning: egl: failed to create dri2 screen` four times, then
   `The Wayland connection broke. Did the Wayland compositor die?` and exit 255,
   with KWin and plasmashell still running. The launcher now switches to XWayland
   (Mesa llvmpipe) in that case; the fix is in build 12 and is not yet verified.
2. **Installer path untested.** Calamares is configured and corrected (branding
   search path, no autologin, live account removed, GRUB config generated), but no
   installation to a disk has been run and no installed system has been booted.
   Build 12 needs an install test against a spare virtual disk.
3. **UEFI desktop unconfirmed.** UEFI reaches the live GRUB entry and the splash;
   the desktop under OVMF has not been observed.
4. **Wi-Fi untested.** NetworkManager and `plasma-nm` are installed; only
   emulated wired Ethernet has been exercised.
5. **Audio, suspend, external displays, touchpad, webcam untested.** No physical
   hardware has been used yet.
6. **AI needs a provider.** No key is shipped; no provider has been exercised end
   to end from the desktop.
7. **Gaming Mode unproven on real hardware.** No frame numbers are claimed.
8. **No GPU acceleration in the verification VM.** QEMU exposes `card0` but no
   `renderD128`, so the session uses Mesa software rendering there.
9. **Backup/restore against a real repository.** Self-test passes; no real
   external destination has been used.

## Closed

- SDDM start job deleted on every boot (build 10, root cause 7).
- World-writable systemd unit files (build 10).
- Live session without a wired link or DNS (build 11).
- KDE Welcome Center window on the live desktop (build 11).
- Installer would autologin the installed user and preset a colliding login name
  (build 12 source).
- Empty serial log after the GRUB rewrite (build 9).
- SDDM conflicting with `plymouth-quit-wait.service` (build 9).
- NOVAIR crash when switching to a loading tab (source fix).
