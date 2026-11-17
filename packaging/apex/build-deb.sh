#!/usr/bin/env bash
# Build an installable Apex browser package from the same payload the ISO ships.
# Run on Ubuntu/Debian (or inside the AURELIA-BUILD WSL distro):
#   bash packaging/apex/build-deb.sh
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
PAYLOAD="$ROOT/distro/config/includes.chroot"
# dpkg-deb enforces control-directory modes; a Windows/NTFS mount ignores chmod
# and always reports 0777, so stage inside the Linux filesystem.
STAGE="${STAGE:-/tmp/apex-deb-stage}"
OUT="$ROOT/build/ci/artifact"
VERSION="${VERSION:-1.0.0}"

command -v dpkg-deb >/dev/null 2>&1 || {
    echo "dpkg-deb is required (run this on Ubuntu/Debian)" >&2
    exit 1
}

rm -rf "$STAGE"
install -d "$STAGE/DEBIAN" "$STAGE/usr/bin" "$STAGE/usr/share/apex" \
    "$STAGE/usr/share/applications" "$STAGE/usr/share/icons/hicolor/48x48/apps" \
    "$STAGE/usr/share/icons/hicolor/256x256/apps" \
    "$STAGE/usr/share/icons/hicolor/512x512/apps"

install -m 0755 "$PAYLOAD/usr/bin/apex" "$STAGE/usr/bin/apex"
install -m 0644 "$PAYLOAD/usr/share/apex/apex.py" "$STAGE/usr/share/apex/apex.py"
install -m 0644 "$PAYLOAD/usr/share/applications/apex.desktop" \
    "$STAGE/usr/share/applications/apex.desktop"
for size in 48x48 256x256 512x512; do
    install -m 0644 "$PAYLOAD/usr/share/icons/hicolor/$size/apps/apex.png" \
        "$STAGE/usr/share/icons/hicolor/$size/apps/apex.png"
done

cat > "$STAGE/DEBIAN/control" <<EOF
Package: apex
Version: $VERSION
Architecture: amd64
Maintainer: AURELIA OS <support@aurelia.invalid>
Installed-Size: $(du -sk "$STAGE" | cut -f1)
Depends: python3, python3-pyqt6, python3-pyqt6.qtwebengine
Section: web
Priority: optional
Homepage: https://github.com/GameTFM/AURELIA-OS
Description: Apex browser for AURELIA OS
 Apex is a tabbed web browser built on Qt WebEngine with its own tabs,
 bookmarks, history, downloads, private windows, settings and tracker
 blocking. This package is the standalone build of the browser that ships
 with AURELIA OS. It does not include the operating system.
EOF

install -d "$OUT"
# The payload is staged on a Windows mount, which arrives mode 0777; dpkg-deb
# refuses a control directory that group/other can write.
chmod 0755 "$STAGE" "$STAGE/DEBIAN"
chmod 0644 "$STAGE/DEBIAN/control"
dpkg-deb --root-owner-group --build "$STAGE" "$OUT/apex_${VERSION}_amd64.deb" >/dev/null
echo "built: $OUT/apex_${VERSION}_amd64.deb"
dpkg-deb --info "$OUT/apex_${VERSION}_amd64.deb" | sed -n '1,12p'
echo "--- contents ---"
dpkg-deb --contents "$OUT/apex_${VERSION}_amd64.deb" | awk '{print $6}' | head -12
