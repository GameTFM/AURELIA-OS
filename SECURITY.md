# Security

## Implemented and present in the image

- **Privilege separation.** The live and installed session runs as the ordinary
  user `aurelia`; system changes go through Polkit-mediated dialogs. No service
  in AURELIA runs a desktop application as root.
- **Display manager ownership of tty1.** `getty@tty1` is masked so SDDM owns the
  console; the serial getty stays available for diagnostics.
- **Unit integrity.** Shipped systemd units are installed mode `0644`; the build
  fails if a unit becomes executable or world-writable, because a writable unit
  is a local root-escalation path. `verify-iso.sh` asserts the modes.
- **Boot-transaction correctness.** A recovery unit can no longer delete the
  display manager's start job on normal boots; recovery masks SDDM for that boot
  instead of conflicting with it.
- **No shipped credentials.** No API keys, tokens or passwords are stored in the
  image. AI providers read the user's own key from the environment/keyring at
  runtime; the browser's AI sidebar asks before sending page content.
- **Destructive operations require typed confirmation.** The recovery console
  requires `RESTORE`, `BOOTLOADER`, or `YES` before restoring a snapshot,
  reinstalling GRUB or touching a disk, refuses a non-empty restore target, and
  never formats anything automatically. The migration tool never moves or deletes
  source files.
- **Package sources.** Software comes from Ubuntu archives through the native
  package manager; the build verifies package names against the real archive
  indexes and the ISO ships an md5 manifest.

## Not verified

- Firewall policy in the live and installed images.
- Application sandboxing (Flatpak/portal permissions) beyond package presence.
- Disk encryption in the installer (Calamares supports it; untested here).
- Secure Boot signing and update signature verification.
- AppArmor profile coverage in this image.

## Reporting

Security issues should describe the affected component, the exact command or
action, the observed result and the log excerpt. Do not include personal data or
credentials.
