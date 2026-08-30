#!/usr/bin/env bash
# Builds DelaysExplore, runs it, and renders all three plots via PyConPlot.py.
# Self-contained: sets up its own build dir and Python venv, both gitignored. Run from
# anywhere; every path below is resolved relative to this script's own location.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -DEXPLORE_STUFF=ON -DCMAKE_BUILD_TYPE=Release -DPACKAGE_TESTS=OFF
cmake --build "$BUILD_DIR" --target DelaysExplore

DATA_DIR="$SCRIPT_DIR/generated"
mkdir -p "$DATA_DIR"

"$BUILD_DIR/documentation/Delays/DelaysExplore" \
    "$DATA_DIR/td_pitchwobble.txt" "$DATA_DIR/td_speedglide.txt" "$DATA_DIR/td_multitap.txt"

if [ ! -d "$SCRIPT_DIR/.venv" ]; then
    python3 -m venv "$SCRIPT_DIR/.venv"
fi
source "$SCRIPT_DIR/.venv/bin/activate"
pip install --quiet --upgrade pip
pip install --quiet -r "$SCRIPT_DIR/requirements.txt"

python3 "$ROOT_DIR/documentation/Plot/PyConPlot.py" -f "$DATA_DIR/td_pitchwobble.txt" \
    -o "$SCRIPT_DIR/td_pitchwobble.png" \
    --labelx "time (s)" --labely "tracked pitch (Hz)" --width 1200 --height 400 --cols 1

python3 "$ROOT_DIR/documentation/Plot/PyConPlot.py" -f "$DATA_DIR/td_speedglide.txt" \
    -o "$SCRIPT_DIR/td_speedglide.png" \
    --labelx "time (s)" --labely "transport ratio" --width 1200 --height 400 --cols 1

python3 "$ROOT_DIR/documentation/Plot/PyConPlot.py" -f "$DATA_DIR/td_multitap.txt" \
    -o "$SCRIPT_DIR/td_multitap.png" \
    --labelx "sample index" --labely "amplitude" --width 1200 --height 400 --cols 1

echo "Done: $SCRIPT_DIR/td_pitchwobble.png, td_speedglide.png, td_multitap.png"
