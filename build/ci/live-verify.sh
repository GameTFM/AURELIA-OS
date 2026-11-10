#!/bin/bash
# One boot, one guest transcript: display manager, Plasma session, NetworkManager
# wired link, DNS, stray welcome windows, and NOVAIR launched into the real
# Wayland session. Writes a screenshot of the resulting screen.
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ISO="${1:-$ROOT/build/ci/artifact/aurelia-1.0-solstice.iso}"
PREFIX="${2:-live}"
LOG="$ROOT/build/ci/guest-session-$PREFIX.log"

APPEND="boot=casper config live-media-path=/casper quiet splash console=ttyS0,115200 username=aurelia hostname=aurelia locale=en_US.UTF-8 keyboard-layouts=us" \
    bash "$ROOT/build/ci/boot-serial.sh" "$ISO" &

python "$ROOT/build/ci/guest-run.py" --port 5557 --out "$LOG" \
    --boot-timeout 900 --cmd-timeout 300 \
    --cmd 'sleep 150; systemctl is-active sddm NetworkManager; systemctl is-system-running; systemctl --failed --no-pager' \
    --cmd 'pgrep -a kwin_wayland | head -2; pgrep -a plasmashell | head -2; ls /dev/dri' \
    --cmd 'nmcli -t device; ip -brief addr; ip route | head -3; getent hosts example.com || echo DNS_FAIL' \
    --cmd 'resolvectl status 2>&1 | head -10; echo ---; cat /etc/resolv.conf' \
    --cmd 'ps -eo pid,ppid,cmd | grep -i "[p]lasma-welcome" || echo NO_WELCOME_WINDOW; echo ---; ls /etc/xdg/autostart/' \
    --cmd 'uid=$(id -u); export XDG_RUNTIME_DIR=/run/user/$uid WAYLAND_DISPLAY=wayland-0; nohup novair https://example.com >/tmp/novair.log 2>&1 & sleep 45; echo "--- log"; cat /tmp/novair.log; echo "--- proc"; pgrep -a -f "[n]ovair" | head -3'

python "$ROOT/build/ci/qemu-screen.py" --port 5556 --out "$PREFIX-screen.png" || true
echo "live verify complete: log=$LOG screenshot=$ROOT/build/ci/$PREFIX-screen.png"
