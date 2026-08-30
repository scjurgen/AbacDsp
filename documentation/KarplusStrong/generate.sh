#!/usr/bin/env bash
# Builds KarplusStrongExplore, runs it, and renders all four plots via PyConPlot.py.
# Self-contained: sets up its own build dir and Python venv, both gitignored. Run from
# anywhere; every path below is resolved relative to this script's own location.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -DEXPLORE_STUFF=ON -DCMAKE_BUILD_TYPE=Release -DPACKAGE_TESTS=OFF
cmake --build "$BUILD_DIR" --target KarplusStrongExplore

DATA_DIR="$SCRIPT_DIR/generated"
mkdir -p "$DATA_DIR"

"$BUILD_DIR/documentation/KarplusStrong/KarplusStrongExplore" \
    "$DATA_DIR/ks_spectral.txt" "$DATA_DIR/ks_decay.txt" "$DATA_DIR/ks_bend.txt" "$DATA_DIR/ks_excitation.txt"

if [ ! -d "$SCRIPT_DIR/.venv" ]; then
    python3 -m venv "$SCRIPT_DIR/.venv"
fi
source "$SCRIPT_DIR/.venv/bin/activate"
pip install --quiet --upgrade pip
pip install --quiet -r "$SCRIPT_DIR/requirements.txt"

python3 "$ROOT_DIR/documentation/Plot/PyConPlot.py" -f "$DATA_DIR/ks_spectral.txt" -o "$SCRIPT_DIR/ks_spectral.png" \
    --labelx "frequency (Hz)" --labely "magnitude (dB)" --width 1200 --height 400 --cols 1

python3 "$ROOT_DIR/documentation/Plot/PyConPlot.py" -f "$DATA_DIR/ks_decay.txt" -o "$SCRIPT_DIR/ks_decay.png" \
    --labelx "time (s)" --labely "level (dB)" --width 1200 --height 350 --cols 1

python3 "$ROOT_DIR/documentation/Plot/PyConPlot.py" -f "$DATA_DIR/ks_bend.txt" -o "$SCRIPT_DIR/ks_bend.png" \
    --labelx "time (s)" --labely "tracked pitch (Hz)" --width 1200 --height 350 --cols 1

python3 "$ROOT_DIR/documentation/Plot/PyConPlot.py" -f "$DATA_DIR/ks_excitation.txt" -o "$SCRIPT_DIR/ks_excitation.png" \
    --labelx "time (s)" --labely "20*ln(peak amplitude)" --width 1200 --height 300 --cols 1 --miny -80 --maxy 0

echo "Done: $SCRIPT_DIR/ks_spectral.png, ks_decay.png, ks_bend.png, ks_excitation.png"
