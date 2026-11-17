# Development environment

Measured 2026-11-10 with `wsl`, `free`, `df`, `lb --version` and `qemu` on the
build host. This file describes the machinery that builds AURELIA; it does not
describe AURELIA itself.

## Host

| Item | Value |
|---|---|
| Host OS | Windows (`win32`), workspace `C:\Users\karth\Downloads\AURELIA-OS` |
| Shell used by the agent | Git Bash (POSIX syntax, `/c/...` paths) |
| CPU model string | not captured (`wmic` is unavailable on this host) |
| Virtualisation exposed to the build | **none** — QEMU runs under TCG |

## Build environment

| Item | Value |
|---|---|
| Distro | WSL2 `AURELIA-BUILD` (Ubuntu 26.04 "Resolute"), running as `root` |
| vCPUs | 8 |
| RAM | 15 GiB |
| Disk | 1,007 GiB virtual disk, 17 GiB used |
| Python | 3.14.4 |
| live-build | 3.0~a57-1 |
| QEMU | bundled `toolchain/qemu/qemu-system-x86_64.exe` |
| Network | `archive.ubuntu.com` reachable (a build fetches ~17 MB of indexes) |
| Kernel under test | `7.0.0-43-generic` (Ubuntu), extracted to `build/ci/new-boot/` |

The distribution tree lives at `/opt/aurelia/distro` inside WSL and is synced
from the Windows workspace by `build/ci/final-build.sh` before each build. The
build environment is disposable; the repository is the source of truth.

## What this environment can prove

- A clean `live-build` ISO from the real package archive.
- BIOS boot to the live Plasma session, with serial-console transcripts.
- Screenshots of the running guest framebuffer.
- Static validation of the payload and the Calamares configuration.

## What it cannot prove

- **No KVM.** Every boot is emulated: 5–8 minutes to reach the desktop, 27–35
  minutes per ISO. Timing and GPU behaviour are not representative.
- **No physical hardware.** Wi-Fi, Bluetooth, audio, suspend, touchpad, webcam,
  battery and external displays cannot be exercised here.
- **No GPU.** The VM exposes `virtio-vga` with no DRM render node, which is
  exactly the condition that exposes the NOVAIR graphics defect.
- **One serial client.** QEMU's TCP serial accepts a single connection, so guest
  diagnostics must attach before boot and stay attached.
- **UEFI desktop untested.** OVMF reaches GRUB and the splash; the graphical
  session has only been confirmed under BIOS.
- **No administrator rights on the Windows host.** Elevation is not requested and
  nothing outside the workspace and the WSL distro is modified.

## Privilege boundaries

Commands run as `root` only inside the disposable WSL distro. The host is never
modified: no services, drivers, partitions or boot entries are touched, and no
`sudo`/elevation wrapper is used on Windows.
