#!/usr/bin/env bash
# Builds MelColumnsExplore, runs it, and renders the three plots via PyConPlot.py.
# Self-contained: sets up its own build dir and Python venv, both gitignored. Run from
# anywhere; every path below is resolved relative to this script's own location.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -DEXPLORE_STUFF=ON -DCMAKE_BUILD_TYPE=Release -DPACKAGE_TESTS=OFF
cmake --build "$BUILD_DIR" --target MelColumnsExplore

DATA_DIR="$SCRIPT_DIR/generated"
mkdir -p "$DATA_DIR"

"$BUILD_DIR/documentation/MelColumns/MelColumnsExplore" \
    "$DATA_DIR/mc_tone.txt" "$DATA_DIR/mc_linearity.txt" "$DATA_DIR/mc_noise.txt"

if [ ! -d "$SCRIPT_DIR/.venv" ]; then
    python3 -m venv "$SCRIPT_DIR/.venv"
fi
source "$SCRIPT_DIR/.venv/bin/activate"
pip install --quiet --upgrade pip
pip install --quiet -r "$SCRIPT_DIR/requirements.txt"

PLOT="$ROOT_DIR/documentation/Plot/PyConPlot.py"

python3 "$PLOT" -f "$DATA_DIR/mc_tone.txt" -o "$SCRIPT_DIR/mc_tone.png" \
    --labelx "frequency (Hz)" --labely "dB (top), % (bottom)" --width 1200 --height 700 --cols 1

python3 "$PLOT" -f "$DATA_DIR/mc_linearity.txt" -o "$SCRIPT_DIR/mc_linearity.png" \
    --labelx "input level (dB)" --labely "band level (dB)" --width 1200 --height 400 --cols 1

python3 "$PLOT" -f "$DATA_DIR/mc_noise.txt" -o "$SCRIPT_DIR/mc_noise.png" \
    --labelx "band centre (Hz)" --labely "level (dB)" --width 1200 --height 400 --cols 1

echo "Done: $SCRIPT_DIR/mc_tone.png, mc_linearity.png, mc_noise.png"
