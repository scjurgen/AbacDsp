#!/usr/bin/env bash
# Runs OnePoleSmootherExplore through dev-scripts/dev-explore.sh and renders both plots via PyConPlot.py.
# Self-contained: sets up its own Python venv (gitignored). Run from anywhere; every path below is
# resolved relative to this script's own location.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
DATA_DIR="$SCRIPT_DIR/generated"
mkdir -p "$DATA_DIR"

EXPLORE_OUT_DIR="$DATA_DIR" "$ROOT_DIR/dev-scripts/dev-explore.sh" "$SCRIPT_DIR/OnePoleSmootherExplore.cpp"

if [ ! -d "$SCRIPT_DIR/.venv" ]; then
    python3 -m venv "$SCRIPT_DIR/.venv"
fi
source "$SCRIPT_DIR/.venv/bin/activate"
pip install --quiet --upgrade pip
pip install --quiet -r "$SCRIPT_DIR/requirements.txt"

python3 "$ROOT_DIR/documentation/Plot/PyConPlot.py" -f "$DATA_DIR/smoother_step.txt" -o "$SCRIPT_DIR/smoother_step.png" \
    --labelx "time (time constants)" --labely "output" --width 1200 --height 500 --cols 1

python3 "$ROOT_DIR/documentation/Plot/PyConPlot.py" -f "$DATA_DIR/smoother_stall.txt" -o "$SCRIPT_DIR/smoother_stall.png" \
    --labelx "time (s)" --labely "1 - output" --width 1200 --height 500 --cols 1

echo "Done: $SCRIPT_DIR/smoother_step.png, smoother_stall.png"
