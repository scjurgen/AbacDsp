#!/usr/bin/env bash
# Builds LinkwitzRileySplitExplore, runs it, and renders the three plots via PyConPlot.py.
# Self-contained: sets up its own build dir and Python venv, both gitignored. Run from
# anywhere; every path below is resolved relative to this script's own location.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../../.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -DEXPLORE_STUFF=ON -DCMAKE_BUILD_TYPE=Release -DPACKAGE_TESTS=OFF
cmake --build "$BUILD_DIR" --target LinkwitzRileySplitExplore

DATA_DIR="$SCRIPT_DIR/generated"
mkdir -p "$DATA_DIR"

"$BUILD_DIR/documentation/Filters/LinkwitzRileySplit/LinkwitzRileySplitExplore" \
    "$DATA_DIR/lrs_bands.txt" "$DATA_DIR/lrs_theory.txt" "$DATA_DIR/lrs_sum.txt"

if [ ! -d "$SCRIPT_DIR/.venv" ]; then
    python3 -m venv "$SCRIPT_DIR/.venv"
fi
source "$SCRIPT_DIR/.venv/bin/activate"
pip install --quiet --upgrade pip
pip install --quiet -r "$SCRIPT_DIR/requirements.txt"

python3 "$ROOT_DIR/documentation/Plot/PyConPlot.py" -f "$DATA_DIR/lrs_bands.txt" -o "$SCRIPT_DIR/lrs_bands.png" \
    --labelx "frequency (Hz)" --labely "magnitude (dB)" --width 1200 --height 500 --cols 1

python3 "$ROOT_DIR/documentation/Plot/PyConPlot.py" -f "$DATA_DIR/lrs_theory.txt" -o "$SCRIPT_DIR/lrs_theory.png" \
    --labelx "frequency (Hz)" --labely "deviation (dB)" --width 1200 --height 700 --cols 1

python3 "$ROOT_DIR/documentation/Plot/PyConPlot.py" -f "$DATA_DIR/lrs_sum.txt" -o "$SCRIPT_DIR/lrs_sum.png" \
    --labelx "frequency (Hz)" --labely "magnitude (dB) / phase (degrees) / deviation (dB)" --width 1200 --height 1000 --cols 1

echo "Done: $SCRIPT_DIR/lrs_bands.png, lrs_theory.png, lrs_sum.png"
