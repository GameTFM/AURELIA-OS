# Building and verifying AURELIA OS

The ISO is built inside the WSL distribution `AURELIA-BUILD` (Ubuntu 26.04) from
the tree at `/opt/aurelia/distro`; the Windows worktree is the source of truth for
the files listed in `build/ci/final-build.sh`.

## Full clean build

From Git Bash on Windows:

```bash
cd /c/Users/karth/Downloads/AURELIA-OS
wsl -d AURELIA-BUILD -u root -- bash -lc \
  'cd /mnt/c/Users/karth/Downloads/AURELIA-OS && bash build/ci/final-build.sh' \
  > build/ci/build11-runner.log 2>&1
```

`final-build.sh` must run inside WSL: it uses Linux paths on both sides, waits
for the `/mnt/c` mount to appear, syncs each changed source file with CRLF
stripped, syntax-checks hooks and Python, runs payload assertions, wipes the old
build tree and then runs `scripts/build-iso.sh`. It reports `BUILD_EXIT=0` on
success and writes the full log to `build/wsl-iso-build-N.log`.

Builds take roughly 17–35 minutes on this machine; there is no hardware
virtualisation, so live boots run under QEMU TCG and take 6–9 minutes each.

## Artifact and payload verification

```bash
bash build/ci/after-build-11.sh
```

It waits for `BUILD_EXIT`, copies the ISO to
`build/ci/artifact/aurelia-1.0-solstice.iso`, writes the SHA-256, runs
`build/ci/verify-iso.sh` (20 assertions against the built tree, including unit
modes, the recovery display-manager conflict, the NetworkManager wired policy and
the initramfs) and then boots the live ISO once for `live-verify.sh`.

## Live verification harness

- `build/ci/boot-serial.sh` — BIOS boot with the serial console on TCP 5557.
- `build/ci/guest-run.py` — connects once, logs in, proves a shell with a probe,
  then runs a batch; each command is read back by a marker on its own line.
- `build/ci/live-verify.sh` — one boot: SDDM, Plasma, NetworkManager link, DNS,
  stray windows, and NOVAIR launched into the Wayland session; screenshot at the
  end via the QEMU monitor (port 5556).
- `build/ci/boot-kernel.sh` — boots the ISO's kernel/initrd with a custom
  `APPEND`; add `systemd.log_level=debug systemd.log_target=console` to capture
  the whole systemd transaction in `build/ci/serial.log`.

Harness rules learned the hard way: QEMU's TCP serial chardev accepts one client
at a time, `wsl.exe` strips `$VAR` from argv (use script files), and a WSL loop
mount disappears when the distribution idles out, so mount/copy/unmount must
happen in one WSL invocation.

## Determinism notes

- `endianness`/`live-build` caches live in `/opt/aurelia/distro/cache`; remove it
  for a fully cold build.
- The ISO carries `md5sum.txt` (323 files) and the artifact directory keeps the
  SHA-256 next to the image.
