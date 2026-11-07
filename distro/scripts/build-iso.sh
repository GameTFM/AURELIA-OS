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

# --- BIOS + UEFI boot images -----------------------------------------------
# Ubuntu 26.04 ships live-build 3.0~a57, whose GRUB2 helpers predate GRUB
# 2.14: its grub-mkimage call omits -p and fails, leaving a 2 KiB El Torito
# image with no core.img, and it never creates an EFI image at all. Rebuild
# both boot images against the GRUB actually installed on this host, then
# assemble the hybrid ISO from live-build's finished binary tree.
PLAIN=""
for candidate in \
    "$ROOT/live-image-amd64.iso" \
    "$ROOT/binary.iso" \
    "$ROOT/chroot/binary.iso" \
    "$ROOT/live-image-amd64.hybrid.iso" \
    "$ROOT/binary.hybrid.iso"; do
    if [ -f "$candidate" ]; then
        PLAIN="$candidate"
        break
    fi
done
if [ -z "$PLAIN" ]; then
    echo "live-build completed but produced no ISO." >&2
    exit 1
fi
echo "live-build ISO: $PLAIN"

TREE="$ROOT/binary"
ISO="$ROOT/live-image-amd64.hybrid.iso"
GRUB_BIOS="/usr/lib/grub/i386-pc"
GRUB_EFI="/usr/lib/grub/x86_64-efi"
BIOS_READY=0
EFI_READY=0

if [ -d "$TREE/boot/grub" ] && [ -f "$TREE/boot/grub/grub.cfg" ]; then
    WORK="$(mktemp -d)"
    EFI_CFG="$WORK/efi-grub.cfg"
    cat > "$EFI_CFG" <<'EOF'
search --no-floppy --set=root --label AURELIA_OS
configfile /boot/grub/grub.cfg
EOF

    # BIOS El Torito: cdboot.img + a core.img whose prefix matches the ISO.
    if [ -f "$GRUB_BIOS/cdboot.img" ] && command -v grub-mkimage >/dev/null 2>&1; then
        if grub-mkimage -O i386-pc -d "$GRUB_BIOS" -p /boot/grub \
            -o "$WORK/core.img" \
            biosdisk iso9660 part_msdos part_gpt fat normal configfile linux \
            search search_fs_file search_fs_uuid search_label echo test \
            all_video gfxterm font keystatus \
            && cat "$GRUB_BIOS/cdboot.img" "$WORK/core.img" > "$TREE/boot/grub/grub_eltorito"; then
            BIOS_READY=1
            echo "BIOS El Torito image: $(du -h "$TREE/boot/grub/grub_eltorito" | cut -f1)"
        else
            echo "W: BIOS boot image could not be rebuilt." >&2
        fi
    else
        echo "W: grub-pc-bin/cdboot.img missing; cannot rebuild BIOS boot." >&2
    fi

    # UEFI: a standalone GRUB that finds the ISO's grub.cfg.
    EFI_IMG="$TREE/boot/grub/efi.img"
    if command -v grub-mkstandalone >/dev/null 2>&1 \
        && [ -d "$GRUB_EFI" ] \
        && command -v mkfs.vfat >/dev/null 2>&1 \
        && command -v mmd >/dev/null 2>&1 \
        && command -v mcopy >/dev/null 2>&1; then
        rm -f "$EFI_IMG"
        dd if=/dev/zero of="$EFI_IMG" bs=1M count=64 status=none
        if mkfs.vfat -F 32 -n AURELIA_EFI "$EFI_IMG" >/dev/null 2>&1 \
            && grub-mkstandalone --format=x86_64-efi --output="$WORK/BOOTX64.EFI" \
                "boot/grub/grub.cfg=$EFI_CFG" >/dev/null 2>&1 \
            && mmd -i "$EFI_IMG" ::/EFI ::/EFI/BOOT >/dev/null 2>&1 \
            && mcopy -i "$EFI_IMG" "$WORK/BOOTX64.EFI" ::/EFI/BOOT/BOOTX64.EFI >/dev/null 2>&1; then
            EFI_READY=1
            echo "EFI system image: $(du -h "$EFI_IMG" | cut -f1)"
        else
            echo "W: EFI system image could not be built." >&2
            rm -f "$EFI_IMG"
        fi
    else
        echo "W: EFI tooling missing; cannot build the UEFI image." >&2
    fi
    rm -rf "$WORK"
else
    echo "W: $TREE/boot/grub is missing; cannot rebuild boot images." >&2
fi

HYBRID=0
if [ "$BIOS_READY" = 1 ] && [ "$EFI_READY" = 1 ] \
    && [ -f "$GRUB_BIOS/boot_hybrid.img" ] \
    && command -v xorriso >/dev/null 2>&1; then
    rm -f "$ISO"
    if xorriso -as mkisofs \
        -iso-level 3 -full-iso9660-filenames -J -joliet-long -l -r \
        -V "AURELIA_OS" \
        -A "AURELIA OS 1.0" \
        -p "AURELIA live-build" \
        -publisher "AURELIA OS Project" \
        -isohybrid-mbr "$GRUB_BIOS/boot_hybrid.img" \
        -b boot/grub/grub_eltorito -no-emul-boot -boot-load-size 4 \
        -boot-info-table --grub2-boot-info \
        -eltorito-alt-boot -e boot/grub/efi.img -no-emul-boot \
        -isohybrid-gpt-basdat \
        -o "$ISO" "$TREE"; then
        HYBRID=1
        echo "Assembled hybrid BIOS+UEFI ISO."
    else
        echo "W: hybrid assembly failed." >&2
        rm -f "$ISO"
    fi
else
    echo "W: a boot image is missing; skipping hybrid assembly." >&2
fi

if [ "$HYBRID" != 1 ]; then
    cp -f "$PLAIN" "$ISO"
    echo "W: shipping live-build's ISO unchanged; it may not boot." >&2
fi

sha256sum "$ISO" | tee "$ISO.sha256"
ls -lh "$ISO"
echo "AURELIA ISO ready: $ISO"
