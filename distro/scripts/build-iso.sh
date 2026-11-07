#!/bin/bash
# Build AURELIA OS on Ubuntu 26.04 LTS (Resolute). This cannot run on Windows.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

if [ "$(id -u)" -ne 0 ]; then
    echo "AURELIA ISO builds need root for debootstrap and chroot." >&2
    echo "Run: sudo $0" >&2
    exit 1
fi

if [ ! -r /etc/os-release ]; then
    echo "This build requires Ubuntu 26.04 LTS." >&2
    exit 1
fi

# shellcheck disable=SC1091
. /etc/os-release
if [ "${VERSION_ID:-}" != "26.04" ]; then
    echo "This build profile targets Ubuntu 26.04 (resolute); found ${PRETTY_NAME:-unknown}." >&2
    echo "Use the Ubuntu 26.04 CI workflow or a 26.04 VM." >&2
    exit 1
fi

for tool in lb debootstrap xorriso mksquashfs python3 node; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "Missing build tool: $tool" >&2
        echo "Install the build dependencies with: $ROOT/scripts/install-build-deps.sh" >&2
        exit 1
    fi
done

python3 "$ROOT/scripts/validate.py"
python3 "$ROOT/scripts/generate-artwork.py"

mkdir -p "$ROOT/build"
exec > >(tee -a "$ROOT/build/live-build.log") 2>&1

echo "== AURELIA live-build configuration =="
bash ./auto/config

echo "== building ISO =="
# noauto skips the auto/build hook; this script is the build driver.
lb build noauto

# --- BIOS + UEFI hybrid assembly -------------------------------------------
# Ubuntu 26.04 ships live-build 3.0~a57. Its GRUB2 path writes genisoimage and
# isohybrid calls into a script that runs inside the chroot, but the chroot has
# no isohybrid, and it never writes an EFI El Torito entry at all. The profile
# therefore asks live-build for a plain ISO and we assemble the real hybrid
# image here from the finished binary tree.
PLAIN="$ROOT/live-image-amd64.iso"
ISO="$ROOT/live-image-amd64.hybrid.iso"
TREE="$ROOT/binary"

if [ ! -f "$PLAIN" ] && [ -f "$ISO" ]; then
    PLAIN="$ISO"
fi
if [ ! -f "$PLAIN" ]; then
    echo "live-build completed but no ISO was produced at $PLAIN." >&2
    exit 1
fi

EFI_READY=0
EFI_IMG="$TREE/boot/grub/efi.img"
EFI_CFG="$ROOT/build/efi-grub.cfg"
cat > "$EFI_CFG" <<'EOF'
search --no-floppy --set=root --file /casper/vmlinuz
configfile /boot/grub/grub.cfg
EOF

if [ -f "$TREE/boot/grub/grub.cfg" ] \
    && command -v grub-mkstandalone >/dev/null 2>&1 \
    && command -v mkfs.vfat >/dev/null 2>&1 \
    && command -v mmd >/dev/null 2>&1 \
    && command -v mcopy >/dev/null 2>&1; then
    EFI_WORK="$(mktemp -d)"
    rm -f "$EFI_IMG"
    dd if=/dev/zero of="$EFI_IMG" bs=1M count=64 status=none
    if mkfs.vfat -F 32 -n AURELIA_EFI "$EFI_IMG" >/dev/null 2>&1 \
        && grub-mkstandalone --format=x86_64-efi --output="$EFI_WORK/BOOTX64.EFI" \
            "boot/grub/grub.cfg=$EFI_CFG" >/dev/null 2>&1 \
        && mmd -i "$EFI_IMG" ::/EFI ::/EFI/BOOT >/dev/null 2>&1 \
        && mcopy -i "$EFI_IMG" "$EFI_WORK/BOOTX64.EFI" ::/EFI/BOOT/BOOTX64.EFI >/dev/null 2>&1; then
        EFI_READY=1
        echo "EFI system image: $(du -h "$EFI_IMG" | cut -f1)"
    else
        echo "W: EFI system image could not be built; keeping BIOS-only boot." >&2
        rm -f "$EFI_IMG"
    fi
    rm -rf "$EFI_WORK"
else
    echo "W: EFI tooling or GRUB config missing; keeping BIOS-only boot." >&2
fi

HYBRID=0
if [ "$EFI_READY" = 1 ] \
    && command -v xorriso >/dev/null 2>&1 \
    && [ -f /usr/lib/grub/i386-pc/boot_hybrid.img ]; then
    rm -f "$ISO"
    if xorriso -as mkisofs \
        -iso-level 3 -full-iso9660-filenames -J -joliet-long -l -r \
        -V "AURELIA_OS" \
        -A "AURELIA OS 1.0" \
        -p "AURELIA live-build" \
        -publisher "AURELIA OS Project" \
        -isohybrid-mbr /usr/lib/grub/i386-pc/boot_hybrid.img \
        -b boot/grub/grub_eltorito -no-emul-boot -boot-load-size 4 \
        -boot-info-table --grub2-boot-info \
        -eltorito-alt-boot -e boot/grub/efi.img -no-emul-boot \
        -isohybrid-gpt-basdat \
        -o "$ISO" "$TREE"; then
        HYBRID=1
        echo "Assembled hybrid BIOS+UEFI ISO."
    else
        echo "W: hybrid assembly failed; falling back to the classic ISO." >&2
        rm -f "$ISO"
    fi
fi

if [ "$HYBRID" != 1 ]; then
    cp -f "$PLAIN" "$ISO"
    echo "W: shipping the plain El Torito ISO (BIOS boot only)." >&2
fi

sha256sum "$ISO" | tee "$ISO.sha256"
ls -lh "$ISO"
echo "AURELIA ISO ready: $ISO"
