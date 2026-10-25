#!/usr/bin/env bash
# Builds NoteSegmenterExplore, runs it, and renders all four plots via PyConPlot.py.
# Self-contained: sets up its own build dir and Python venv, both gitignored. Run from
# anywhere; every path below is resolved relative to this script's own location.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -DEXPLORE_STUFF=ON -DCMAKE_BUILD_TYPE=Release -DPACKAGE_TESTS=OFF
cmake --build "$BUILD_DIR" --target NoteSegmenterExplore

DATA_DIR="$SCRIPT_DIR/generated"
mkdir -p "$DATA_DIR"

"$BUILD_DIR/documentation/NoteSegmenter/NoteSegmenterExplore" \
    "$DATA_DIR/ns_phrase.txt" "$DATA_DIR/ns_errors.txt" "$DATA_DIR/ns_noise.txt" "$DATA_DIR/ns_ringing.txt"

if [ ! -d "$SCRIPT_DIR/.venv" ]; then
    python3 -m venv "$SCRIPT_DIR/.venv"
fi
source "$SCRIPT_DIR/.venv/bin/activate"
pip install --quiet --upgrade pip
pip install --quiet -r "$SCRIPT_DIR/requirements.txt"

PLOT="$ROOT_DIR/documentation/Plot/PyConPlot.py"

python3 "$PLOT" -f "$DATA_DIR/ns_phrase.txt" -o "$SCRIPT_DIR/ns_phrase.png" --width 1200 --height 400 --cols 1
python3 "$PLOT" -f "$DATA_DIR/ns_errors.txt" -o "$SCRIPT_DIR/ns_errors.png" --width 1200 --height 350 --cols 1
python3 "$PLOT" -f "$DATA_DIR/ns_noise.txt" -o "$SCRIPT_DIR/ns_noise.png" --width 1200 --height 400 --cols 1
python3 "$PLOT" -f "$DATA_DIR/ns_ringing.txt" -o "$SCRIPT_DIR/ns_ringing.png" --width 1200 --height 450 --cols 1

echo "Done: $SCRIPT_DIR/ns_phrase.png, ns_errors.png, ns_noise.png, ns_ringing.png"
