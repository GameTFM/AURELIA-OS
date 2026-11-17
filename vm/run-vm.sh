#!/usr/bin/env bash
# Boot the AURELIA live ISO in a local virtual machine.
#
# Defaults are the configuration that has actually been tested (BIOS, virtio-vga,
# no disk). Set DISK_GB to create an empty virtio disk if you want to exercise
# the installer in the guest.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ISO="${1:-$ROOT/build/ci/artifact/aurelia-1.0-solstice.iso}"
MEM="${MEM:-4096}"
CPUS="${CPUS:-2}"
BACKEND="${DISPLAY_BACKEND:-gtk}"
DISK_GB="${DISK_GB:-0}"
QEMU="${QEMU:-}"

die() { printf 'error: %s\n' "$*" >&2; exit 1; }

[ -f "$ISO" ] || die "ISO not found: $ISO (pass the path as the first argument)"
if [ -z "$QEMU" ]; then
    if command -v qemu-system-x86_64 >/dev/null 2>&1; then
        QEMU="$(command -v qemu-system-x86_64)"
    elif [ -x "$ROOT/toolchain/qemu/qemu-system-x86_64.exe" ]; then
        QEMU="$ROOT/toolchain/qemu/qemu-system-x86_64.exe"
    else
        die "qemu-system-x86_64 not found; install QEMU or set QEMU=/path/to/qemu-system-x86_64"
    fi
fi

if [ -f "$ISO.sha256" ]; then
    expected="$(cut -d' ' -f1 "$ISO.sha256")"
    if command -v sha256sum >/dev/null 2>&1; then
        actual="$(sha256sum "$ISO" | cut -d' ' -f1)"
    else
        actual="$(shasum -a 256 "$ISO" | cut -d' ' -f1)"
    fi
    [ "$expected" = "$actual" ] || die "SHA-256 mismatch: expected $expected, got $actual"
    echo "SHA-256 verified: $actual"
fi

if [ -w /dev/kvm ]; then
    ACCEL=(-accel kvm -cpu host)
else
    ACCEL=(-accel tcg,thread=multi -cpu max)
    echo "note: no /dev/kvm, using emulation (slower boot)"
fi

DISK_ARGS=()
if [ "$DISK_GB" -gt 0 ]; then
    DISK="$ROOT/build/ci/aurelia-test.qcow2"
    if [ ! -f "$DISK" ]; then
        IMG="$ROOT/toolchain/qemu/qemu-img.exe"
        [ -x "$IMG" ] || IMG="$(command -v qemu-img || true)"
        [ -n "$IMG" ] || die "qemu-img not found; cannot create the test disk"
        "$IMG" create -f qcow2 "$DISK" "${DISK_GB}G"
        echo "created empty test disk: $DISK (${DISK_GB} GB)"
    fi
    DISK_ARGS=(-drive "file=$DISK,if=virtio,format=qcow2")
fi

exec "$QEMU" \
    -name AURELIA-VM \
    -machine pc \
    -m "$MEM" \
    -smp "$CPUS" \
    "${ACCEL[@]}" \
    -cdrom "$ISO" \
    -boot d \
    -device virtio-vga \
    -display "$BACKEND" \
    -netdev user,id=net0 \
    -device e1000,netdev=net0 \
    -usb \
    "${DISK_ARGS[@]}"
