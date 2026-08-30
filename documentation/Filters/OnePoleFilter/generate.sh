#!/usr/bin/env bash
# Builds OnePoleExplore, runs it, and renders all three plots via PyConPlot.py.
# Self-contained: sets up its own build dir and Python venv, both gitignored. Run from
# anywhere; every path below is resolved relative to this script's own location.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../../.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -DEXPLORE_STUFF=ON -DCMAKE_BUILD_TYPE=Release -DPACKAGE_TESTS=OFF
cmake --build "$BUILD_DIR" --target OnePoleExplore

DATA_DIR="$SCRIPT_DIR/generated"
mkdir -p "$DATA_DIR"

"$BUILD_DIR/documentation/Filters/OnePoleFilter/OnePoleExplore" \
    "$DATA_DIR/op_response.txt" "$DATA_DIR/op_decaytime.txt" "$DATA_DIR/op_allpass_phase.txt"

if [ ! -d "$SCRIPT_DIR/.venv" ]; then
    python3 -m venv "$SCRIPT_DIR/.venv"
fi
source "$SCRIPT_DIR/.venv/bin/activate"
pip install --quiet --upgrade pip
pip install --quiet -r "$SCRIPT_DIR/requirements.txt"

python3 "$ROOT_DIR/documentation/Plot/PyConPlot.py" -f "$DATA_DIR/op_response.txt" -o "$SCRIPT_DIR/op_response.png" \
    --labelx "frequency (Hz)" --labely "magnitude (dB)" --width 1200 --height 500 --cols 1

python3 "$ROOT_DIR/documentation/Plot/PyConPlot.py" -f "$DATA_DIR/op_decaytime.txt" -o "$SCRIPT_DIR/op_decaytime.png" \
    --labelx "time (s)" --labely "level (dB)" --width 1200 --height 500 --cols 1

python3 "$ROOT_DIR/documentation/Plot/PyConPlot.py" -f "$DATA_DIR/op_allpass_phase.txt" \
    -o "$SCRIPT_DIR/op_allpass_phase.png" \
    --labelx "frequency (Hz)" --labely "magnitude (dB) / phase (degrees)" --width 1200 --height 350 --cols 1

echo "Done: $SCRIPT_DIR/op_response.png, op_decaytime.png, op_allpass_phase.png"
