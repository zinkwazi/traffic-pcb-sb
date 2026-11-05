#!/usr/bin/env bash
# Build, flash, and run an on-device Unity test variant, printing results and
# exiting non-zero on failure. See CMakePresets.json's configurePresets for the
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
#
# --no-coverage  skip the gcov dump + coverage report even if the variant was
#             built with CONFIG_ESP_GCOV_ENABLE=y.
#
# Outputs (under builds/<variant>/test/, regenerated every run):
#   test_results.txt         the serial (Unity) output alone -- no build/flash
#                            logs -- for grepping.
#   coverage_summary.txt     (gcov builds only) per-file line coverage;
#                            Missing = line numbers never executed.
#   coverage_branches.txt    (gcov builds only) per-file branch coverage;
#                            Missing = lines with at least one branch never taken.
#   coverage_report/index.html  (gcov builds only) gcovr HTML report with
#                            per-file, line-annotated source pages.
#
# Coverage works by an on-the-fly `esp gcov` dump through OpenOCD after the
# tests finish (the test app idles in a loop afterwards, so no firmware hook is
# needed). An already-running OpenOCD is reused; otherwise one is started and
# stopped by this script. A coverage failure is reported on stderr but does not
# change the exit code, which always reflects the test results.
set -euo pipefail

USAGE="usage: $0 <variant> [--port PORT] [--app-only] [--no-build] [--no-coverage] [--timeout SECONDS]"

if [[ $# -lt 1 ]]; then
  echo "$USAGE" >&2
  exit 1
fi

VARIANT="$1"
shift

PORT=""
APP_ONLY=0
NO_BUILD=0
NO_COVERAGE=0
TIMEOUT=60

while [[ $# -gt 0 ]]; do
  case "$1" in
    --port) PORT="$2"; shift 2 ;;
    --app-only) APP_ONLY=1; shift ;;
    --no-build) NO_BUILD=1; shift ;;
    --no-coverage) NO_COVERAGE=1; shift ;;
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

# idf.py never reads CMakePresets.json itself (the VS Code ESP-IDF extension
# does) -- pull this variant's executable name and sdkconfig layering out of
# it here so it only has to be transcribed once.
CONFIG_LINE=$(python3 - "$VARIANT" <<'PYEOF'
import json
import sys

variant = sys.argv[1]
with open("CMakePresets.json") as f:
    presets = {p["name"]: p for p in json.load(f)["configurePresets"]}
if variant not in presets:
    sys.exit(f"unknown variant '{variant}'; choices: {', '.join(presets)}")
cfg = presets[variant]
defaults = cfg["cacheVariables"]["SDKCONFIG_DEFAULTS"]
sdkconfig = cfg["cacheVariables"]["SDKCONFIG"].replace("${sourceDir}/", "")
print(cfg["environment"]["executable"], defaults, sdkconfig, sep="\t")
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

RESULTS_DIR="$REPO_ROOT/$BUILD_DIR/test"
COVERAGE_DIR="$RESULTS_DIR/coverage_report"
COVERAGE_SUMMARY="$RESULTS_DIR/coverage_summary.txt"
COVERAGE_BRANCHES="$RESULTS_DIR/coverage_branches.txt"
mkdir -p "$RESULTS_DIR"

COVERAGE=0
if [[ "$NO_COVERAGE" -eq 0 ]] && grep -q '^CONFIG_ESP_GCOV_ENABLE=y' "$SDKCONFIG_PATH"; then
  COVERAGE=1
fi

# Always clear old coverage output so a missing report means "this run's
# coverage step failed", never "stale report from an earlier run". gcov
# accumulates counts into existing .gcda files, so those must go too.
rm -rf "$COVERAGE_DIR" "$COVERAGE_SUMMARY" "$COVERAGE_BRANCHES"
if [[ "$COVERAGE" -eq 1 ]]; then
  find "$BUILD_DIR" -name '*.gcda' -delete
fi

# gcov support is enabled in the shared test sdkconfig, but only components
# compiled with --coverage produce .gcno files; skip if nothing is instrumented.
if [[ "$COVERAGE" -eq 1 && -z "$(find "$BUILD_DIR" -name '*.gcno' -print -quit)" ]]; then
  echo "coverage: skipped, no source files in $BUILD_DIR are compiled with --coverage" >&2
  COVERAGE=0
fi

if [[ "$APP_ONLY" -eq 1 ]]; then
  idf.py -B "$BUILD_DIR" -p "$PORT" app-flash
else
  idf.py -B "$BUILD_DIR" -p "$PORT" flash
fi

set +e
python3 "$REPO_ROOT/agents/monitor_unity.py" "$PORT" "$TIMEOUT" "$RESULTS_DIR/test_results.txt"
TEST_STATUS=$?
set -e

openocd_up() { (exec 3<>/dev/tcp/127.0.0.1/6666) 2>/dev/null; }

run_coverage() {
  # Only dump after a completed run; a hung board won't answer the dump.
  if [[ "$TEST_STATUS" -eq 2 ]]; then
    echo "coverage: skipped, test run timed out" >&2
    return 1
  fi

  if ! openocd_up; then
    echo "coverage: starting OpenOCD (log: $RESULTS_DIR/openocd.log)" >&2
    openocd -f board/esp32s3-builtin.cfg >"$RESULTS_DIR/openocd.log" 2>&1 &
    OPENOCD_PID=$!
    trap 'kill "$OPENOCD_PID" 2>/dev/null || true' EXIT
    for _ in $(seq 1 30); do
      openocd_up && break
      sleep 0.5
    done
    if ! openocd_up; then
      echo "coverage: OpenOCD did not come up; see $RESULTS_DIR/openocd.log" >&2
      return 1
    fi
  else
    echo "coverage: reusing running OpenOCD" >&2
  fi

  echo "coverage: dumping gcov data via OpenOCD" >&2
  python3 "$REPO_ROOT/agents/openocd_cmd.py" "esp gcov" --timeout 120 >&2 || return 1

  if [[ -z "$(find "$BUILD_DIR" -name '*.gcda' -print -quit)" ]]; then
    echo "coverage: no .gcda files were written by the dump" >&2
    return 1
  fi

  mkdir -p "$COVERAGE_DIR"
  echo "coverage: generating report" >&2
  local gcovr_args=(-r "$REPO_ROOT"
    --gcov-executable xtensa-esp32s3-elf-gcov
    --object-directory "$REPO_ROOT/$BUILD_DIR")
  gcovr "${gcovr_args[@]}" \
    --txt-metric line --txt "$COVERAGE_SUMMARY" \
    --html-details "$COVERAGE_DIR/index.html" \
    --print-summary >&2
  # Separate run: combining metrics in one txt report drops the Missing column.
  gcovr "${gcovr_args[@]}" --txt-metric branch --txt "$COVERAGE_BRANCHES"
  echo "coverage: report at $COVERAGE_DIR/index.html; text at $COVERAGE_SUMMARY and $COVERAGE_BRANCHES" >&2
}

if [[ "$COVERAGE" -eq 1 ]]; then
  run_coverage || echo "coverage: FAILED (test results are unaffected)" >&2
fi

exit "$TEST_STATUS"
