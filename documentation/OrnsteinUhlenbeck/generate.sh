#!/usr/bin/env bash
# Builds OrnsteinUhlenbeckTimeline, runs it, and renders all three plots via PyConPlot.py.
# Self-contained: sets up its own build dir and Python venv, both gitignored. Run from
# anywhere; every path below is resolved relative to this script's own location.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -DEXPLORE_STUFF=ON -DCMAKE_BUILD_TYPE=Release -DPACKAGE_TESTS=OFF
cmake --build "$BUILD_DIR" --target OrnsteinUhlenbeckTimeline

"$BUILD_DIR/documentation/OrnsteinUhlenbeck/OrnsteinUhlenbeckTimeline" \
    "$SCRIPT_DIR/ou_timelines.txt" "$SCRIPT_DIR/ou_autocorr.txt" "$SCRIPT_DIR/ou_distribution.txt"

if [ ! -d "$SCRIPT_DIR/.venv" ]; then
    python3 -m venv "$SCRIPT_DIR/.venv"
fi
source "$SCRIPT_DIR/.venv/bin/activate"
pip install --quiet --upgrade pip
pip install --quiet -r "$SCRIPT_DIR/requirements.txt"

python3 "$ROOT_DIR/documentation/Plot/PyConPlot.py" -f "$SCRIPT_DIR/ou_timelines.txt" -o "$SCRIPT_DIR/ou_timeline.png" \
    --labelx "time (s)" --labely "x" --width 2400 --height 200 --cols 1 --miny -0.2 --maxy 0.2

python3 "$ROOT_DIR/documentation/Plot/PyConPlot.py" -f "$SCRIPT_DIR/ou_autocorr.txt" -o "$SCRIPT_DIR/ou_autocorr.png" \
    --labelx "lag tau (s)" --labely "ACF" --width 1200 --height 250 --cols 1 --miny -0.5 --maxy 1.05

python3 "$ROOT_DIR/documentation/Plot/PyConPlot.py" -f "$SCRIPT_DIR/ou_distribution.txt" -o "$SCRIPT_DIR/ou_distribution.png" \
    --labelx "deviation from mean" --labely "density" --width 1200 --height 300 --cols 1

echo "Done: $SCRIPT_DIR/ou_timeline.png, ou_autocorr.png, ou_distribution.png"
