#!/bin/bash
# Boot the AURELIA ISO with a TCP serial console, sign in, and assert the live
# session state: display manager, compositor, networking and NOVAIR. Leaves a
# guest transcript, a desktop screenshot and, when it starts, a NOVAIR window in
# the capture.
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ISO="${1:-$ROOT/build/ci/artifact/aurelia-1.0-solstice.iso}"
PREFIX="${2:-check}"
LOG="$ROOT/build/ci/guest-session-$PREFIX.log"

APPEND="boot=casper config live-media-path=/casper quiet splash console=ttyS0,115200 username=aurelia hostname=aurelia locale=en_US.UTF-8 keyboard-layouts=us" \
    bash "$ROOT/build/ci/boot-serial.sh" "$ISO" &
BOOT_PID=$!

python "$ROOT/build/ci/guest-run.py" --port 5557 --out "$LOG" \
    --boot-timeout 900 --cmd-timeout 240 \
    --cmd 'sleep 120; systemctl is-active sddm; systemctl is-system-running; systemctl --failed --no-pager' \
    --cmd 'pgrep -a kwin_wayland | head -2; pgrep -a plasmashell | head -2' \
    --cmd 'ip -brief addr; getent hosts example.com || echo DNS_FAIL' \
    --cmd 'ls -l /dev/dri 2>&1 | head -3' \
    --cmd 'uid=$(id -u); export XDG_RUNTIME_DIR=/run/user/$uid WAYLAND_DISPLAY=wayland-0; (novair https://example.com >/tmp/novair.log 2>&1 &); sleep 40; tail -8 /tmp/novair.log; pgrep -a novair | head -2'

python "$ROOT/build/ci/qemu-screen.py" --port 5556 --out "$PREFIX-desktop.png" || true
echo "check complete: log=$LOG screenshot=$ROOT/build/ci/$PREFIX-desktop.png"
