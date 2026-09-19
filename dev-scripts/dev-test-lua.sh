#!/bin/bash
# Build + run the Lua graph tests (GraphLuaTests: Lua loader and crossover presets).
# They need WITH_LUA_GRAPH=ON, which build-tests/ leaves off, so this uses its own
# gitignored build-lua/ directory. Logs go to build-tests/scratch/, no cd.
# Usage: dev-test-lua.sh [gtest-filter]   (e.g. "LuaGraphLoaderTest.*"; default: all)
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/_env.sh"

LUA_BUILD_DIR="$ROOT_DIR/build-lua"
FILTER="${1:-*}"
JOBS="$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)"

CONFIGURE_LOG="$SCRATCH_DIR/lua-configure.log"
if ! cmake -S "$ROOT_DIR" -B "$LUA_BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DPACKAGE_TESTS=ON \
    -DBUILD_FULL_PROJECT=OFF \
    -DWITH_LUA_GRAPH=ON \
    > "$CONFIGURE_LOG" 2>&1; then
    echo "CONFIGURE FAILED. Last 30 lines:"
    tail -n 30 "$CONFIGURE_LOG"
    echo "Full log: $CONFIGURE_LOG"
    exit 1
fi

BUILD_LOG="$SCRATCH_DIR/lua-build.log"
if ! cmake --build "$LUA_BUILD_DIR" --target GraphLuaTests -- -j"$JOBS" > "$BUILD_LOG" 2>&1; then
    echo "BUILD FAILED. Last 60 lines:"
    tail -n 60 "$BUILD_LOG"
    echo "Full log: $BUILD_LOG"
    exit 1
fi

echo "--- warnings/errors (excluding 3rdparty) ---"
grep -B1 -E "warning:|error:" "$BUILD_LOG" \
    | grep -oE "^${ROOT_DIR}/[^:]+:[0-9]+:[0-9]+: (warning|error): .*" \
    | grep -v "/3rdparty/" | sort -u || echo "(none)"
echo "Full log: $BUILD_LOG"

RUN_LOG="$SCRATCH_DIR/lua-test.log"
STATUS=0
"$LUA_BUILD_DIR/test/GraphLuaTests" --gtest_brief=1 --gtest_filter="$FILTER" > "$RUN_LOG" 2>&1 || STATUS=$?
cat "$RUN_LOG"

# gtest exits 0 when a filter matches nothing; that must not read as a pass.
if grep -q "^\[==========\] 0 tests" "$RUN_LOG"; then
    echo "error: filter '$FILTER' matched no tests" >&2
    exit 1
fi
exit "$STATUS"
