#!/bin/bash
# Build + run tests that only exist under the full project (BUILD_FULL_PROJECT=ON),
# e.g. a JUCE example's own unittests, which dev-test.sh/build-tests/ never reach.
#
# Reuses cmake-build-debug/ (already configured with BUILD_FULL_PROJECT=ON and JUCE
# built there, e.g. by CLion) instead of bootstrapping a second, redundant JUCE build.
# If that directory isn't configured yet, this script tells you the command to run
# once rather than silently kicking off a long reconfigure.
#
# Usage: dev-test-full.sh <cmake-target> [cmake-target...] [-- ctest-regex]
#
#   cmake-target   one or more CMake targets to build (e.g. DroneScriptEngineTests).
#   ctest-regex    optional, after a literal --, filters which discovered tests run.
#                  Omit it to run every test currently registered in the build dir.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/_env.sh"

FULL_BUILD_DIR="$ROOT_DIR/cmake-build-debug"

if [[ ! -f "$FULL_BUILD_DIR/CMakeCache.txt" ]]; then
    echo "error: $FULL_BUILD_DIR is not configured yet." >&2
    echo "Configure it once with:" >&2
    echo "  cmake -S \"$ROOT_DIR\" -B \"$FULL_BUILD_DIR\" -DBUILD_FULL_PROJECT=ON -DPACKAGE_TESTS=ON" >&2
    exit 1
fi

TARGETS=()
REGEX=""
while [[ $# -gt 0 ]]; do
    case "$1" in
        --)
            shift
            REGEX="${1:-}"
            break
            ;;
        *)
            TARGETS+=("$1")
            shift
            ;;
    esac
done

if [[ ${#TARGETS[@]} -eq 0 ]]; then
    echo "usage: dev-test-full.sh <cmake-target> [cmake-target...] [-- ctest-regex]" >&2
    exit 2
fi

JOBS="$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)"
LOG="$SCRATCH_DIR/build-full.log"
if ! cmake --build "$FULL_BUILD_DIR" --target "${TARGETS[@]}" -- -j"$JOBS" > "$LOG" 2>&1; then
    echo "BUILD FAILED. Last 60 lines:"
    tail -n 60 "$LOG"
    echo "Full log: $LOG"
    exit 1
fi

echo "--- warnings/errors (excluding 3rdparty) ---"
grep -B1 -E "warning:|error:" "$LOG" \
    | grep -oE "^${ROOT_DIR}/[^:]+:[0-9]+:[0-9]+: (warning|error): .*" \
    | grep -v "/3rdparty/" | sort -u || echo "(none)"

if [[ -n "$REGEX" ]]; then
    ctest --test-dir "$FULL_BUILD_DIR" --output-on-failure -R "$REGEX"
else
    ctest --test-dir "$FULL_BUILD_DIR" --output-on-failure
fi
