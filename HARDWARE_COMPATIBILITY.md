# Hardware compatibility

Only what has actually been exercised is listed as tested. Everything else is
expected from the Ubuntu 26.04 kernel and Mesa/NetworkManager stacks but has not
been verified by this project.

## Tested

| Platform | Configuration | Result |
|---|---|---|
| QEMU (BIOS, SeaBIOS) | x86_64, 2 vCPU, 4 GB RAM, TCG (no KVM), `virtio-vga`, user-mode network | **Boots to the AURELIA Plasma desktop (build 10).** Graphics run through Mesa software rendering because no DRM render node is exposed. Wired link was unmanaged on build 10 (fixed for build 11). |
| QEMU (UEFI, OVMF/pflash) | Same image | Reaches the live GRUB entry and the AURELIA graphical splash. **Desktop not confirmed.** |

No physical computer has been used for this project yet.

## Target reference platform (not yet tested)

Dell Latitude 7420 — Intel Tiger Lake, Intel Iris Xe graphics, Wi-Fi 6, laptop
panel and battery. It is the intended first physical test target for:
Intel graphics via Mesa, Wi-Fi through NetworkManager/`iwlwifi`, audio
(`sof`/`snd_hda`), touchpad, battery and suspend/resume. The hardware differs
between configurations of the same model, so results must be recorded per unit.

## Expected but unverified

Intel and AMD CPUs; Intel, AMD and NVIDIA GPUs (NVIDIA needs the proprietary
driver, which the image does not preinstall); Ethernet; Wi-Fi; Bluetooth; USB;
audio; webcams; touchpads; touchscreens; external displays over HDMI/DP;
suspend/resume.

## Policy

- No compatibility claim is made from package presence alone.
- The system reports unavailable metrics as unavailable rather than inventing
  temperatures, clocks or power draw.
- Failures on real hardware are recorded in `KNOWN_ISSUES.md` with the exact
  model, kernel and log excerpt.
