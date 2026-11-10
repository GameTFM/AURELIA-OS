#!/bin/bash
# Wait for build 12, publish the artifact, verify the payload, then run one live
# boot covering the desktop, networking, DNS, stray windows and NOVAIR.
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
RUNNER="$ROOT/build/ci/build12-runner.log"
LOG="$ROOT/build/wsl-iso-build-12.log"
ISO="$ROOT/build/ci/artifact/aurelia-1.0-solstice.iso"

echo "waiting for BUILD_EXIT in $RUNNER"
for _ in $(seq 1 240); do
    if grep -q '^BUILD_EXIT=' "$RUNNER" 2>/dev/null; then
        break
    fi
    sleep 15
done

RESULT="$(grep '^BUILD_EXIT=' "$RUNNER" 2>/dev/null | tail -1)"
if [ -z "$RESULT" ]; then
    echo "FAIL: the build never reported an exit status"
    exit 1
fi
echo "$RESULT"
if [ "$RESULT" != "BUILD_EXIT=0" ]; then
    echo "FAIL: build failed; first errors in $LOG:"
    grep -nE '^(E:|ERROR|Hook .* failed|AURELIA:)' "$LOG" 2>/dev/null | head -12
    exit 1
fi

wsl -d AURELIA-BUILD -u root -- bash -lc \
    'cp -f /opt/aurelia/distro/live-image-amd64.hybrid.iso /mnt/c/Users/karth/Downloads/AURELIA-OS/build/ci/artifact/aurelia-1.0-solstice.iso && echo COPY_OK'
if [ ! -s "$ISO" ]; then
    echo "FAIL: artifact copy missing"
    exit 1
fi

cd "$ROOT/build/ci/artifact" || exit 1
sha256sum aurelia-1.0-solstice.iso | sed 's|^\(\w*\) .*|\1  aurelia-1.0-solstice.iso|' > aurelia-1.0-solstice.iso.sha256
cat aurelia-1.0-solstice.iso.sha256
ls -l aurelia-1.0-solstice.iso

echo "=== payload verifier ==="
wsl -d AURELIA-BUILD -u root -- bash -lc \
    'tr -d "\r" < /mnt/c/Users/karth/Downloads/AURELIA-OS/build/ci/verify-iso.sh > /tmp/verify-iso.sh; bash /tmp/verify-iso.sh' \
    2>&1 | tee "$ROOT/build/ci/verify-12.log"

echo "=== live verification ==="
bash "$ROOT/build/ci/live-verify.sh" "$ISO" v12
echo "DONE12"
