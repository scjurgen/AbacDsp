#!/usr/bin/env bash
# Runs EnvelopeCurveExplore through dev-scripts/dev-explore.sh and renders both plots via PyConPlot.py.
# Self-contained: sets up its own Python venv (gitignored). Run from anywhere; every path below is
# resolved relative to this script's own location.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
DATA_DIR="$SCRIPT_DIR/generated"
mkdir -p "$DATA_DIR"

EXPLORE_OUT_DIR="$DATA_DIR" "$ROOT_DIR/dev-scripts/dev-explore.sh" "$SCRIPT_DIR/EnvelopeCurveExplore.cpp"

if [ ! -d "$SCRIPT_DIR/.venv" ]; then
    python3 -m venv "$SCRIPT_DIR/.venv"
fi
source "$SCRIPT_DIR/.venv/bin/activate"
pip install --quiet --upgrade pip
pip install --quiet -r "$SCRIPT_DIR/requirements.txt"

python3 "$ROOT_DIR/documentation/Plot/PyConPlot.py" -f "$DATA_DIR/envelope_shapes.txt" -o "$SCRIPT_DIR/envelope_shapes.png" \
    --labelx "position in the segment" --labely "fraction of the distance covered" --width 1200 --height 500 --cols 1

python3 "$ROOT_DIR/documentation/Plot/PyConPlot.py" -f "$DATA_DIR/envelope_pitch.txt" -o "$SCRIPT_DIR/envelope_pitch.png" \
    --labelx "time (ms)" --labely "MIDI note" --width 1200 --height 500 --cols 1

echo "Done: $SCRIPT_DIR/envelope_shapes.png, envelope_pitch.png"
