#!/usr/bin/env bash
# Runs ExactOrnsteinUhlenbeckExplore through dev-scripts/dev-explore.sh and renders its three plots via
# PyConPlot.py. Self-contained: sets up its own Python venv (gitignored). Run from anywhere; every path
# below is resolved relative to this script's own location.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
DATA_DIR="$SCRIPT_DIR/generated"
mkdir -p "$DATA_DIR"

EXPLORE_OUT_DIR="$DATA_DIR" "$ROOT_DIR/dev-scripts/dev-explore.sh" "$SCRIPT_DIR/ExactOrnsteinUhlenbeckExplore.cpp"

if [ ! -d "$SCRIPT_DIR/.venv" ]; then
    python3 -m venv "$SCRIPT_DIR/.venv"
fi
source "$SCRIPT_DIR/.venv/bin/activate"
pip install --quiet --upgrade pip
pip install --quiet -r "$SCRIPT_DIR/requirements.txt"

python3 "$ROOT_DIR/documentation/Plot/PyConPlot.py" -f "$DATA_DIR/ou_exact_autocorr.txt" -o "$SCRIPT_DIR/ou_exact_autocorr.png" \
    --labelx "lag tau (s)" --labely "ACF" --width 1200 --height 250 --cols 1 --miny -0.5 --maxy 1.05

python3 "$ROOT_DIR/documentation/Plot/PyConPlot.py" -f "$DATA_DIR/ou_exact_distribution.txt" -o "$SCRIPT_DIR/ou_exact_distribution.png" \
    --labelx "deviation from mean" --labely "density" --width 1200 --height 300 --cols 1

python3 "$ROOT_DIR/documentation/Plot/PyConPlot.py" -f "$DATA_DIR/ou_exact_stability.txt" -o "$SCRIPT_DIR/ou_exact_stability.png" \
    --labelx "step rate (Hz)" --labely "deviation / theory" --width 1200 --height 500 --cols 1 --logx

echo "Done: $SCRIPT_DIR/ou_exact_autocorr.png, ou_exact_distribution.png, ou_exact_stability.png"
