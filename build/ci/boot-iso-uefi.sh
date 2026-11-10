#!/bin/bash
# UEFI boot smoke test for the AURELIA ISO. Lives under the ignored build/ tree.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ISO="${1:-$ROOT/build/ci/artifact/live-image-amd64.hybrid.iso}"
MON_PORT="${2:-5557}"
LOG_DIR="$ROOT/build/ci"
QEMU="$ROOT/toolchain/qemu/qemu-system-x86_64.exe"
CODE="$ROOT/toolchain/qemu/share/edk2-x86_64-code.fd"
VARS_SRC="$ROOT/toolchain/qemu/share/edk2-i386-vars.fd"
VARS="$ROOT/build/ci/edk2-vars.fd"

if [ ! -f "$ISO" ]; then
    echo "ISO not found: $ISO" >&2
    exit 1
fi
if [ ! -x "$QEMU" ]; then
    echo "QEMU not found: $QEMU" >&2
    exit 1
fi
if [ ! -f "$CODE" ] || [ ! -f "$VARS_SRC" ]; then
    echo "UEFI firmware pieces not found: $CODE / $VARS_SRC" >&2
    exit 1
fi
# pflash needs a writable variable store separate from the read-only code.
[ -f "$VARS" ] || cp "$VARS_SRC" "$VARS"

rm -f "$LOG_DIR/serial-uefi.log"
mkdir -p "$LOG_DIR"

# Native Windows QEMU: convert every file argument from Git Bash paths.
w() { cygpath -m "$1" 2>/dev/null || printf '%s' "$1"; }

exec "$QEMU" \
    -name AURELIA-UEFI \
    -machine q35 \
    -accel tcg,thread=multi \
    -cpu max \
    -m 4096 \
    -smp 2 \
    -drive "if=pflash,format=raw,readonly=on,file=$(w "$CODE")" \
    -drive "if=pflash,format=raw,file=$(w "$VARS")" \
    -drive "file=$(w "$ISO"),media=cdrom,readonly=on" \
    -boot order=d \
    -device virtio-vga \
    -device qemu-xhci \
    -device usb-tablet \
    -display egl-headless \
    -monitor "tcp:127.0.0.1:$MON_PORT,server,nowait" \
    -serial "file:$(w "$LOG_DIR/serial-uefi.log")" \
    -netdev user,id=net0 \
    -device e1000,netdev=net0 \
    -no-reboot \
    -L "$(w "$ROOT/toolchain/qemu/share")"
