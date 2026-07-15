#!/bin/bash
# Fast per-file coverage loop for the branch-coverage work: rebuild ONE test
# target in build-coverage, clear its stale .gcda, run it, then report the
# coverage of ONE source file plus its actionable branch-gap detail.
#
# Usage:
#   dev-cov-file.sh <TestTarget> <src-filter>
#     <TestTarget>  a test executable/target, e.g. FiltersTests
#     <src-filter>  a path under src/includes/, e.g. Filters/OnePoleFilter.h
#                   (a bare substring also works; it is matched by gcovr/branch_gaps)
#
# Example:
#   dev-cov-file.sh FiltersTests Filters/OnePoleFilter.h
#
# Assumes build-coverage/ is already configured (run dev-coverage.sh once first).
#
# Caveat: this rebuilds only ONE target. If the source file is #include'd by
# several test targets, the others keep stale instrumentation and gcovr aborts
# on a line-number mismatch ("Got function ... on multiple lines"). For a header
# shared across targets, run the full dev-coverage.sh instead.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build-coverage"

if [ "$#" -lt 2 ]; then
    echo "usage: dev-cov-file.sh <TestTarget> <src-filter>" >&2
    exit 2
fi
TARGET="$1"
FILTER="$2"

if [ ! -d "$BUILD_DIR" ]; then
    echo "build-coverage/ not configured yet; run dev-scripts/dev-coverage.sh once first." >&2
    exit 1
fi

JOBS="$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)"
cmake --build "$BUILD_DIR" --target "$TARGET" -- -j"$JOBS"

# Recompiling a TU invalidates its old .gcda ("corrupt arc tag" on merge), so
# drop this target's counters before the fresh run instead of merging into them.
find "$BUILD_DIR/test/CMakeFiles/$TARGET.dir" -name '*.gcda' -delete 2>/dev/null || true

"$BUILD_DIR/test/$TARGET"

# gcovr's --filter is a regex over absolute paths; anchor it under src/includes.
GCOVR_FILTER="$ROOT_DIR/src/includes/.*$FILTER"

echo
echo "=== coverage: $FILTER ==="
gcovr --root "$ROOT_DIR" --filter "$GCOVR_FILTER" \
    --exclude-unreachable-branches --exclude-throw-branches --decisions \
    --print-summary "$BUILD_DIR" 2>/dev/null | grep -E "^(lines|functions|branches|decisions):" || true

echo
echo "=== branch gaps: $FILTER ==="
python3 "$SCRIPT_DIR/branch_gaps.py" --build-dir "$BUILD_DIR" --no-report --file "$FILTER" || true
