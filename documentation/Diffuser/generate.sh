#!/usr/bin/env bash
# Builds DiffuserExplore, runs it, and renders both plots via PyConPlot.py.
# Self-contained: sets up its own build dir and Python venv, both gitignored. Run from
# anywhere; every path below is resolved relative to this script's own location.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -DEXPLORE_STUFF=ON -DCMAKE_BUILD_TYPE=Release -DPACKAGE_TESTS=OFF
cmake --build "$BUILD_DIR" --target DiffuserExplore

DATA_DIR="$SCRIPT_DIR/generated"
mkdir -p "$DATA_DIR"

"$BUILD_DIR/documentation/Diffuser/DiffuserExplore" \
    "$DATA_DIR/df_spectral.txt" "$DATA_DIR/df_density.txt" "$DATA_DIR/df_buildup.txt" "$DATA_DIR/df_waveform.txt"

if [ ! -d "$SCRIPT_DIR/.venv" ]; then
    python3 -m venv "$SCRIPT_DIR/.venv"
fi
source "$SCRIPT_DIR/.venv/bin/activate"
pip install --quiet --upgrade pip
pip install --quiet -r "$SCRIPT_DIR/requirements.txt"

python3 "$ROOT_DIR/documentation/Plot/PyConPlot.py" -f "$DATA_DIR/df_spectral.txt" -o "$SCRIPT_DIR/df_spectral.png" \
    --labelx "frequency (Hz)" --labely "magnitude (dB)" --width 1200 --height 500 --cols 1

python3 "$ROOT_DIR/documentation/Plot/PyConPlot.py" -f "$DATA_DIR/df_density.txt" -o "$SCRIPT_DIR/df_density.png" \
    --labelx "time (s)" --labely "normalized echo density" --width 1200 --height 400 --cols 1

python3 "$ROOT_DIR/documentation/Plot/PyConPlot.py" -f "$DATA_DIR/df_buildup.txt" -o "$SCRIPT_DIR/df_buildup.png" \
    --labelx "time (s)" --labely "level (dB)" --width 1200 --height 350 --cols 1

python3 "$ROOT_DIR/documentation/Plot/PyConPlot.py" -f "$DATA_DIR/df_waveform.txt" -o "$SCRIPT_DIR/df_waveform.png" \
    --labelx "time (s)" --labely "amplitude" --width 1200 --height 220 --cols 1

echo "Done: $SCRIPT_DIR/df_spectral.png, df_density.png, df_buildup.png, df_waveform.png"
