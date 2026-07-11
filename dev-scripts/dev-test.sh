#!/bin/bash
# Build + run tests via ctest, without cd.
# Usage: dev-test.sh [ctest-regex]   (regex filters test suite names, e.g. "Delays|Modulation")
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/_env.sh"

REGEX="${1:-}"

"$SCRIPT_DIR/dev-build.sh" unit_tests

if [ -n "$REGEX" ]; then
    ctest --test-dir "$BUILD_DIR" --output-on-failure -R "$REGEX"
else
    ctest --test-dir "$BUILD_DIR" --output-on-failure
fi
