#!/bin/bash
# Thin wrapper around dev-scripts/check_test_coverage.py, kept at the repo
# root so the documented `./check_test_coverage.sh` invocation keeps working.
# See dev-scripts/check_test_coverage.py for the actual logic.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec python3 "$SCRIPT_DIR/dev-scripts/check_test_coverage.py" "$@"
