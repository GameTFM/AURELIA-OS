#!/usr/bin/env bash
# Publish the built Apex package as a GitHub release asset.
# Uses the same stored credential as git push; it is never printed.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
REPO="${REPO:-GameTFM/AURELIA-OS}"
TAG="${1:-apex-v1.0.0}"
DEB="${2:-$ROOT/build/ci/artifact/apex_1.0.0_amd64.deb}"
NAME="Apex browser 1.0.0 (preview)"

[ -f "$DEB" ] || { echo "package not found: $DEB" >&2; exit 1; }
TOKEN=$(printf 'protocol=https\nhost=github.com\n\n' | git credential fill | sed -n 's/^password=//p')
[ -n "$TOKEN" ] || { echo "no GitHub credential available" >&2; exit 1; }

api() { curl -fsSL -H "Authorization: Bearer $TOKEN" -H "Accept: application/vnd.github+json" "$@"; }

# Keep JSON in the workspace: Git Bash and Windows Python disagree about /tmp.
RELEASE_JSON="$ROOT/build/ci/apex-release.json"
mkdir -p "$(dirname "$RELEASE_JSON")"
UPLOAD=""
if api "https://api.github.com/repos/$REPO/releases/tags/$TAG" -o "$RELEASE_JSON" 2>/dev/null; then
    UPLOAD=$(python3 -c 'import json,sys;print(json.load(open(sys.argv[1]))["upload_url"].split("{")[0])' "$RELEASE_JSON")
    echo "using existing release $TAG"
else
    BODY_JSON="$ROOT/build/ci/apex-release-body.json"
    python3 - "$REPO" "$TAG" "$NAME" > "$BODY_JSON" <<'PY'
import json, sys
repo, tag, name = sys.argv[1:4]
print(json.dumps({
    "tag_name": tag,
    "name": name,
    "draft": False,
    "body": "Standalone Apex browser package for Ubuntu 26.04.\n\n"
            "Apex is the browser that ships with AURELIA OS, built on Qt WebEngine. "
            "This is a preview: on machines without a DRM render node it starts "
            "without presenting a window. Do not treat it as a finished browser.\n\n"
            "Install with: sudo apt install ./apex_1.0.0_amd64.deb",
}))
PY
    api -X POST "https://api.github.com/repos/$REPO/releases" \
        -H "Content-Type: application/json" \
        --data-binary "@$BODY_JSON" -o "$RELEASE_JSON"
    UPLOAD=$(python3 -c 'import json,sys;print(json.load(open(sys.argv[1]))["upload_url"].split("{")[0])' "$RELEASE_JSON")
    echo "created release $TAG"
fi
[ -n "$UPLOAD" ] || { echo "could not resolve an upload URL" >&2; exit 1; }

FILENAME=$(basename "$DEB")
echo "uploading $FILENAME ($(stat -c %s "$DEB") bytes)"
curl -fsSL -X POST \
    -H "Authorization: Bearer $TOKEN" \
    -H "Accept: application/vnd.github+json" \
    -H "Content-Type: application/octet-stream" \
    --data-binary "@$DEB" \
    "$UPLOAD?name=$FILENAME" -o /dev/null
echo "PUBLISHED $TAG/$FILENAME"
python3 -c 'import json,sys;print(json.load(open(sys.argv[1]))["html_url"])' "$RELEASE_JSON"
