#!/bin/bash
# Boot AURELIA's own kernel and initramfs with a custom command line. This lets
# boot parameters (serial console, masked units) be tested without rebuilding
# the ISO. The ISO is still attached as a CD so casper can find the squashfs.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ISO="${ISO:-$ROOT/build/ci/artifact/aurelia-1.0-solstice.iso}"
KERNEL="${KERNEL:-$ROOT/build/ci/new-boot/vmlinuz-7.0.0-43-generic}"
INITRD="${INITRD:-$ROOT/build/ci/new-boot/initrd.img-7.0.0-43-generic}"
QEMU="$ROOT/toolchain/qemu/qemu-system-x86_64.exe"
LOG_DIR="$ROOT/build/ci"
APPEND="${APPEND:-boot=casper config live-media-path=/casper quiet splash console=ttyS0 username=aurelia hostname=aurelia locale=en_US.UTF-8 keyboard-layouts=us}"

for f in "$ISO" "$KERNEL" "$INITRD"; do
    [ -f "$f" ] || { echo "Missing: $f" >&2; exit 1; }
done

rm -f "$LOG_DIR/serial.log"

w() { cygpath -m "$1" 2>/dev/null || printf '%s' "$1"; }

exec "$QEMU" \
    -name AURELIA-Kernel \
    -machine pc \
    -accel tcg,thread=multi \
    -cpu max \
    -m 4096 \
    -smp 2 \
    -kernel "$(w "$KERNEL")" \
    -initrd "$(w "$INITRD")" \
    -append "$APPEND" \
    -drive "file=$(w "$ISO"),media=cdrom,readonly=on" \
    -device virtio-vga \
    -display egl-headless \
    -monitor tcp:127.0.0.1:5556,server,nowait \
    -serial "file:$(w "$LOG_DIR/serial.log")" \
    -netdev user,id=net0 \
    -device e1000,netdev=net0 \
    -no-reboot \
    -L "$(w "$ROOT/toolchain/qemu/share")"
