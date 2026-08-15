#!/usr/bin/env bash
# Builds PoleMixingExplore, runs it, and renders all six plots via PyConPlot.py.
# Self-contained: sets up its own build dir and Python venv, both gitignored. Run from
# anywhere; every path below is resolved relative to this script's own location.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../../.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -DEXPLORE_STUFF=ON -DCMAKE_BUILD_TYPE=Release -DPACKAGE_TESTS=OFF
cmake --build "$BUILD_DIR" --target PoleMixingExplore

"$BUILD_DIR/documentation/Filters/PoleMixing/PoleMixingExplore" \
    "$SCRIPT_DIR/pm_response.txt" "$SCRIPT_DIR/pm_resonance.txt" "$SCRIPT_DIR/pm_overdrive.txt" \
    "$SCRIPT_DIR/pm_cutoff_accuracy.txt" "$SCRIPT_DIR/pm_realtime.txt" "$SCRIPT_DIR/pm_topology.txt" \
    "$SCRIPT_DIR/pm_raw_correction_data.txt"

if [ ! -d "$SCRIPT_DIR/.venv" ]; then
    python3 -m venv "$SCRIPT_DIR/.venv"
fi
source "$SCRIPT_DIR/.venv/bin/activate"
pip install --quiet --upgrade pip
pip install --quiet -r "$SCRIPT_DIR/requirements.txt"

python3 "$ROOT_DIR/documentation/Plot/PyConPlot.py" -f "$SCRIPT_DIR/pm_response.txt" -o "$SCRIPT_DIR/pm_response.png" \
    --labelx "frequency (Hz)" --labely "magnitude (dB) / phase (deg)" --width 1200 --height 450 --cols 1

python3 "$ROOT_DIR/documentation/Plot/PyConPlot.py" -f "$SCRIPT_DIR/pm_resonance.txt" -o "$SCRIPT_DIR/pm_resonance.png" \
    --labelx "frequency (Hz) / time (s)" --labely "magnitude (dB) / amplitude" --width 1200 --height 350 --cols 1

python3 "$ROOT_DIR/documentation/Plot/PyConPlot.py" -f "$SCRIPT_DIR/pm_overdrive.txt" -o "$SCRIPT_DIR/pm_overdrive.png" \
    --labelx "input / frequency (Hz)" --labely "output / magnitude (dB)" --width 1200 --height 350 --cols 1

python3 "$ROOT_DIR/documentation/Plot/PyConPlot.py" -f "$SCRIPT_DIR/pm_cutoff_accuracy.txt" \
    -o "$SCRIPT_DIR/pm_cutoff_accuracy.png" \
    --labelx "requested cutoff (Hz)" --labely "measured resonant-peak frequency (Hz)" --width 1200 --height 500 --cols 1

python3 "$ROOT_DIR/documentation/Plot/PyConPlot.py" -f "$SCRIPT_DIR/pm_realtime.txt" -o "$SCRIPT_DIR/pm_realtime.png" \
    --labelx "time (s)" --labely "output amplitude" --width 1200 --height 350 --cols 1

python3 "$ROOT_DIR/documentation/Plot/PyConPlot.py" -f "$SCRIPT_DIR/pm_topology.txt" -o "$SCRIPT_DIR/pm_topology.png" \
    --labelx "frequency (Hz) / time (s)" --labely "magnitude (dB) / amplitude" --width 1200 --height 350 --cols 1

echo "Done: $SCRIPT_DIR/pm_response.png, pm_resonance.png, pm_overdrive.png, pm_cutoff_accuracy.png, pm_realtime.png, pm_topology.png"
