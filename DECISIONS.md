# Engineering decisions

Each entry records a decision that shaped the distribution, with the evidence
that drove it. Superseded decisions stay visible.

## D1 — Ubuntu 26.04 LTS as the foundation

Chosen over Debian and Fedora for the longest support window on a current kernel,
the broadest laptop hardware coverage, an archive that already carries KDE Plasma
6.6, and Calamares availability. No second package system is mixed in.

## D2 — KDE Plasma 6 on Wayland, not a custom compositor

KWin, plasmashell and the KDE platform give a maintained Wayland compositor,
window management, panels, tray, notifications and accessibility. AURELIA builds
its identity as a look-and-feel, applets, greeter, splash and applications on top
instead of reimplementing a display server.

## D3 — live-build + Casper for the image

Produces a hybrid BIOS/UEFI ISO with a squashfs live system and the standard
Ubuntu live boot path, so installer and recovery work with well-understood
tooling. The build is reproducible on the WSL Ubuntu 26.04 host.

## D4 — NOVAIR on Qt WebEngine

The browser uses a real maintained engine (Chromium via Qt WebEngine) in a PyQt6
shell, rather than a bespoke engine. Security and rendering stay with the engine;
AURELIA owns the interface, branding and AI sidebar.

## D5 — Graphics fallback by capability detection

Software rendering is selected only when `/dev/dri/renderD128` is missing, in the
Plasma session environment and in the NOVAIR launcher. Machines with a render node
keep hardware acceleration. Rationale: measured EGL failures on a GPU-less VM
(`egl: failed to create dri2 screen`) that aborted KWin and the browser.

## D6 — Recovery owns the screen without a boot-time conflict

`aurelia-recovery.service` is gated by `aurelia.recovery=1`. It previously carried
`Conflicts=display-manager.service`, which systemd resolved before conditions
were evaluated: every normal boot got a stop job for the display manager and SDDM
never started. The conflict now belongs to the recovery boot only, via
`systemd.mask=sddm.service` on that GRUB entry, and the service stops the display
manager at runtime if started manually.

## D7 — NetworkManager owns wired devices directly

Ubuntu's default policy marks all wired devices unmanaged because netplan owns
them on server images. AURELIA ships no netplan, so that policy left the live
session offline (`ens3 DOWN`, `DNS_FAIL`). The image overrides the policy path
with an empty `unmanaged-devices` list and relies on NetworkManager's default
wired DHCP profile; `systemd-resolved` provides DNS.

## D8 — Ship no KDE Welcome Center

`plasma-welcome` autostarted on the live desktop and ignored a `Hidden=true`
override. Nothing depends on it, AURELIA has its own setup experience, so the
package is not installed and the verifier asserts its absence.

## D9 — Serial console in every kernel line

Diagnostics depended on it: a regenerated GRUB config silently dropped
`console=ttyS0`, which made every post-GRUB boot look empty. `build-iso.sh` now
appends `console=ttyS0,115200` to every kernel line and warns if it is absent.
