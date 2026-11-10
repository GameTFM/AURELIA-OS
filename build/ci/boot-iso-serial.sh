#!/bin/bash
# Boot the extracted AURELIA live kernel with the serial console visible in a
# log, using the real ISO as the live media. Lives under the ignored build/ tree.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ISO="${1:-$ROOT/build/ci/artifact/live-image-amd64.hybrid.iso}"
BOOTDIR="${2:-$ROOT/build/ci/boot-live}"
PORT="${3:-5559}"
LOG="$ROOT/build/ci/serial-live.log"
QEMU="$ROOT/toolchain/qemu/qemu-system-x86_64.exe"

[ -f "$ISO" ] || { echo "ISO not found: $ISO" >&2; exit 1; }
[ -f "$BOOTDIR/vmlinuz" ] || { echo "kernel not found: $BOOTDIR/vmlinuz" >&2; exit 1; }
[ -f "$BOOTDIR/initrd.img" ] || { echo "initrd not found: $BOOTDIR/initrd.img" >&2; exit 1; }

rm -f "$LOG"
w() { cygpath -m "$1" 2>/dev/null || printf '%s' "$1"; }

exec "$QEMU" \
    -name AURELIA-LIVE \
    -machine pc \
    -accel tcg,thread=multi \
    -cpu max \
    -m 4096 \
    -smp 2 \
    -kernel "$(w "$BOOTDIR/vmlinuz")" \
    -initrd "$(w "$BOOTDIR/initrd.img")" \
    -append "boot=casper live-media-path=/casper console=ttyS0,115200 no_timer_check" \
    -drive "file=$(w "$ISO"),media=cdrom,readonly=on" \
    -device virtio-vga \
    -display egl-headless \
    -monitor "tcp:127.0.0.1:$PORT,server,nowait" \
    -serial "file:$(w "$LOG")" \
    -netdev user,id=net0 \
    -device e1000,netdev=net0 \
    -no-reboot \
    -L "$(w "$ROOT/toolchain/qemu/share")"
