#!/usr/bin/env bash
# Build, flash, and run an on-device Unity test variant, printing results and
# exiting non-zero on failure. See esp_idf_project_configuration.json for the
# full list of variants (e.g. test_refresh, test_hardware).
#
# Usage:
#   agents/run_test.sh <variant> [--port /dev/ttyACM0] [--app-only] [--no-build] [--timeout SECONDS]
#
# --app-only  skip reflashing the bootloader/partition table (they rarely
#             change between test iterations; only the app changed most of
#             the time, so this is the common fast path).
# --no-build  skip the idf.py build step entirely (flash+run only).
# --timeout   safety-net ceiling in seconds for the on-device run (default 60);
#             the script exits as soon as the Unity summary line appears, so
#             this is rarely hit on a passing/failing run -- only a hang.
set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "usage: $0 <variant> [--port PORT] [--app-only] [--no-build] [--timeout SECONDS]" >&2
  exit 1
fi

VARIANT="$1"
shift

PORT=""
APP_ONLY=0
NO_BUILD=0
TIMEOUT=60

while [[ $# -gt 0 ]]; do
  case "$1" in
    --port) PORT="$2"; shift 2 ;;
    --app-only) APP_ONLY=1; shift ;;
    --no-build) NO_BUILD=1; shift ;;
    --timeout) TIMEOUT="$2"; shift 2 ;;
    *) echo "unknown arg: $1" >&2; exit 1 ;;
  esac
done

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

if [[ -z "$PORT" ]]; then
  PORT=$(ls /dev/ttyACM0 /dev/ttyUSB0 2>/dev/null | head -1) || true
  if [[ -z "$PORT" ]]; then
    echo "No serial device found at /dev/ttyACM0 or /dev/ttyUSB0; pass --port explicitly." >&2
    exit 1
  fi
fi

# idf.py never reads esp_idf_project_configuration.json itself (it's a
# VS Code ESP-IDF extension format) -- pull this variant's executable name
# and sdkconfig layering out of it here so it only has to be transcribed once.
CONFIG_LINE=$(python3 - "$VARIANT" <<'PYEOF'
import json
import sys

variant = sys.argv[1]
with open("esp_idf_project_configuration.json") as f:
    d = json.load(f)
if variant not in d:
    sys.exit(f"unknown variant '{variant}'; choices: {', '.join(d)}")
cfg = d[variant]
defaults = ";".join(cfg["build"]["sdkconfigDefaults"])
sdkconfig = cfg["build"]["sdkconfigFilePath"].replace("${workspaceFolder}/", "")
print(cfg["env"]["executable"], defaults, sdkconfig, sep="\t")
PYEOF
)
IFS=$'\t' read -r EXECUTABLE SDKCONFIG_DEFAULTS SDKCONFIG_PATH <<<"$CONFIG_LINE"

IDF_EXPORT=$(python3 -c "
import json, os
with open(os.path.expanduser('~/.espressif/idf-env.json')) as f:
    d = json.load(f)
path = next(iter(d['idfInstalled'].values()))['path']
print(os.path.join(path, 'export.sh'))
")

# shellcheck disable=SC1090
source "$IDF_EXPORT" >/dev/null

export executable="$EXECUTABLE"
export IDF_CCACHE_ENABLE=1

BUILD_DIR="builds/$VARIANT"

if [[ "$NO_BUILD" -eq 0 ]]; then
  idf.py -B "$BUILD_DIR" \
    -DSDKCONFIG_DEFAULTS="$SDKCONFIG_DEFAULTS" \
    -DSDKCONFIG="$SDKCONFIG_PATH" \
    build
fi

if [[ "$APP_ONLY" -eq 1 ]]; then
  idf.py -B "$BUILD_DIR" -p "$PORT" app-flash
else
  idf.py -B "$BUILD_DIR" -p "$PORT" flash
fi

exec python3 "$REPO_ROOT/agents/monitor_unity.py" "$PORT" "$TIMEOUT"
