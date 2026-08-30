#!/usr/bin/env bash
# Builds ModulationExplore, runs it, and renders all four plots via PyConPlot.py.
# Self-contained: sets up its own build dir and Python venv, both gitignored. Run from
# anywhere; every path below is resolved relative to this script's own location.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -DEXPLORE_STUFF=ON -DCMAKE_BUILD_TYPE=Release -DPACKAGE_TESTS=OFF
cmake --build "$BUILD_DIR" --target ModulationExplore

DATA_DIR="$SCRIPT_DIR/generated"
mkdir -p "$DATA_DIR"

"$BUILD_DIR/documentation/Modulation/ModulationExplore" \
    "$DATA_DIR/md_timelines.txt" "$DATA_DIR/md_spectra.txt" "$DATA_DIR/md_orbit.txt" \
    "$DATA_DIR/md_tremolo_ringmod.txt"

if [ ! -d "$SCRIPT_DIR/.venv" ]; then
    python3 -m venv "$SCRIPT_DIR/.venv"
fi
source "$SCRIPT_DIR/.venv/bin/activate"
pip install --quiet --upgrade pip
pip install --quiet -r "$SCRIPT_DIR/requirements.txt"

python3 "$ROOT_DIR/documentation/Plot/PyConPlot.py" -f "$DATA_DIR/md_timelines.txt" -o "$SCRIPT_DIR/md_timelines.png" \
    --labelx "time (s)" --labely "value" --width 1200 --height 350 --cols 1

python3 "$ROOT_DIR/documentation/Plot/PyConPlot.py" -f "$DATA_DIR/md_spectra.txt" -o "$SCRIPT_DIR/md_spectra.png" \
    --labelx "frequency (Hz)" --labely "magnitude (dB)" --width 1200 --height 350 --cols 1

python3 "$ROOT_DIR/documentation/Plot/PyConPlot.py" -f "$DATA_DIR/md_orbit.txt" -o "$SCRIPT_DIR/md_orbit.png" \
    --labelx "x[n]" --labely "x[n+delta]" --width 700 --height 700 --cols 2

python3 "$ROOT_DIR/documentation/Plot/PyConPlot.py" -f "$DATA_DIR/md_tremolo_ringmod.txt" \
    -o "$SCRIPT_DIR/md_tremolo_ringmod.png" \
    --labelx "time (s) / frequency (Hz)" --labely "gain / magnitude (dB)" --width 1200 --height 400 --cols 1

echo "Done: $SCRIPT_DIR/md_timelines.png, md_spectra.png, md_orbit.png, md_tremolo_ringmod.png"
