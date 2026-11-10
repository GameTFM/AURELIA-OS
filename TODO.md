# TODO

Ordered by what blocks a usable release. Nothing is marked done without a log or
screenshot.

## Done

- [x] Build 11: wired device connected with an IPv4 address and working DNS
      (`ens3:ethernet:connected`, `10.0.2.15`, `example.com` resolved).
- [x] Build 11: no KDE Welcome window on the live desktop.
- [x] Installer: branding on Calamares' search path, no installed-user autologin,
      live account and casper packages removed from the target, `grubcfg` and
      AURELIA EFI entry configured (source-validated, 203 checks).

## Now

- [ ] Build 12: confirm the payload verifier passes the new installer checks and
      the live session still boots to Plasma with networking.
- [ ] Build 12: verify whether NOVAIR's XWayland fallback starts and loads a page
      without a render node. If it still fails, capture the full Qt log with
      `QT_LOGGING_RULES=qt.qpa.*=true` and reconsider the renderer path.

## Next

- [ ] Run Calamares against a spare virtual disk: partitioning, user creation,
      bootloader install, reboot into the installed system.
- [ ] Verify the installed system: GRUB entries, no autologin, the chosen account
      (not `aurelia`), removal of casper/install launcher, recovery entries.
- [ ] UEFI desktop under OVMF with a persistent variable store.
- [ ] Verify the AURELIA setup/OOBE flow as the first installed login.
- [ ] Verify Software (Discover) installs and removes a real package.
- [ ] Verify migration import and a restic backup/restore to real media.

## Later

- [ ] Physical hardware pass: Wi-Fi, Bluetooth, audio, webcam, touchpad, battery,
      suspend/resume, external display.
- [ ] Gaming: real Steam/Proton title with MangoHud measurements.
- [ ] AI: provider configured, streaming, cancellation, history, sharing prompt.
- [ ] Security review: firewall, app sandboxing, disk encryption, update signing.
