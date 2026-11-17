#!/bin/bash
# Second diagnostic boot: query plasmashell for its panels, then run NOVAIR with
# the Wayland platform and software GL (no render node), logging Qt's own state.
set -uo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ISO="$ROOT/build/ci/artifact/aurelia-1.0-solstice.iso"
LOG="$ROOT/build/ci/v12-diag3.log"

taskkill //IM qemu-system-x86_64.exe //F >/dev/null 2>&1 || true
sleep 2

APPEND="boot=casper config live-media-path=/casper quiet splash console=ttyS0,115200 username=aurelia hostname=aurelia locale=en_US.UTF-8 keyboard-layouts=us" \
  bash "$ROOT/build/ci/boot-serial.sh" "$ISO" &

python "$ROOT/build/ci/guest-run.py" --port 5557 --out "$LOG" \
  --boot-timeout 780 --cmd-timeout 240 \
  --cmd 'sleep 90; systemctl is-active sddm; qdbus6 org.kde.plasmashell /PlasmaShell org.kde.PlasmaShell.evaluateScript "print(panels().length)" 2>&1; qdbus6 org.kde.plasmashell /PlasmaShell org.kde.PlasmaShell.evaluateScript "for (var i=0;i<panels().length;i++) print(panels()[i].location)" 2>&1' \
  --cmd 'uid=$(id -u); export XDG_RUNTIME_DIR=/run/user/$uid WAYLAND_DISPLAY=wayland-0 QT_QPA_PLATFORM=wayland QT_OPENGL=software LIBGL_ALWAYS_SOFTWARE=1 QT_QUICK_BACKEND=software QTWEBENGINE_CHROMIUM_FLAGS="--disable-gpu --enable-unsafe-swiftshader --disable-dev-shm-usage" QT_LOGGING_RULES="qt.qpa.*=true"; nohup novair https://example.com >/tmp/n3.log 2>&1 & sleep 45; echo ---log; tail -25 /tmp/n3.log; echo ---proc; pgrep -a -f "[n]ovair" | head -2' \
  --cmd 'dpkg -l libxcb-cursor0 2>/dev/null | tail -1; ls /run/user/$(id -u)/xauth_* 2>/dev/null | head -2; command -v qdbus6 qdbus xwininfo'

python "$ROOT/build/ci/qemu-screen.py" --port 5556 --out v12-diag3.png || true
echo DIAG3_DONE
