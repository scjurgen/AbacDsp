#!/bin/bash
# Build one or more CMake targets in build-tests/, without cd.
# Usage: dev-build.sh [target...]   (defaults to run_unit_tests)
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/_env.sh"

TARGETS=("${@:-run_unit_tests}")
JOBS="$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)"

LOG="$SCRATCH_DIR/build.log"
if ! cmake --build "$BUILD_DIR" --target "${TARGETS[@]}" -- -j"$JOBS" > "$LOG" 2>&1; then
    echo "BUILD FAILED. Last 60 lines:"
    tail -n 60 "$LOG"
    echo "Full log: $LOG"
    exit 1
fi

echo "--- warnings/errors (excluding 3rdparty) ---"
grep -B1 -E "warning:|error:" "$LOG" \
    | grep -oE "^${ROOT_DIR}/[^:]+:[0-9]+:[0-9]+: (warning|error): .*" \
    | grep -v "/3rdparty/" | sort -u || echo "(none)"

echo "Full log: $LOG"
