#!/bin/bash
set -u
cd "$(dirname "$0")/../.."
TOKEN=$(printf 'protocol=https\nhost=github.com\n\n' | git credential fill | sed -n 's/^password=//p')
RUN=42382402182
for i in $(seq 1 80); do
    STATE=$(curl -fsSL -H "Authorization: Bearer $TOKEN" -H 'Accept: application/vnd.github+json' \
      "https://api.github.com/repos/GameTFM/AURELIA-OS/actions/runs/$RUN" \
      | python3 -c 'import json,sys; r=json.load(sys.stdin); print(r.get("status")+":"+str(r.get("conclusion")))' 2>/dev/null || echo "api-error:")
    echo "$(date -u +%H:%M:%S) $STATE"
    case "$STATE" in
      completed:success)
        ART=$(curl -fsSL -H "Authorization: Bearer $TOKEN" -H 'Accept: application/vnd.github+json' \
          "https://api.github.com/repos/GameTFM/AURELIA-OS/actions/runs/$RUN/artifacts" \
          | python3 -c 'import json,sys; d=json.load(sys.stdin); a=(d.get("artifacts") or [{}])[0]; print(a.get("id",""), a.get("archive_download_url",""))' 2>/dev/null || true)
        ID=$(printf '%s' "$ART" | cut -d' ' -f1)
        URL=$(printf '%s' "$ART" | cut -d' ' -f2-)
        if [ -n "$ID" ] && [ -n "$URL" ]; then
          echo "downloading artifact $ID"
          curl -fL --retry 3 -H "Authorization: Bearer $TOKEN" -o build/ci/aurelia-artifact.zip "$URL"
          python3 - <<'PY'
import zipfile
from pathlib import Path
out = Path("build/ci/artifact")
out.mkdir(parents=True, exist_ok=True)
with zipfile.ZipFile("build/ci/aurelia-artifact.zip") as archive:
    archive.extractall(out)
for path in sorted(out.rglob("*")):
    if path.is_file():
        print("artifact:", path, path.stat().st_size)
PY
          echo "ARTIFACT_READY"
        else
          echo "NO_ARTIFACT"
        fi
        exit 0
        ;;
      completed:*)
        echo "BUILD_COMPLETED_$STATE"
        exit 1
        ;;
    esac
    sleep 30
done
echo "WAIT_TIMEOUT"
exit 1
