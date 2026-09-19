#!/usr/bin/env bash
# Builds OverSamplingExplore, runs it, and renders every plot via PyConPlot.py and
# spectrogram_plot.py. Self-contained: sets up its own build dir and Python venv, both
# gitignored. Run from anywhere; every path below is resolved relative to this script's own
# location.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -DEXPLORE_STUFF=ON -DCMAKE_BUILD_TYPE=Release -DPACKAGE_TESTS=OFF
cmake --build "$BUILD_DIR" --target OverSamplingExplore

DATA_DIR="$SCRIPT_DIR/generated"
mkdir -p "$DATA_DIR"

"$BUILD_DIR/documentation/OverSampling/OverSamplingExplore" "$DATA_DIR"

if [ ! -d "$SCRIPT_DIR/.venv" ]; then
    python3 -m venv "$SCRIPT_DIR/.venv"
fi
source "$SCRIPT_DIR/.venv/bin/activate"
pip install --quiet --upgrade pip
pip install --quiet -r "$SCRIPT_DIR/requirements.txt"

PLOT="$ROOT_DIR/documentation/Plot/PyConPlot.py"
SPEC="$SCRIPT_DIR/spectrogram_plot.py"

python3 "$SPEC" \
    --panel "$DATA_DIR/os_sweep_0.99.txt" "ratio 0.99" \
    --panel "$DATA_DIR/os_sweep_1.0.txt" "ratio 1.0" \
    --panel "$DATA_DIR/os_sweep_0.5.txt" "ratio 0.5 (undersampling, internal 24 kHz)" \
    --panel "$DATA_DIR/os_sweep_2.0.txt" "ratio 2.0 (oversampling)" \
    --panel "$DATA_DIR/os_sweep_4.0.txt" "ratio 4.0 (oversampling)" \
    -o "$SCRIPT_DIR/os_sweep.png" --maxfreq 24000 --height 2.0

python3 "$PLOT" -f "$DATA_DIR/os_delaytime.txt" -o "$SCRIPT_DIR/os_delaytime.png" \
    --labelx "ratio" --labely "arrival (host samples)" --width 1200 --height 400 --cols 1

python3 "$SPEC" \
    --panel "$DATA_DIR/os_jump_abrupt.txt" "abrupt ratio 0.5 -> 2.0 at 2 s" \
    --panel "$DATA_DIR/os_jump_slow.txt" "ratio 0.75 to 1.25, sine at 0.5 Hz" \
    -o "$SCRIPT_DIR/os_ratiochange.png" --maxfreq 24000 --height 2.0

echo "Done: PNGs in $SCRIPT_DIR, WAVs (for listening) and .txt data in $DATA_DIR"
