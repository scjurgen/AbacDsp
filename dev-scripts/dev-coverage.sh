#!/bin/bash
# Build + run tests with --coverage instrumentation and generate a gcovr report.
# Uses its own build directory (build-coverage/) so normal dev-test.sh builds
# are unaffected. Requires gcovr (brew install gcovr / pip install gcovr).
# Usage: dev-coverage.sh
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build-coverage"
SCRATCH_DIR="$BUILD_DIR/scratch"

mkdir -p "$SCRATCH_DIR"

CONFIGURE_LOG="$SCRATCH_DIR/configure.log"
cmake -S "$ROOT_DIR" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DPACKAGE_TESTS=ON \
    -DBUILD_FULL_PROJECT=OFF \
    -DENABLE_COVERAGE=ON \
    > "$CONFIGURE_LOG" 2>&1
tail -n 20 "$CONFIGURE_LOG"

JOBS="$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)"
BUILD_LOG="$SCRATCH_DIR/build.log"
if ! cmake --build "$BUILD_DIR" --target coverage -- -j"$JOBS" > "$BUILD_LOG" 2>&1; then
    echo "COVERAGE RUN FAILED. Last 60 lines:"
    tail -n 60 "$BUILD_LOG"
    echo "Full log: $BUILD_LOG"
    exit 1
fi

echo "Coverage report: $ROOT_DIR/COVERAGE.md"
echo "HTML report: $BUILD_DIR/coverage/index.html"
echo "Full build log: $BUILD_LOG"

# gcovr's markdown output has no decision column, so inject a Decisions row into
# the overall table of COVERAGE.md. Decision coverage counts only source-level
# decisions (if / ?: / && / || / switch), dropping the float/SIMD/library/throw
# branch noise that makes the raw Branches figure misleading here.
python3 "$SCRIPT_DIR/inject_decisions.py" "$BUILD_DIR" "$ROOT_DIR/COVERAGE.md" || true

# Refresh the actionable branch-gap report (real logic gaps vs float/SIMD noise).
python3 "$SCRIPT_DIR/branch_gaps.py" --build-dir "$BUILD_DIR" >"$SCRATCH_DIR/branch_gaps.log" 2>&1 || true
echo "Branch-gap report: $ROOT_DIR/test/BRANCH_GAPS.md"
