#!/bin/bash
# Control test: are ordinary KDE windows visible in this VM at all?
set -uo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ISO="$ROOT/build/ci/artifact/aurelia-1.0-solstice.iso"
LOG="$ROOT/build/ci/v12-diag4.log"
taskkill //IM qemu-system-x86_64.exe //F >/dev/null 2>&1 || true
sleep 2
APPEND="boot=casper config live-media-path=/casper quiet splash console=ttyS0,115200 username=aurelia hostname=aurelia locale=en_US.UTF-8 keyboard-layouts=us" \
  bash "$ROOT/build/ci/boot-serial.sh" "$ISO" &
python "$ROOT/build/ci/guest-run.py" --port 5557 --out "$LOG" \
  --boot-timeout 420 --cmd-timeout 200 \
  --cmd 'sleep 60; systemctl is-active sddm; ls -l $HOME/.config/plasma-org.kde.plasma.desktop-appletsrc 2>&1; pgrep -a plasmashell | head -1; pgrep -a kwin_wayland | head -1' \
  --cmd 'uid=$(id -u); export XDG_RUNTIME_DIR=/run/user/$uid WAYLAND_DISPLAY=wayland-0 DISPLAY=:0 XAUTHORITY=$(ls /run/user/$uid/xauth_* 2>/dev/null | head -1); nohup konsole >/tmp/k.log 2>&1 & nohup kcalc >/tmp/kc.log 2>&1 & sleep 25; pgrep -a konsole | head -1; pgrep -a kcalc | head -1; tail -3 /tmp/k.log'
python "$ROOT/build/ci/qemu-screen.py" --port 5556 --out v12-diag4.png || true
echo DIAG4_DONE
