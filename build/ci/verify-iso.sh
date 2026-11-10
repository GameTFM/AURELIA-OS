#!/bin/bash
# Post-build verification of the AURELIA live image. Runs inside WSL against the
# built tree in /opt/aurelia/distro. Every check prints PASS or FAIL and the
# script exits non-zero if anything is missing.
set -uo pipefail

ROOT=/opt/aurelia/distro
SQ=$ROOT/binary/casper/filesystem.squashfs
ISO=$ROOT/live-image-amd64.hybrid.iso
fails=0

pass() { echo "PASS  $1"; }
fail() { echo "FAIL  $1"; fails=$((fails + 1)); }

[ -f "$SQ" ] && pass "squashfs present" || fail "squashfs missing"
[ -f "$ISO" ] && pass "iso present ($(du -h "$ISO" 2>/dev/null | cut -f1))" || fail "iso missing"

if [ -f "$SQ" ]; then
    unsquashfs -cat "$SQ" etc/passwd 2>/dev/null | grep -q '^aurelia:' \
        && pass "live user aurelia in /etc/passwd" || fail "live user aurelia in /etc/passwd"

    for bin in usr/bin/sddm usr/bin/plasmashell usr/bin/novair usr/bin/aurelia-setup; do
        if unsquashfs -ll "$SQ" "$bin" 2>/dev/null | grep -q "$bin"; then
            pass "binary $bin"
        else
            fail "binary $bin"
        fi
    done

    if unsquashfs -ll "$SQ" etc/systemd/system/graphical.target.wants/sddm.service 2>/dev/null \
        | grep -q 'graphical.target.wants/sddm.service'; then
        pass "graphical.target wants sddm"
    else
        fail "graphical.target wants sddm"
    fi

    if unsquashfs -ll "$SQ" etc/systemd/system/getty@tty1.service 2>/dev/null | grep -q '/dev/null'; then
        pass "getty@tty1 masked"
    else
        fail "getty@tty1 masked"
    fi

    if unsquashfs -cat "$SQ" etc/systemd/system/sddm.service.d/10-aurelia.conf 2>/dev/null \
        | grep -q '^After='; then
        pass "sddm plymouth drop-in installed"
    else
        fail "sddm plymouth drop-in installed"
    fi

    # Ubuntu's NetworkManager policy leaves wired devices to netplan; AURELIA has
    # no netplan, so the live session stayed offline until this was overridden.
    unsquashfs -cat "$SQ" usr/lib/NetworkManager/conf.d/10-globally-managed-devices.conf 2>/dev/null \
        | grep -q '^unmanaged-devices=$' \
        && pass "NetworkManager manages wired devices" \
        || fail "NetworkManager manages wired devices"

    unsquashfs -cat "$SQ" usr/share/calamares/branding/aurelia/branding.desc 2>/dev/null \
        | grep -q 'componentName: aurelia' \
        && pass "Calamares branding is on the search path" \
        || fail "Calamares branding is on the search path"

    unsquashfs -cat "$SQ" etc/calamares/modules/users.conf 2>/dev/null \
        | grep -q 'doAutologin: false' \
        && pass "installer does not autologin the installed user" \
        || fail "installer does not autologin the installed user"

    unsquashfs -cat "$SQ" etc/calamares/modules/shellprocess-targetcleanup.conf 2>/dev/null \
        | grep -q 'userdel -r aurelia' \
        && pass "installer removes the live account from the target" \
        || fail "installer removes the live account from the target"

    if unsquashfs -ll "$SQ" usr/bin/plasma-welcome 2>/dev/null | grep -q 'usr/bin/plasma-welcome'; then
        fail "KDE Welcome Center is not installed"
    else
        pass "KDE Welcome Center is not installed"
    fi

    unsquashfs -cat "$SQ" etc/plymouth/plymouthd.conf 2>/dev/null | grep -q 'Theme=aurelia' \
        && pass "plymouth theme selected" || fail "plymouth theme selected"
    unsquashfs -cat "$SQ" etc/X11/default-display-manager 2>/dev/null | grep -q '/usr/bin/sddm' \
        && pass "default display manager is sddm" || fail "default display manager is sddm"
    unsquashfs -cat "$SQ" etc/sddm.conf.d/zzzz-aurelia-live.conf 2>/dev/null | grep -q 'User=aurelia' \
        && pass "sddm live autologin configured" || fail "sddm live autologin configured"

    # A Conflicts=display-manager.service in any unit wanted by a normal boot
    # target makes systemd delete SDDM's start job while building the
    # transaction, before conditions are evaluated (VERIFICATION.md root cause 7).
    if unsquashfs -cat "$SQ" etc/systemd/system/aurelia-recovery.service 2>/dev/null \
        | grep -q '^Conflicts=.*display-manager'; then
        fail "recovery unit does not conflict with the display manager"
    else
        pass "recovery unit does not conflict with the display manager"
    fi

    unsquashfs -cat "$SQ" etc/grub.d/41_aurelia_recovery 2>/dev/null \
        | grep -q 'systemd.mask=sddm.service' \
        && pass "recovery entry masks the display manager" \
        || fail "recovery entry masks the display manager"

    # World-writable unit files are ignored with a warning and are a local
    # root-escalation path; the payload arrives from the Windows worktree as 0777.
    unsafe=""
    for unit in aurelia-firstboot.service aurelia-boot-guard.service \
        aurelia-boot-success.service aurelia-recovery.service; do
        # unsquashfs -ll prints every parent directory before the file itself.
        mode="$(unsquashfs -ll "$SQ" "etc/systemd/system/$unit" 2>/dev/null | tail -1 | awk '{print $1}')"
        [ "$mode" = "-rw-r--r--" ] || unsafe="$unsafe $unit($mode)"
    done
    if [ -z "$unsafe" ]; then
        pass "systemd units are mode 0644"
    else
        fail "systemd units are mode 0644:$unsafe"
    fi
fi

INITRD=$(ls -1 "$ROOT"/binary/casper/initrd.img-* 2>/dev/null | head -1)
if [ -n "$INITRD" ] && command -v lsinitramfs >/dev/null 2>&1; then
    lsinitramfs "$INITRD" | grep -q 'themes/aurelia/aurelia.script' \
        && pass "initramfs carries the AURELIA plymouth theme" || fail "initramfs carries the AURELIA plymouth theme"
    lsinitramfs "$INITRD" | grep -q 'overlayfs/overlay.ko' \
        && pass "initramfs carries overlayfs" || fail "initramfs carries overlayfs"
else
    fail "casper initramfs not found"
fi

if [ -s "$ROOT/binary/md5sum.txt" ]; then
    pass "md5sum manifest present ($(wc -l < "$ROOT/binary/md5sum.txt") files)"
else
    fail "md5sum manifest present"
fi

echo "-----"
if [ "$fails" -eq 0 ]; then
    echo "VERIFY ISO: PASS"
else
    echo "VERIFY ISO: $fails FAILURE(S)"
fi
exit "$fails"
