#!/bin/bash
# Local smoke harness for the AURELIA ISO. This file lives under ignored build/.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ISO="${1:-$ROOT/build/ci/artifact/live-image-amd64.hybrid.iso}"
LOG_DIR="$ROOT/build/ci"
QEMU="$ROOT/toolchain/qemu/qemu-system-x86_64.exe"

if [ ! -f "$ISO" ]; then
    echo "ISO not found: $ISO" >&2
    exit 1
fi
if [ ! -x "$QEMU" ]; then
    echo "QEMU not found: $QEMU" >&2
    exit 1
fi

rm -f "$LOG_DIR/serial.log" "$LOG_DIR/screen.ppm" "$LOG_DIR/screen.png"
mkdir -p "$LOG_DIR"

# This is the native Windows QEMU build, so every file argument needs a
# Windows path even though the harness itself runs under Git Bash.
w() { cygpath -m "$1" 2>/dev/null || printf '%s' "$1"; }

exec "$QEMU" \
    -name AURELIA-Test \
    -machine pc \
    -accel tcg,thread=multi \
    -cpu max \
    -m 4096 \
    -smp 2 \
    -drive "file=$(w "$ISO"),media=cdrom,readonly=on" \
    -boot d \
    -device virtio-vga \
    -display egl-headless \
    -monitor tcp:127.0.0.1:5556,server,nowait \
    -serial "file:$(w "$LOG_DIR/serial.log")" \
    -netdev user,id=net0 \
    -device e1000,netdev=net0 \
    -no-reboot \
    -L "$(w "$ROOT/toolchain/qemu/share")"
