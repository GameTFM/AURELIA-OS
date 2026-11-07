#!/bin/bash
# Boot the built ISO for a real smoke test. The ISO path can be overridden.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ISO="${1:-$ROOT/live-image-amd64.hybrid.iso}"

if [ ! -f "$ISO" ]; then
    echo "ISO not found: $ISO" >&2
    exit 1
fi

if ! command -v qemu-system-x86_64 >/dev/null 2>&1; then
    echo "Install qemu-system-x86 first." >&2
    exit 1
fi

ACCEL=()
if [ -w /dev/kvm ]; then
    ACCEL=(-enable-kvm -cpu host)
else
    echo "KVM is unavailable; using QEMU software emulation. Boot will be slow." >&2
    ACCEL=(-accel tcg)
fi

qemu-system-x86_64 \
    "${ACCEL[@]}" \
    -m 4096 \
    -smp 2 \
    -machine q35 \
    -device virtio-vga-gl \
    -display gtk \
    -cdrom "$ISO" \
    -boot d
