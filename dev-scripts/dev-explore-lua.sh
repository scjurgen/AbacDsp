#!/bin/bash
# Compile and run a throwaway C++ program against Lua + sol2, via dev-explore.sh.
# Bundles the flags that combination needs (Lua's .c sources compiled as C++, plus
# SOL_USING_CXX_LUA=1 so sol2's declarations match that linkage) so callers don't have
# to re-derive them by hand.
#
# Usage:
#   dev-explore-lua.sh [source.cpp] [--example NAME]... [-- extra clang args...]
#
# Same argument shape as dev-explore.sh (default source: explore/explore.cpp).
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/_env.sh"

LUA_DIR="$ROOT_DIR/3rdparty/lua"
SOL2_DIR="$ROOT_DIR/3rdparty/sol2"

LUA_SRCS=()
for f in "$LUA_DIR"/*.c; do
    case "$f" in
        */lua.c | */luac.c | */onelua.c | */ltests.c) continue ;;
    esac
    LUA_SRCS+=("$f")
done

DEV_EXPLORE_ARGS=()
EXTRA_ARGS=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        --)
            shift
            EXTRA_ARGS+=("$@")
            break
            ;;
        --example)
            DEV_EXPLORE_ARGS+=("$1" "$2")
            shift 2
            ;;
        *)
            DEV_EXPLORE_ARGS+=("$1")
            shift
            ;;
    esac
done

"$SCRIPT_DIR/dev-explore.sh" "${DEV_EXPLORE_ARGS[@]+"${DEV_EXPLORE_ARGS[@]}"}" -- \
    -DLUA_USE_MACOSX -DSOL_USING_CXX_LUA=1 \
    -I"$LUA_DIR" -I"$SOL2_DIR/include" \
    -x c++ "${LUA_SRCS[@]}" -x none \
    ${EXTRA_ARGS[@]+"${EXTRA_ARGS[@]}"}
