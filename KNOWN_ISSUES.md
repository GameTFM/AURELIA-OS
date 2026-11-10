# Known issues

Status as of build 12 (2026-11-10).

## Open

1. **NOVAIR shows no window on a machine without a DRM render node.** Build 12's
   XWayland fallback stopped the crash: the process stayed alive for 45 s, printed
   four `libEGL warning: egl: failed to create dri2 screen` lines, and never
   reported `The Wayland connection broke`. But no browser window appeared in
   either screenshot. The browser is still not usable there.
2. **Desktop panel and dock are missing at capture time.** Build 12 boots to the
   Solstice calendar and clock with a black background but no top panel or dock
   in two screenshots taken ~4 minutes apart. Build 10's screenshot with panel
   and dock was captured much later in that session, so this may be look-and-feel
   apply timing rather than a regression. Confirm with a late capture and, if it
   persists, inspect `~/.config/plasma-org.kde.plasma.desktop-appletsrc` and
   plasmashell startup.
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
