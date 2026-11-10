#!/bin/bash
# Wait for the WSL build to finish, copy the ISO into the artifact folder, run
# the payload verifier, then boot the ISO and capture a screenshot sequence.
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
LOG="$ROOT/build/wsl-iso-build-8.log"
ART="$ROOT/build/ci/artifact/aurelia-1.0-solstice.iso"

echo "waiting for AURELIA_BUILD_OK in $LOG"
for _ in $(seq 1 120); do
    if grep -q "AURELIA_BUILD_OK" "$LOG" 2>/dev/null; then
        break
    fi
    if grep -q "BUILD_EXIT=" "$LOG" 2>/dev/null; then
        echo "build exited without AURELIA_BUILD_OK"
        break
    fi
    sleep 15
done

if ! grep -q "AURELIA_BUILD_OK" "$LOG" 2>/dev/null; then
    echo "BUILD_DID_NOT_COMPLETE"
    exit 1
fi
echo "build complete"

wsl -d AURELIA-BUILD -u root -- bash -lc 'cp -f /opt/aurelia/distro/live-image-amd64.hybrid.iso /mnt/c/Users/karth/Downloads/AURELIA-OS/build/ci/artifact/aurelia-1.0-solstice.iso'
echo "copied artifact"

cd "$ROOT/build/ci/artifact" || exit 1
sha256sum aurelia-1.0-solstice.iso > aurelia-1.0-solstice.iso.sha256
cat aurelia-1.0-solstice.iso.sha256
ls -l aurelia-1.0-solstice.iso

echo "=== payload verification ==="
wsl -d AURELIA-BUILD -u root -- bash -lc 'tr -d "\r" < /mnt/c/Users/karth/Downloads/AURELIA-OS/build/ci/verify-iso.sh > /tmp/verify-iso.sh; bash /tmp/verify-iso.sh'
echo "VERIFY_RC=$?"

echo "=== booting for capture ==="
bash "$ROOT/build/ci/boot-and-capture.sh" "$ART" final8 9 45
