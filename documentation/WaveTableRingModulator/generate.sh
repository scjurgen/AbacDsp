#!/usr/bin/env bash
# Builds WaveTableRingModulatorExplore, runs it, and renders all six plots via PyConPlot.py.
# Self-contained: sets up its own build dir and Python venv, both gitignored. Run from
# anywhere; every path below is resolved relative to this script's own location.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -DEXPLORE_STUFF=ON -DCMAKE_BUILD_TYPE=Release -DPACKAGE_TESTS=OFF
cmake --build "$BUILD_DIR" --target WaveTableRingModulatorExplore

DATA_DIR="$SCRIPT_DIR/generated"
mkdir -p "$DATA_DIR"

"$BUILD_DIR/documentation/WaveTableRingModulator/WaveTableRingModulatorExplore" \
    "$DATA_DIR/wr_level.txt" "$DATA_DIR/wr_sidebands.txt" "$DATA_DIR/wr_harmonics.txt" \
    "$DATA_DIR/wr_morph_harmonics.txt" "$DATA_DIR/wr_aliasing.txt" "$DATA_DIR/wr_prefilter.txt"

if [ ! -d "$SCRIPT_DIR/.venv" ]; then
    python3 -m venv "$SCRIPT_DIR/.venv"
fi
source "$SCRIPT_DIR/.venv/bin/activate"
pip install --quiet --upgrade pip
pip install --quiet -r "$SCRIPT_DIR/requirements.txt"

PLOT="$ROOT_DIR/documentation/Plot/PyConPlot.py"

python3 "$PLOT" -f "$DATA_DIR/wr_level.txt" -o "$SCRIPT_DIR/wr_level.png" \
    --labelx "morph" --labely "RMS (dB)" --width 1200 --height 900 --cols 1

python3 "$PLOT" -f "$DATA_DIR/wr_sidebands.txt" -o "$SCRIPT_DIR/wr_sidebands.png" \
    --labelx "frequency (Hz)" --labely "magnitude (dB)" --width 1200 --height 400 --cols 1

python3 "$PLOT" -f "$DATA_DIR/wr_harmonics.txt" -o "$SCRIPT_DIR/wr_harmonics.png" \
    --labelx "harmonic" --labely "level (dB)" --width 1200 --height 400 --cols 1

python3 "$PLOT" -f "$DATA_DIR/wr_morph_harmonics.txt" -o "$SCRIPT_DIR/wr_morph_harmonics.png" \
    --labelx "morph" --labely "level (dB)" --width 1200 --height 400 --cols 1

python3 "$PLOT" -f "$DATA_DIR/wr_aliasing.txt" -o "$SCRIPT_DIR/wr_aliasing.png" \
    --labelx "carrier frequency (Hz)" --labely "aliased component (dB)" --width 1200 --height 400 --cols 1

python3 "$PLOT" -f "$DATA_DIR/wr_prefilter.txt" -o "$SCRIPT_DIR/wr_prefilter.png" \
    --labelx "frequency (Hz)" --labely "magnitude (dB)" --width 1200 --height 400 --cols 1

echo "Done: $SCRIPT_DIR/wr_level.png, wr_sidebands.png, wr_harmonics.png, wr_morph_harmonics.png, wr_aliasing.png, wr_prefilter.png"
