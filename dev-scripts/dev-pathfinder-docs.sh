#!/bin/bash
# Regenerate Pathfinder's documentation: the node reference inside examples/pathfinder/README.md
# (built from the registered nodes), then examples/pathfinder/scripting.html.
# Usage: dev-pathfinder-docs.sh
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/_env.sh"

TABLE="$SCRATCH_DIR/pathfinder-node-reference.md"
"$SCRIPT_DIR/dev-explore-lua.sh" "$ROOT_DIR/examples/pathfinder/tools/PrintNodeReference.cpp" --example pathfinder \
    > "$TABLE" 2> "$SCRATCH_DIR/pathfinder-docs-build.log" \
    || { echo "node reference program failed, see $SCRATCH_DIR/pathfinder-docs-build.log" >&2; exit 1; }

python3 "$SCRIPT_DIR/pathfinder_docs.py" "$TABLE"
python3 "$SCRIPT_DIR/generate_lua_docs.py" pathfinder
