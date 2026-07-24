#!/bin/bash
# Compile and run a throwaway C++ program against the header-only DSP library.
# Build artifacts land in explore/ (gitignored), not /tmp, so no permission prompts.
#
# Usage:
#   dev-explore.sh [source.cpp] [--example NAME]... [-- extra clang args...]
#
#   source.cpp   program to build (default: explore/explore.cpp).
#   --example N  add examples/N/src and examples/N/src/impl to the include path,
#                so "impl/<Name>Impl.h", "EffectBase.h" etc resolve. Repeatable.
#   everything after a literal -- is forwarded verbatim to clang++.
#
# The binary is written next to the source (explore/<basename>) and then run.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/_env.sh"

EXPLORE_DIR="$ROOT_DIR/explore"
mkdir -p "$EXPLORE_DIR"

SRC="$EXPLORE_DIR/explore.cpp"
EXAMPLE_INCLUDES=()
EXTRA_ARGS=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        --example)
            shift
            [[ $# -gt 0 ]] || { echo "--example needs a name" >&2; exit 2; }
            EXAMPLE_INCLUDES+=("-I$ROOT_DIR/examples/$1/src" "-I$ROOT_DIR/examples/$1/src/impl")
            shift
            ;;
        --)
            shift
            EXTRA_ARGS+=("$@")
            break
            ;;
        *)
            SRC="$1"
            shift
            ;;
    esac
done

if [[ ! -f "$SRC" ]]; then
    echo "source not found: $SRC" >&2
    exit 1
fi

BIN="$EXPLORE_DIR/$(basename "${SRC%.*}")"

# -iquote (not -I) for the explore dir: it holds built binaries whose names can
# collide with standard headers (e.g. a binary "compare" shadowing <compare>).
# -iquote only satisfies "quoted" includes, so headers next to the source resolve
# while angle-bracket standard headers stay clean.
clang++ -std=c++20 -O2 -Wall -Wextra \
    -I"$ROOT_DIR/src/includes" \
    -I"$ROOT_DIR/3rdparty/AudioFile" \
    -iquote "$EXPLORE_DIR" \
    ${EXAMPLE_INCLUDES[@]+"${EXAMPLE_INCLUDES[@]}"} \
    ${EXTRA_ARGS[@]+"${EXTRA_ARGS[@]}"} \
    "$SRC" -o "$BIN"

"$BIN"
