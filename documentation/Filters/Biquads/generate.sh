#!/usr/bin/env bash
# Builds BiquadsExplore, runs it, and renders all five plots via PyConPlot.py.
# Self-contained: sets up its own build dir and Python venv, both gitignored. Run from
# anywhere; every path below is resolved relative to this script's own location.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../../.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -DEXPLORE_STUFF=ON -DCMAKE_BUILD_TYPE=Release -DPACKAGE_TESTS=OFF
cmake --build "$BUILD_DIR" --target BiquadsExplore

DATA_DIR="$SCRIPT_DIR/generated"
mkdir -p "$DATA_DIR"

"$BUILD_DIR/documentation/Filters/Biquads/BiquadsExplore" \
    "$DATA_DIR/bq_response.txt" "$DATA_DIR/bq_phase.txt" "$DATA_DIR/bq_chebyshev.txt" \
    "$DATA_DIR/bq_stability.txt" "$DATA_DIR/bq_peaksymmetry.txt"

if [ ! -d "$SCRIPT_DIR/.venv" ]; then
    python3 -m venv "$SCRIPT_DIR/.venv"
fi
source "$SCRIPT_DIR/.venv/bin/activate"
pip install --quiet --upgrade pip
pip install --quiet -r "$SCRIPT_DIR/requirements.txt"

python3 "$ROOT_DIR/documentation/Plot/PyConPlot.py" -f "$DATA_DIR/bq_response.txt" -o "$SCRIPT_DIR/bq_response.png" \
    --labelx "frequency (Hz)" --labely "magnitude (dB)" --width 1200 --height 500 --cols 1

python3 "$ROOT_DIR/documentation/Plot/PyConPlot.py" -f "$DATA_DIR/bq_phase.txt" -o "$SCRIPT_DIR/bq_phase.png" \
    --labelx "frequency (Hz)" --labely "phase (degrees)" --width 1200 --height 500 --cols 1

python3 "$ROOT_DIR/documentation/Plot/PyConPlot.py" -f "$DATA_DIR/bq_chebyshev.txt" -o "$SCRIPT_DIR/bq_chebyshev.png" \
    --labelx "frequency (Hz)" --labely "magnitude (dB)" --width 1200 --height 350 --cols 1 --miny -60 --maxy 6

python3 "$ROOT_DIR/documentation/Plot/PyConPlot.py" -f "$DATA_DIR/bq_stability.txt" -o "$SCRIPT_DIR/bq_stability.png" \
    --labelx "frequency (Hz) / Q" --labely "tail decay (dB)" --width 1200 --height 350 --cols 1

python3 "$ROOT_DIR/documentation/Plot/PyConPlot.py" -f "$DATA_DIR/bq_peaksymmetry.txt" \
    -o "$SCRIPT_DIR/bq_peaksymmetry.png" \
    --labelx "frequency (Hz)" --labely "magnitude (dB)" --width 1200 --height 400 --cols 1

echo "Done: $SCRIPT_DIR/bq_response.png, bq_phase.png, bq_chebyshev.png, bq_stability.png, bq_peaksymmetry.png"
