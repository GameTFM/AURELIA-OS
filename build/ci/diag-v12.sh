#!/bin/bash
# One diagnostic boot: panel containers in the live appletsrc, and NOVAIR driven
# through XWayland with DISPLAY set (what a desktop-launched app actually gets).
set -uo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ISO="$ROOT/build/ci/artifact/aurelia-1.0-solstice.iso"
LOG="$ROOT/build/ci/v12-diag2.log"

APPEND="boot=casper config live-media-path=/casper quiet splash console=ttyS0,115200 username=aurelia hostname=aurelia locale=en_US.UTF-8 keyboard-layouts=us" \
  bash "$ROOT/build/ci/boot-serial.sh" "$ISO" &

python "$ROOT/build/ci/guest-run.py" --port 5557 --out "$LOG" \
  --boot-timeout 780 --cmd-timeout 240 \
  --cmd 'sleep 90; systemctl is-active sddm; f=$HOME/.config/plasma-org.kde.plasma.desktop-appletsrc; echo "panels=$(grep -c "plugin=org.kde.panel" "$f")"; grep -n "plugin=org.kde.panel" "$f" | head -4; grep -n "^\[Containments\]" "$f" | head -8; wc -l "$f"' \
  --cmd 'command -v xwininfo || echo NO_XWININFO; DISPLAY=:0 QT_QPA_PLATFORM=xcb nohup novair https://example.com >/tmp/n2.log 2>&1 & sleep 45; echo ---log; tail -8 /tmp/n2.log; echo ---proc; pgrep -a -f "[n]ovair" | head -2' \
  --cmd 'command -v xwininfo >/dev/null && DISPLAY=:0 xwininfo -root -tree 2>/dev/null | grep -ci novair || echo NO_XWININFO; DISPLAY=:0 xprop -root _NET_CLIENT_LIST 2>/dev/null | head -2'

python "$ROOT/build/ci/qemu-screen.py" --port 5556 --out v12-diag2.png || true
echo DIAG_DONE
