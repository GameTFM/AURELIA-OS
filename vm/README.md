# AURELIA in a virtual machine

A VM is the safest way to try AURELIA before touching a disk. The runners here
boot the **same hybrid ISO** that the build produces, verify its SHA-256, and
use the BIOS/virtio-vga configuration that has actually been tested.

## Requirements

| Item | Value |
|---|---|
| QEMU | 8.0 or newer; the repository bundles QEMU 11.1 in `toolchain/qemu` |
| RAM | 4 GB for the guest (8 GB host recommended) |
| Disk | none for a live session; 20 GB free for an install test |
| ISO | 2.4 GB, `aurelia-1.0-solstice.iso` |
| Acceleration | KVM on Linux, WHPX on Windows if enabled, otherwise TCG |

## Get the ISO

Build it locally:

```bash
cd distro && sudo scripts/install-build-deps.sh && sudo scripts/build-iso.sh
```

or use a published release asset. Keep the `.sha256` file beside the ISO; both
runners verify it and refuse to start on a mismatch.

## Run it

Windows (PowerShell):

```powershell
.\vm\run-vm.ps1
.\vm\run-vm.ps1 -Iso D:\images\aurelia-1.0-solstice.iso -MemoryMB 6144
.\vm\run-vm.ps1 -DiskGB 24          # attach an empty 24 GB disk for the installer
```

Linux/macOS:

```bash
./vm/run-vm.sh
./vm/run-vm.sh /images/aurelia-1.0-solstice.iso
DISK_GB=24 ./vm/run-vm.sh            # empty virtio disk for an install test
DISPLAY_BACKEND=sdl ./vm/run-vm.sh   # if the GTK window backend is unavailable
```

Close the window to power off. The live session's changes are discarded.

## What to expect

- GRUB, then the AURELIA Plymouth splash, then SDDM on the live image.
- The live session signs in automatically as `aurelia`; the installed system is
  meant to run the setup/OOBE instead.
- Calamares is on the live desktop for installation.
- With `-DiskGB`/`DISK_GB`, Calamares can install to the attached empty disk;
  the ISO itself is never modified.

## Known limitations in a VM

These are measured, not hypothetical:

- **No GPU acceleration.** The VM exposes `virtio-vga` without a DRM render
  node, so Plasma and Qt use software rendering.
- **NOVAIR does not present a window** on such a machine, although the process
  runs (see `../KNOWN_ISSUES.md`).
- **The top panel and dock do not appear** on the live desktop; the desktop
  widgets themselves render.
- **UEFI desktop is unverified.** The ISO's UEFI path reaches GRUB and the
  splash; the graphical session has only been confirmed under BIOS.

If your host exposes a real GPU to the guest, results will differ; report what
you observe rather than assuming the VM's software path is representative.
