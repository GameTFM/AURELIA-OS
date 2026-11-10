#!/bin/bash
# Boot the AURELIA ISO once and collect why networking stays down, which process
# starts KDE's Welcome Center, and why NOVAIR does not stay running in the live
# Plasma Wayland session.
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ISO="${1:-$ROOT/build/ci/artifact/aurelia-1.0-solstice.iso}"
PREFIX="${2:-netdiag}"
LOG="$ROOT/build/ci/guest-session-$PREFIX.log"

APPEND="boot=casper config live-media-path=/casper quiet splash console=ttyS0,115200 username=aurelia hostname=aurelia locale=en_US.UTF-8 keyboard-layouts=us" \
    bash "$ROOT/build/ci/boot-serial.sh" "$ISO" &

python "$ROOT/build/ci/guest-run.py" --port 5557 --out "$LOG" \
    --boot-timeout 900 --cmd-timeout 240 \
    --cmd 'sleep 90; systemctl is-active NetworkManager sddm; nmcli -t device; ip -brief addr' \
    --cmd 'systemctl status NetworkManager --no-pager 2>&1 | head -18' \
    --cmd 'cat /usr/lib/NetworkManager/conf.d/10-globally-managed-devices.conf; echo ---; nmcli -f GENERAL.STATE,GENERAL.CONNECTION device show ens3 2>&1 | head -10' \
    --cmd 'cat /etc/resolv.conf; echo ---; systemctl is-active systemd-resolved; resolvectl status 2>&1 | head -12' \
    --cmd 'ps -eo pid,ppid,cmd | grep -i "[w]elcome"; echo ---; grep -iE "autostart|X-KDE|OnlyShowIn" /usr/share/applications/org.kde.plasma-welcome.desktop; echo ---; ls /etc/xdg/autostart/' \
    --cmd 'ls -l /run/user/$(id -u)/wayland-0 /run/user/$(id -u) 2>&1 | head -5; echo ---; python3 /usr/share/novair/novair.py --self-test 2>&1 | tail -8' \
    --cmd 'uid=$(id -u); export XDG_RUNTIME_DIR=/run/user/$uid WAYLAND_DISPLAY=wayland-0 QT_QPA_PLATFORM=wayland; nohup novair https://example.com >/tmp/novair.log 2>&1 & sleep 35; echo "--- log"; cat /tmp/novair.log; echo "--- proc"; pgrep -a -f "[n]ovair" | head -4'

python "$ROOT/build/ci/qemu-screen.py" --port 5556 --out "$PREFIX-screen.png" || true
echo "diag complete: log=$LOG screenshot=$ROOT/build/ci/$PREFIX-screen.png"
