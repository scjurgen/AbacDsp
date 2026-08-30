#!/usr/bin/env bash
# Builds PinkFilterExplore, runs it, and renders both plots via PyConPlot.py.
# Self-contained: sets up its own build dir and Python venv, both gitignored. Run from
# anywhere; every path below is resolved relative to this script's own location.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../../.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -DEXPLORE_STUFF=ON -DCMAKE_BUILD_TYPE=Release -DPACKAGE_TESTS=OFF
cmake --build "$BUILD_DIR" --target PinkFilterExplore

DATA_DIR="$SCRIPT_DIR/generated"
mkdir -p "$DATA_DIR"

"$BUILD_DIR/documentation/Filters/PinkFilter/PinkFilterExplore" \
    "$DATA_DIR/pf_spectrum.txt" "$DATA_DIR/pf_accuracy.txt"

if [ ! -d "$SCRIPT_DIR/.venv" ]; then
    python3 -m venv "$SCRIPT_DIR/.venv"
fi
source "$SCRIPT_DIR/.venv/bin/activate"
pip install --quiet --upgrade pip
pip install --quiet -r "$SCRIPT_DIR/requirements.txt"

python3 "$ROOT_DIR/documentation/Plot/PyConPlot.py" -f "$DATA_DIR/pf_spectrum.txt" -o "$SCRIPT_DIR/pf_spectrum.png" \
    --labelx "frequency (Hz)" --labely "power (dB)" --width 1200 --height 500 --cols 1

python3 "$ROOT_DIR/documentation/Plot/PyConPlot.py" -f "$DATA_DIR/pf_accuracy.txt" -o "$SCRIPT_DIR/pf_accuracy.png" \
    --labelx "frequency (Hz)" --labely "error vs. ideal (dB)" --width 1200 --height 350 --cols 1

echo "Done: $SCRIPT_DIR/pf_spectrum.png, pf_accuracy.png"
