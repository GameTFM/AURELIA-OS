#!/bin/bash
# Boot the artifact ISO and capture a timed sequence of screenshots, so the
# boot can be reviewed afterwards instead of polled by hand.
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ISO="${1:-$ROOT/build/ci/artifact/aurelia-1.0-solstice.iso}"
PREFIX="${2:-final}"
SHOTS="${3:-10}"
INTERVAL="${4:-45}"

bash "$ROOT/build/ci/boot-iso.sh" "$ISO" &
BOOT_PID=$!

sleep 45
for i in $(seq 1 "$SHOTS"); do
    python "$ROOT/build/ci/qemu-screen.py" --port 5556 --out "${PREFIX}-${i}.png" || true
    sleep "$INTERVAL"
done

echo "capture complete: $SHOTS frames with prefix $PREFIX"
wait "$BOOT_PID"
