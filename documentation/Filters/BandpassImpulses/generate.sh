#!/usr/bin/env bash
# Builds BandpassImpulseExplore, runs it, and renders all six plots via PyConPlot.py.
# Self-contained: sets up its own build dir and Python venv, both gitignored. Run from
# anywhere; every path below is resolved relative to this script's own location.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../../.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -DEXPLORE_STUFF=ON -DCMAKE_BUILD_TYPE=Release -DPACKAGE_TESTS=OFF
cmake --build "$BUILD_DIR" --target BandpassImpulseExplore

"$BUILD_DIR/documentation/Filters/BandpassImpulses/BandpassImpulseExplore" \
    "$SCRIPT_DIR/rb_response.txt" "$SCRIPT_DIR/rb_decay_accuracy.txt" "$SCRIPT_DIR/rb_compensation_accuracy.txt" \
    "$SCRIPT_DIR/rb_pitchbend.txt" "$SCRIPT_DIR/rb_topology.txt" "$SCRIPT_DIR/rb_isactive.txt"

if [ ! -d "$SCRIPT_DIR/.venv" ]; then
    python3 -m venv "$SCRIPT_DIR/.venv"
fi
source "$SCRIPT_DIR/.venv/bin/activate"
pip install --quiet --upgrade pip
pip install --quiet -r "$SCRIPT_DIR/requirements.txt"

python3 "$ROOT_DIR/documentation/Plot/PyConPlot.py" -f "$SCRIPT_DIR/rb_response.txt" -o "$SCRIPT_DIR/rb_response.png" \
    --labelx "frequency (Hz)" --labely "magnitude (dB, normalized to peak)" --width 900 --height 450 --cols 2

python3 "$ROOT_DIR/documentation/Plot/PyConPlot.py" -f "$SCRIPT_DIR/rb_decay_accuracy.txt" \
    -o "$SCRIPT_DIR/rb_decay_accuracy.png" \
    --labelx "requested 60 dB decay time (s)" --labely "error (%)" --width 900 --height 450 --cols 2

python3 "$ROOT_DIR/documentation/Plot/PyConPlot.py" -f "$SCRIPT_DIR/rb_compensation_accuracy.txt" \
    -o "$SCRIPT_DIR/rb_compensation_accuracy.png" \
    --labelx "decay time (s)" --labely "peak amplitude (dB, 0 dB = exact)" --width 1200 --height 450 --cols 1

python3 "$ROOT_DIR/documentation/Plot/PyConPlot.py" -f "$SCRIPT_DIR/rb_pitchbend.txt" -o "$SCRIPT_DIR/rb_pitchbend.png" \
    --labelx "pitch bend (cents)" --labely "error (cents)" --width 900 --height 450 --cols 2

python3 "$ROOT_DIR/documentation/Plot/PyConPlot.py" -f "$SCRIPT_DIR/rb_topology.txt" -o "$SCRIPT_DIR/rb_topology.png" \
    --labelx "frequency (Hz)" --labely "magnitude (dB, normalized to peak)" --width 900 --height 450 --cols 2

python3 "$ROOT_DIR/documentation/Plot/PyConPlot.py" -f "$SCRIPT_DIR/rb_isactive.txt" -o "$SCRIPT_DIR/rb_isactive.png" \
    --labelx "sample index" --labely "dB (envelope; isActive() at 0/-80)" --width 900 --height 450 --cols 2

echo "Done: $SCRIPT_DIR/rb_response.png, rb_decay_accuracy.png, rb_compensation_accuracy.png, rb_pitchbend.png, rb_topology.png, rb_isactive.png"
