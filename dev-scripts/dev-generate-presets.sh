#!/bin/bash
# Regenerate src/includes/Graph/Presets/ChorusPresets.h from Graph/Presets/lua/*.lua, no cd.
# Usage: dev-generate-presets.sh [--check]   (--check exits 1 if the header is out of date)
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

python3 "$SCRIPT_DIR/generate_graph_presets.py" "$@"
