#!/bin/bash
# Sync every changed AURELIA source into the WSL build tree, validate the
# payload, then run a full clean ISO build.
#
# This script uses Linux paths on both sides, so it must run INSIDE the
# AURELIA-BUILD distribution:
#   wsl -d AURELIA-BUILD -u root -- bash -lc \
#     'cd /mnt/c/Users/karth/Downloads/AURELIA-OS && bash build/ci/final-build.sh'
set -euo pipefail

SRC=/mnt/c/Users/karth/Downloads/AURELIA-OS/distro
DST=/opt/aurelia/distro
LOG=/mnt/c/Users/karth/Downloads/AURELIA-OS/build/wsl-iso-build-12.log

# The 9P mount of C: can be briefly unavailable right after the distribution
# starts, which made the first copy fail before the build began. Wait for it.
ready=0
for attempt in $(seq 1 60); do
    if [ -d "$SRC" ]; then
        ready=1
        break
    fi
    sleep 2
done
if [ "$ready" != 1 ]; then
    echo "Cannot see $SRC; is /mnt/c mounted?" >&2
    exit 1
fi
echo "MOUNT_OK"

sync_file() {
    rel="$1"
    mkdir -p "$(dirname "$DST/$rel")"
    cp -f "$SRC/$rel" "$DST/$rel"
    tr -d '\r' < "$DST/$rel" > /tmp/aurelia-sync.tmp
    mv /tmp/aurelia-sync.tmp "$DST/$rel"
    echo "synced $rel"
}

sync_file config/hooks/0100-aurelia-branding.hook.chroot
sync_file config/hooks/0950-aurelia-initramfs.hook.chroot
sync_file config/hooks/0960-aurelia-features.hook.chroot
sync_file config/hooks/0990-aurelia-verify.hook.chroot
sync_file config/hooks/normal/0100-aurelia-branding.hook.chroot
sync_file config/hooks/normal/0950-aurelia-initramfs.hook.chroot
sync_file config/hooks/normal/0960-aurelia-features.hook.chroot
sync_file config/hooks/normal/0990-aurelia-verify.hook.chroot
sync_file config/includes.chroot/etc/systemd/system/aurelia-recovery.service
sync_file config/includes.chroot/etc/grub.d/41_aurelia_recovery
sync_file config/includes.chroot/usr/lib/NetworkManager/conf.d/10-globally-managed-devices.conf
sync_file config/includes.chroot/usr/bin/novair
sync_file config/package-lists/desktop.list.chroot
rm -f "$DST/config/includes.chroot/etc/xdg/autostart/org.kde.welcome.desktop"
sync_file config/includes.chroot/etc/systemd/system/sddm.service.d/10-aurelia.conf
sync_file config/includes.chroot/etc/calamares/settings.conf
sync_file config/includes.chroot/etc/calamares/modules/users.conf
sync_file config/includes.chroot/etc/calamares/modules/bootloader.conf
sync_file config/includes.chroot/etc/calamares/modules/shellprocess-targetcleanup.conf
sync_file config/includes.chroot/etc/xdg/plasma-workspace/env/10-aurelia-software-gl.sh
sync_file config/includes.chroot/usr/share/novair/novair.py
sync_file config/includes.chroot/usr/share/plasma/look-and-feel/org.aurelia.solstice/contents/layouts/org.kde.plasma.desktop-layout.js
sync_file scripts/build-iso.sh
# The static validator is executed from the WSL tree inside the build; if it is
# not synced, its assertions silently test the previous revision.
sync_file scripts/validate.py

chmod 0755 "$DST/scripts/build-iso.sh"
chmod 0755 "$DST/config/includes.chroot/etc/xdg/plasma-workspace/env/10-aurelia-software-gl.sh"
chmod 0644 "$DST/config/includes.chroot/etc/systemd/system/sddm.service.d/10-aurelia.conf"

for hook in "$DST"/config/hooks/*.hook.chroot "$DST"/config/hooks/normal/*.hook.chroot; do
    sh -n "$hook" || { echo "SYNTAX ERROR: $hook" >&2; exit 1; }
done
bash -n "$DST/scripts/build-iso.sh" || { echo "SYNTAX ERROR: build-iso.sh" >&2; exit 1; }
sh -n "$DST/config/includes.chroot/etc/xdg/plasma-workspace/env/10-aurelia-software-gl.sh" || exit 1
python3 -m py_compile "$DST/config/includes.chroot/usr/share/novair/novair.py" \
    || { echo "NOVAIR does not compile" >&2; exit 1; }

grep -q 'getty@tty1' "$DST/config/hooks/0100-aurelia-branding.hook.chroot" || exit 1
grep -q 'systemd-resolved.service' "$DST/config/hooks/0100-aurelia-branding.hook.chroot" || exit 1
grep -q 'progress_value' "$DST/config/includes.chroot/usr/share/novair/novair.py" || exit 1
grep -q 'grub_eltorito' "$DST/scripts/build-iso.sh" || exit 1
grep -q '^Conflicts=$' "$DST/config/includes.chroot/etc/systemd/system/sddm.service.d/10-aurelia.conf" \
    || { echo "SDDM drop-in lost the conflict reset" >&2; exit 1; }
grep -q 'console=ttyS0' "$DST/scripts/build-iso.sh" || exit 1
if grep -q '^Conflicts=.*display-manager' "$DST/config/includes.chroot/etc/systemd/system/aurelia-recovery.service"; then
    echo "Recovery service still conflicts with the display manager" >&2
    exit 1
fi
grep -q 'systemd.mask=sddm.service' "$DST/config/includes.chroot/etc/grub.d/41_aurelia_recovery" \
    || { echo "Recovery entry lost the display manager mask" >&2; exit 1; }
grep -q 'chmod 0644 /etc/systemd/system/aurelia' "$DST/config/hooks/0960-aurelia-features.hook.chroot" \
    || { echo "0960 lost the unit permission fix" >&2; exit 1; }
grep -q '^unmanaged-devices=$' "$DST/config/includes.chroot/usr/lib/NetworkManager/conf.d/10-globally-managed-devices.conf" \
    || { echo "NetworkManager still ignores wired devices" >&2; exit 1; }
grep -q 'LIBGL_ALWAYS_SOFTWARE' "$DST/config/includes.chroot/usr/bin/novair" \
    || { echo "NOVAIR lost its software-GL fallback" >&2; exit 1; }
grep -q 'usr/share/calamares/branding' "$DST/config/hooks/0100-aurelia-branding.hook.chroot" \
    || { echo "Calamares branding is not mirrored into its search path" >&2; exit 1; }
grep -q '^doAutologin: false' "$DST/config/includes.chroot/etc/calamares/modules/users.conf" \
    || { echo "Installer autologin is enabled again" >&2; exit 1; }
grep -q 'grubcfg' "$DST/config/includes.chroot/etc/calamares/settings.conf" \
    || { echo "Installer never writes the target GRUB configuration" >&2; exit 1; }
grep -q 'userdel -r aurelia' "$DST/config/includes.chroot/etc/calamares/modules/shellprocess-targetcleanup.conf" \
    || { echo "Installer keeps the live account on the target" >&2; exit 1; }
if grep -q '^plasma-welcome$' "$DST/config/package-lists/desktop.list.chroot"; then
    echo "KDE Welcome Center is back in the package list" >&2
    exit 1
fi
grep -q 'org.kde.plasma.weather' "$DST/config/includes.chroot/usr/share/plasma/look-and-feel/org.aurelia.solstice/contents/layouts/org.kde.plasma.desktop-layout.js" \
    && { echo "layout still ships the empty weather card" >&2; exit 1; }

echo "SOURCES_OK"

cd "$DST"
rm -rf chroot binary cache .build
rm -f live-image-amd64.hybrid.iso chroot.packages.install chroot.packages.live \
    chroot.headers binary.packages binary.contents \
    build/aurelia-live-initrd.img build/live-build.log
echo "CLEAN_START $(date -Is)"

set +e
bash ./scripts/build-iso.sh > "$LOG" 2>&1
rc=$?
set -e
echo "BUILD_EXIT=$rc"
