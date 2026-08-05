#!/bin/bash
# Build the API documentation into docs/html/.
#
# Usage:
#   dev-docs.sh            # build, report warnings, print the output path
#   dev-docs.sh --open     # build, then open the result in the default browser
#
# Doxygen runs with WARN_AS_ERROR=FAIL_ON_WARNINGS, so a malformed command or a
# broken cross-reference fails the build rather than producing a quietly wrong
# page.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/_env.sh"

OPEN=0
if [ "${1:-}" = "--open" ]; then
    OPEN=1
fi

if ! command -v doxygen > /dev/null 2>&1; then
    echo "doxygen not found. Install it with: brew install doxygen"
    exit 1
fi

LOG="$SCRATCH_DIR/doxygen.log"
OUT="$ROOT_DIR/docs/html/index.html"

# Doxyfile paths are relative to the project root, so run from there.
if ! (cd "$ROOT_DIR" && doxygen docs/Doxyfile) > "$LOG" 2>&1; then
    echo "DOXYGEN FAILED. Last 40 lines:"
    tail -n 40 "$LOG"
    echo "Full log: $LOG"
    exit 1
fi

if [ -s "$LOG" ]; then
    echo "--- doxygen warnings ---"
    cat "$LOG"
else
    echo "no doxygen warnings"
fi

echo "Output: $OUT"
echo "Full log: $LOG"

if [ "$OPEN" -eq 1 ]; then
    open "$OUT"
fi
