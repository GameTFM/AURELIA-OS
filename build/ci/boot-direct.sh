#!/bin/bash
# Boot the extracted kernel/initrd directly with a serial console, so casper
# tells us why it cannot find the live filesystem.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ISO="$ROOT/build/ci/artifact/live-image-amd64.hybrid.iso"
QEMU="$ROOT/toolchain/qemu/qemu-system-x86_64.exe"
w() { cygpath -m "$1" 2>/dev/null || printf '%s' "$1"; }
rm -f "$ROOT/build/ci/serial-direct.log"
exec "$QEMU" \
  -name AURELIA-DIRECT \
  -machine pc -accel tcg,thread=multi -cpu max -m 4096 -smp 2 \
  -kernel "$(w "$ROOT/build/ci/boot/vmlinuz")" \
  -initrd "$(w "$ROOT/build/ci/boot/initrd.img")" \
  -append "boot=casper live-media-path=/casper console=ttyS0,115200 debug ignore_loglevel" \
  -drive "file=$(w "$ISO"),media=cdrom,readonly=on" \
  -device virtio-vga -display egl-headless \
  -monitor tcp:127.0.0.1:5556,server,nowait \
  -serial "file:$(w "$ROOT/build/ci/serial-direct.log")" \
  -no-reboot -L "$(w "$ROOT/toolchain/qemu/share")"
