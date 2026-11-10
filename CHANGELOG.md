# Changelog

## 2026-11-08 — build 11 (verified)

- **Live networking and DNS verified.** `ens3:ethernet:connected`, address
  `10.0.2.15/24`, default route via `10.0.2.2`, and `example.com` resolving to
  `2606:4700:10::ac42:93f3` through `systemd-resolved` on link `ens3`.
- **KDE Welcome Center gone** from the live session (`NO_WELCOME_WINDOW`;
  package absent, verifier asserts it).
- Payload verifier now 21/21 (wired policy, Welcome absence, branding and
  installer checks added).
- NOVAIR still aborts without a render node; the launcher's software-GL exports
  did not help because Qt's Wayland EGL path needs a GBM device. Build 12 adds
  an XWayland fallback for that case; it is not yet verified.

## 2026-11-08 — installer corrections (build 12, not yet built)

- Calamares branding mirrored to `/usr/share/calamares/branding/aurelia`; the
  profile in `/etc/calamares/branding` was never on the search path.
- Installer autologin disabled and the `aurelia` login-name preset removed; it
  collided with the live account already present in the target.
- Target cleanup now deletes the live account, its NOPASSWD sudoers entry,
  autologin drop-ins, OOBE markers and the casper/live-boot packages.
- `grubcfg` added to the exec sequence; EFI boot entry renamed from `ubuntu` to
  `aurelia`.
- The static validator asserted the old autologin behaviour; it now asserts the
  opposite and checks the cleanup tokens (203 checks pass).

## 2026-11-08 — build 11 (in progress)

- Fix live wired networking. Ubuntu's
  `/usr/lib/NetworkManager/conf.d/10-globally-managed-devices.conf` sets
  `unmanaged-devices=*,except:type:wifi,...` because netplan owns Ethernet on
  server images. AURELIA ships no netplan, so NetworkManager never managed
  `ens3` and the live session had no address and no DNS. The distribution now
  ships that path with an empty list.
- Fix NOVAIR aborting without a DRM render node. The launcher set Chromium's
  swiftshader flags but not Qt's, so Qt failed first with
  `libEGL warning: egl: failed to create dri2 screen`. It now exports
  `LIBGL_ALWAYS_SOFTWARE`, `QT_QUICK_BACKEND=software` and `QT_OPENGL=software`
  only when `/dev/dri/renderD128` is absent.
- Remove `plasma-welcome`. KDE's Welcome Center autostarted on the live desktop
  and ignored a `Hidden=true` autostart override; nothing depends on the package
  and AURELIA has its own setup experience.
- One-boot live verification harness (`build/ci/live-verify.sh`): desktop,
  NetworkManager link, DNS, stray windows and NOVAIR in the real session.

## 2026-11-08 — build 10

- **The AURELIA desktop starts.** Root cause 7: `aurelia-recovery.service` was
  `WantedBy=multi-user.target` and declared `Conflicts=display-manager.service`.
  systemd resolves conflicts while building the transaction, before the unit's
  `ConditionKernelCommandLine` is evaluated, so every normal boot got a stop job
  for the display manager and `sddm.service/start` was deleted. The recovery GRUB
  entry now boots with `systemd.mask=sddm.service` and the service stops the
  display manager from `ExecStartPre` instead.
- Unit files were world-writable (`0777`) in the image because the payload comes
  from a Windows worktree. `0960` now normalises them to `0644`; `0990` and the
  ISO verifier fail if that regresses. A writable systemd unit is a local
  root-escalation path.
- Debug-boot harness: `systemd.log_level=debug systemd.log_target=console` with
  `-serial file:` writes the full boot transaction to `build/ci/serial.log`.
- `guest-run.py` rewritten twice: it now proves a shell by running a probe, and
  it waits for a marker on its own line so command output is not lost in the
  echoed command.
- Docs: root causes 7–9 in `distro/VERIFICATION.md`, status rows in
  `distro/TESTING.md`.

## 2026-11-08 — build 9

- Serial console restored: the regenerated GRUB config had dropped
  `console=ttyS0`, so every post-GRUB boot wrote an empty `serial.log`.
- SDDM drop-in clears the inherited `Conflicts=plymouth-quit-wait.service`.
- Payload verifier 16/16 PASS. The desktop still did not start (root cause 7 was
  not yet known).
