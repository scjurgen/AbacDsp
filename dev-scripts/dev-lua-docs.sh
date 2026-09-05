#!/bin/bash
# Regenerate examples/<name>/scripting.html for the Lua-scripted examples.
#
# Usage:
#   dev-lua-docs.sh              # regenerate all Lua-scripted examples
#   dev-lua-docs.sh <example>    # regenerate just that one
#   dev-lua-docs.sh --open       # also open the (first) regenerated file in the browser
#
# Combines LUA-MANUAL.md with each example's own README.md "## Scripting"
# section into a self-contained HTML page (see generate_lua_docs.py). Not run
# automatically by the JUCE generator - re-run this by hand after editing
# LUA-MANUAL.md or an example's Scripting section.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/_env.sh"

python3 "$SCRIPT_DIR/generate_lua_docs.py" "$@"
