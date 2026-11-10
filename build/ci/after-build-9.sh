#!/bin/bash
# Wait for build 9 to report its exit status, then copy the ISO to the artifact
# path, re-hash it, run the payload verifier and boot the live session check.
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
RUNNER="$ROOT/build/ci/build9-runner.log"
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
    echo "FAIL: build failed; see build/wsl-iso-build-9.log"
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
    2>&1 | tee "$ROOT/build/ci/verify-9.log"

echo "=== live session check ==="
bash "$ROOT/build/ci/boot-and-check.sh" "$ISO" check9
echo "DONE9"
