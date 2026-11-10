#!/bin/bash
# Split the finished AURELIA ISO and publish it as a GitHub release.
# GitHub limits each release asset to 2 GiB, so the ISO ships as numbered parts
# plus checksum files and join instructions. The full ISO itself is not
# uploaded because it exceeds that per-asset limit.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ISO="${1:-$ROOT/distro/live-image-amd64.hybrid.iso}"
TAG="${2:-v1.0.0-solstice}"
NAME="${3:-AURELIA OS 1.0 Solstice}"
REPO="GameTFM/AURELIA-OS"
OUT="$ROOT/build/ci/release"
TOKEN=$(printf 'protocol=https\nhost=github.com\n\n' | git credential fill | sed -n 's/^password=//p')

[ -f "$ISO" ] || { echo "ISO not found: $ISO" >&2; exit 1; }
[ -n "$TOKEN" ] || { echo "no GitHub token available" >&2; exit 1; }

rm -rf "$OUT"; mkdir -p "$OUT"
BASE="aurelia-1.0-solstice.iso"
cp "$ISO" "$OUT/$BASE"
( cd "$OUT" && sha256sum "$BASE" > "$BASE.sha256" )
# 400 MiB keeps each upload well inside the connection's timeout window.
( cd "$OUT" && split -b 400M -d -a 2 "$BASE" "$BASE.part-" )
( cd "$OUT" && sha256sum "$BASE".part-* > "$BASE.parts.sha256" )

cat > "$OUT/JOIN-THE-ISO.txt" <<'TXT'
AURELIA OS 1.0 "Solstice" ships as split ISO parts because GitHub limits each
release asset to 2 GiB.

Linux / macOS
    cat aurelia-1.0-solstice.iso.part-* > aurelia-1.0-solstice.iso
    sha256sum -c aurelia-1.0-solstice.iso.sha256

Windows (PowerShell)
    Get-Content aurelia-1.0-solstice.iso.part-* -AsByteStream | 
        Set-Content aurelia-1.0-solstice.iso -AsByteStream
    Get-FileHash aurelia-1.0-solstice.iso -Algorithm SHA256

Then verify the joined ISO against aurelia-1.0-solstice.iso.sha256 and write it
to a USB drive with balenaEtcher, Rufus (DD mode) or
    sudo dd if=aurelia-1.0-solstice.iso of=/dev/sdX bs=8M status=progress oflag=direct

The image boots on BIOS and UEFI firmware and contains the live Plasma desktop,
the AURELIA Setup assistant for installed systems, NOVAIR and the Calamares
installer. The BIOS live desktop is boot-tested; see distro/VERIFICATION.md for
the remaining UEFI, Calamares and installed-system test gaps.
TXT

RELEASE_JSON="$OUT/release.json"
python3 - "$TOKEN" "$REPO" "$TAG" "$NAME" "$OUT" "$RELEASE_JSON" <<'PY'
import json, pathlib, sys, urllib.request

token, repo, tag, name, outdir, jsonpath = sys.argv[1:]
out = pathlib.Path(outdir)
notes = (out / "JOIN-THE-ISO.txt").read_text()
body = {
    "tag_name": tag,
    "name": name,
    "body": "AURELIA OS 1.0 \"Solstice\" - Ubuntu 26.04 LTS base, KDE Plasma 6.6, "
            "the AURELIA desktop, the 13-step setup assistant, NOVAIR 1.0 and Calamares.\n\n"
            "The ISO is split into parts; join and verify it before writing it to USB.\n\n"
            "```\n" + notes + "```\n",
    "draft": False,
}
req = urllib.request.Request(
    f"https://api.github.com/repos/{repo}/releases",
    data=json.dumps(body).encode(),
    headers={"Authorization": f"Bearer {token}", "Accept": "application/vnd.github+json"},
    method="POST",
)
with urllib.request.urlopen(req) as resp:
    release = json.load(resp)
pathlib.Path(jsonpath).write_text(json.dumps(release))
print("release:", release["html_url"])
print("upload_url:", release["upload_url"])
PY

UPLOAD_URL=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["upload_url"].split("{")[0])' "$RELEASE_JSON")

upload() {
    local path="$1"
    echo "uploading $(basename "$path") ($(stat -c %s "$path") bytes)"
    curl -fsSL --retry 3 -X POST \
        -H "Authorization: Bearer $TOKEN" \
        -H "Accept: application/vnd.github+json" \
        -H "Content-Type: application/octet-stream" \
        --data-binary "@$path" \
        "$UPLOAD_URL?name=$(python3 -c 'import sys,urllib.parse; print(urllib.parse.quote(sys.argv[1]))' "$(basename "$path")")" \
        -o /dev/null
}

for part in "$OUT/$BASE".part-*; do
    upload "$part"
done
upload "$OUT/JOIN-THE-ISO.txt"
upload "$OUT/$BASE.sha256"
upload "$OUT/$BASE.parts.sha256"

echo "RELEASE_DONE $TAG"
