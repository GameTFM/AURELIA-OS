#!/bin/bash
set -u
cd "$(dirname "$0")/../.."
TOKEN=$(printf 'protocol=https\nhost=github.com\n\n' | git credential fill | sed -n 's/^password=//p')
SHA=$(git rev-parse HEAD)
RUN=""
for i in $(seq 1 30); do
  RUN=$(curl -fsSL -H "Authorization: Bearer $TOKEN" -H 'Accept: application/vnd.github+json' "https://api.github.com/repos/GameTFM/AURELIA-OS/actions/runs?head_sha=$SHA" | python3 -c 'import json,sys; d=json.load(sys.stdin); r=d["workflow_runs"]; print(r[0]["id"] if r else "")' 2>/dev/null)
  [ -n "$RUN" ] && break
  sleep 5
done
echo "RUN=$RUN SHA=$SHA"
[ -z "$RUN" ] && exit 1
for i in $(seq 1 100); do
  STATE=$(curl -fsSL -H "Authorization: Bearer $TOKEN" -H 'Accept: application/vnd.github+json' "https://api.github.com/repos/GameTFM/AURELIA-OS/actions/runs/$RUN" | python3 -c 'import json,sys; r=json.load(sys.stdin); print(r.get("status")+":"+str(r.get("conclusion")))' 2>/dev/null || echo "api-error:")
  echo "$(date -u +%H:%M:%S) $STATE"
  case "$STATE" in
    completed:success)
      curl -fsSL -H "Authorization: Bearer $TOKEN" -H 'Accept: application/vnd.github+json' "https://api.github.com/repos/GameTFM/AURELIA-OS/actions/runs/$RUN/artifacts" -o build/ci/artifacts.json
      python3 -c 'import json; d=json.load(open("build/ci/artifacts.json")); print([(a["id"],a["name"],a["size_in_bytes"],a["expired"]) for a in d.get("artifacts",[])])'
      ID=$(python3 -c 'import json; d=json.load(open("build/ci/artifacts.json")); print(d["artifacts"][0]["id"] if d.get("artifacts") else "")')
      if [ -n "$ID" ]; then
        curl -fL --retry 3 -H "Authorization: Bearer $TOKEN" -o build/ci/aurelia-artifact.zip "https://api.github.com/repos/GameTFM/AURELIA-OS/actions/artifacts/$ID/zip"
        rm -rf build/ci/artifact; mkdir -p build/ci/artifact
        python3 -c 'import zipfile; zipfile.ZipFile("build/ci/aurelia-artifact.zip").extractall("build/ci/artifact")'
        find build/ci/artifact -type f -exec ls -la {} \;
        echo ARTIFACT_READY
      fi
      exit 0;;
    completed:*)
      echo "BUILD_DONE_$STATE"; exit 1;;
  esac
  sleep 30
done
echo WAIT_TIMEOUT
