#!/bin/bash
# Sync the AURELIA source from the Windows worktree into the WSL build tree,
# verify the payload hooks, then run a full clean ISO build. This lives in a
# file because variables passed through wsl.exe's argv do not survive.
set -euo pipefail

SRC=/mnt/c/Users/karth/Downloads/AURELIA-OS/distro
DST=/opt/aurelia/distro
LOG=/mnt/c/Users/karth/Downloads/AURELIA-OS/build/wsl-iso-build-7.log

sync_file() {
    rel="$1"
    cp -f "$SRC/$rel" "$DST/$rel"
    tr -d '\r' < "$DST/$rel" > "/tmp/aurelia-sync.tmp"
    mv "/tmp/aurelia-sync.tmp" "$DST/$rel"
    echo "synced $rel"
}

sync_file config/hooks/0100-aurelia-branding.hook.chroot
sync_file config/hooks/0950-aurelia-initramfs.hook.chroot
sync_file config/hooks/0990-aurelia-verify.hook.chroot
sync_file config/hooks/normal/0100-aurelia-branding.hook.chroot
sync_file config/hooks/normal/0950-aurelia-initramfs.hook.chroot
sync_file config/hooks/normal/0990-aurelia-verify.hook.chroot
sync_file scripts/build-iso.sh

mkdir -p "$DST/config/includes.chroot/etc/systemd/system/sddm.service.d"
sync_file config/includes.chroot/etc/systemd/system/sddm.service.d/10-aurelia.conf
chmod 0644 "$DST/config/includes.chroot/etc/systemd/system/sddm.service.d/10-aurelia.conf"
chmod 0755 "$DST/scripts/build-iso.sh"

for hook in "$DST"/config/hooks/*.hook.chroot "$DST"/config/hooks/normal/*.hook.chroot; do
    sh -n "$hook" || { echo "SYNTAX ERROR: $hook" >&2; exit 1; }
done
bash -n "$DST/scripts/build-iso.sh" || { echo "SYNTAX ERROR: build-iso.sh" >&2; exit 1; }

grep -q 'ln -sf /dev/null /etc/systemd/system/getty@tty1.service' \
    "$DST/config/hooks/0100-aurelia-branding.hook.chroot" \
    || { echo "getty@tty1 mask missing from the branding hook" >&2; exit 1; }
grep -q 'sddm.service.d/10-aurelia.conf' "$DST/config/hooks/0990-aurelia-verify.hook.chroot" \
    || { echo "sddm drop-in missing from the verifier" >&2; exit 1; }
grep -q 'md5sum.txt' "$DST/scripts/build-iso.sh" \
    || { echo "md5sum manifest step missing from build-iso.sh" >&2; exit 1; }

echo "SYNC OK"

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
