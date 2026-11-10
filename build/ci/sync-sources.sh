#!/bin/bash
# Sync changed AURELIA sources into the WSL build tree and validate them.
set -euo pipefail

SRC=/mnt/c/Users/karth/Downloads/AURELIA-OS/distro
DST=/opt/aurelia/distro

sync_file() {
    rel="$1"
    mkdir -p "$(dirname "$DST/$rel")"
    cp -f "$SRC/$rel" "$DST/$rel"
    tr -d '\r' < "$DST/$rel" > /tmp/aurelia-src.tmp
    mv /tmp/aurelia-src.tmp "$DST/$rel"
    echo "synced $rel"
}

sync_file config/includes.chroot/usr/share/novair/novair.py
sync_file config/includes.chroot/usr/share/plasma/look-and-feel/org.aurelia.solstice/contents/layouts/org.kde.plasma.desktop-layout.js
sync_file config/includes.chroot/etc/xdg/plasma-workspace/env/10-aurelia-software-gl.sh
sync_file scripts/build-iso.sh

chmod 0755 "$DST/config/includes.chroot/etc/xdg/plasma-workspace/env/10-aurelia-software-gl.sh"

python3 -m py_compile "$DST/config/includes.chroot/usr/share/novair/novair.py" \
    && echo "NOVAIR_COMPILES"
sh -n "$DST/config/includes.chroot/etc/xdg/plasma-workspace/env/10-aurelia-software-gl.sh" \
    && echo "PLASMA_ENV_OK"
bash -n "$DST/scripts/build-iso.sh" && echo "BUILD_SCRIPT_OK"

grep -q 'progress_value' "$DST/config/includes.chroot/usr/share/novair/novair.py" \
    || { echo "NOVAIR progress fix missing" >&2; exit 1; }
grep -q 'LIBGL_ALWAYS_SOFTWARE' "$DST/config/includes.chroot/etc/xdg/plasma-workspace/env/10-aurelia-software-gl.sh" \
    || { echo "Plasma software-GL fallback missing" >&2; exit 1; }

echo "SOURCES_SYNCED"
