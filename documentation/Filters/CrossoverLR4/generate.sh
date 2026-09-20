#!/usr/bin/env bash
# Builds CrossoverLR4Explore, runs it, and renders all three plots via PyConPlot.py.
# Self-contained: sets up its own build dir and Python venv, both gitignored. Run from
# anywhere; every path below is resolved relative to this script's own location.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../../.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -DEXPLORE_STUFF=ON -DCMAKE_BUILD_TYPE=Release -DPACKAGE_TESTS=OFF
cmake --build "$BUILD_DIR" --target CrossoverLR4Explore

DATA_DIR="$SCRIPT_DIR/generated"
mkdir -p "$DATA_DIR"

"$BUILD_DIR/documentation/Filters/CrossoverLR4/CrossoverLR4Explore" \
    "$DATA_DIR/cl4_response.txt" "$DATA_DIR/cl4_sum_allpass.txt" "$DATA_DIR/cl4_error.txt"

if [ ! -d "$SCRIPT_DIR/.venv" ]; then
    python3 -m venv "$SCRIPT_DIR/.venv"
fi
source "$SCRIPT_DIR/.venv/bin/activate"
pip install --quiet --upgrade pip
pip install --quiet -r "$SCRIPT_DIR/requirements.txt"

python3 "$ROOT_DIR/documentation/Plot/PyConPlot.py" -f "$DATA_DIR/cl4_response.txt" -o "$SCRIPT_DIR/cl4_response.png" \
    --labelx "frequency (Hz)" --labely "magnitude (dB)" --width 1200 --height 500 --cols 1

python3 "$ROOT_DIR/documentation/Plot/PyConPlot.py" -f "$DATA_DIR/cl4_sum_allpass.txt" \
    -o "$SCRIPT_DIR/cl4_sum_allpass.png" \
    --labelx "frequency (Hz)" --labely "magnitude (dB) / phase (degrees)" --width 1200 --height 700 --cols 1

python3 "$ROOT_DIR/documentation/Plot/PyConPlot.py" -f "$DATA_DIR/cl4_error.txt" -o "$SCRIPT_DIR/cl4_error.png" \
    --labelx "frequency (Hz)" --labely "deviation (dB)" --width 1200 --height 400 --cols 1

echo "Done: $SCRIPT_DIR/cl4_response.png, cl4_sum_allpass.png, cl4_error.png"
