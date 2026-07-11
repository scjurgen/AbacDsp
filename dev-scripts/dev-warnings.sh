#!/bin/bash
# Clean rebuild of build-tests/ and report every non-3rdparty compiler warning.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/_env.sh"

cmake --build "$BUILD_DIR" --target clean > /dev/null 2>&1 || true

LOG="$SCRATCH_DIR/warnings_full_build.log"
JOBS="$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)"
cmake --build "$BUILD_DIR" --target run_unit_tests -- -j"$JOBS" > "$LOG" 2>&1 || true

echo "--- non-3rdparty warnings ---"
grep -B1 "warning:" "$LOG" \
    | grep -oE "^${ROOT_DIR}/[^:]+:[0-9]+:[0-9]+: warning: .*" \
    | grep -v "/3rdparty/" | sort -u || echo "(none)"

echo
echo "--- test summary ---"
grep -E "tests passed|FAILED" "$LOG" | tail -5

echo
echo "Full log: $LOG"
