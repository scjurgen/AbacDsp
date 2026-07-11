#!/bin/bash
# Configure the tests-only build directory (build-tests/), no JUCE required.
# Safe to re-run; CMake reconfigures in place.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/_env.sh"

LOG="$SCRATCH_DIR/configure.log"
cmake -S "$ROOT_DIR" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DPACKAGE_TESTS=ON \
    -DBUILD_FULL_PROJECT=OFF \
    > "$LOG" 2>&1

tail -n 20 "$LOG"
echo "Full log: $LOG"
