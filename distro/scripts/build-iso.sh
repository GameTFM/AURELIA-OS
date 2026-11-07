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

# live-build 3.0~a57 uses genisoimage and isohybrid directly. Route the
# former through xorriso for >4 GiB images and neutralize the latter's
# GRUB2-specific false failure. Both wrappers are recorded in the log.
install -m 0755 "$ROOT/scripts/genisoimage-xorriso" /usr/local/bin/genisoimage
install -m 0755 "$ROOT/scripts/isohybrid-stub" /usr/local/bin/isohybrid

mkdir -p "$ROOT/build"
exec > >(tee -a "$ROOT/build/live-build.log") 2>&1

echo "== AURELIA live-build configuration =="
bash ./auto/config

echo "== building ISO =="
bash ./auto/build

ISO="$ROOT/live-image-amd64.hybrid.iso"
if [ ! -f "$ISO" ] && [ -f "$ROOT/binary.hybrid.iso" ]; then
    mv "$ROOT/binary.hybrid.iso" "$ISO"
fi
if [ ! -f "$ISO" ]; then
    echo "live-build completed but no ISO was produced ($ISO or binary.hybrid.iso)." >&2
    exit 1
fi

sha256sum "$ISO" | tee "$ISO.sha256"
ls -lh "$ISO"
echo "AURELIA ISO ready: $ISO"
