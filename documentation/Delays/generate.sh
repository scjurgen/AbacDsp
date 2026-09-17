#!/usr/bin/env bash
# Builds DelaysExplore, runs it, and renders all plots via PyConPlot.py/spectrogram_plot.py.
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

"$BUILD_DIR/documentation/Delays/DelaysExplore" "$DATA_DIR"

if [ ! -d "$SCRIPT_DIR/.venv" ]; then
    python3 -m venv "$SCRIPT_DIR/.venv"
fi
source "$SCRIPT_DIR/.venv/bin/activate"
pip install --quiet --upgrade pip
pip install --quiet -r "$SCRIPT_DIR/requirements.txt"

PLOT="$ROOT_DIR/documentation/Plot/PyConPlot.py"
SPEC="$SCRIPT_DIR/spectrogram_plot.py"

python3 "$PLOT" -f "$DATA_DIR/td_pitchwobble.txt" -o "$SCRIPT_DIR/td_pitchwobble.png" \
    --labelx "time (s)" --labely "tracked pitch (Hz)" --width 1200 --height 400 --cols 1

python3 "$PLOT" -f "$DATA_DIR/td_speedglide.txt" -o "$SCRIPT_DIR/td_speedglide.png" \
    --labelx "time (s)" --labely "transport ratio" --width 1200 --height 400 --cols 1

python3 "$PLOT" -f "$DATA_DIR/td_readheadwobble.txt" -o "$SCRIPT_DIR/td_readheadwobble.png" \
    --labelx "time (s)" --labely "tracked pitch (Hz)" --width 1200 --height 400 --cols 1

python3 "$PLOT" -f "$DATA_DIR/td_driftstability.txt" -o "$SCRIPT_DIR/td_driftstability.png" \
    --labelx "time (s)" --labely "write/read distance error (samples)" --width 1200 --height 400 --cols 1

python3 "$PLOT" -f "$DATA_DIR/td_multitap.txt" -o "$SCRIPT_DIR/td_multitap.png" \
    --labelx "sample index" --labely "amplitude" --width 1200 --height 400 --cols 1

# Aliasing: two ratios (1:10 extreme, 1:2 mild control) x two transports (write-side
# VariSpeedTapeDelay, read-side OrganicChorusTransport), each a 2-panel sinc4/sinc_69_768
# spectrogram. --maxfreq matches each ratio's own Nyquist x2 (the fold point) for legibility.
python3 "$SPEC" \
    --panel "$DATA_DIR/td_aliasing_writeside_1to10_sinc4.txt" "sinc4" \
    --panel "$DATA_DIR/td_aliasing_writeside_1to10_sinc69768.txt" "sinc_69_768" \
    -o "$SCRIPT_DIR/td_aliasing_writeside_1to10.png" --maxfreq 4800

python3 "$SPEC" \
    --panel "$DATA_DIR/td_aliasing_writeside_1to2_sinc4.txt" "sinc4" \
    --panel "$DATA_DIR/td_aliasing_writeside_1to2_sinc69768.txt" "sinc_69_768" \
    -o "$SCRIPT_DIR/td_aliasing_writeside_1to2.png" --maxfreq 24000

python3 "$SPEC" \
    --panel "$DATA_DIR/td_aliasing_readside_1to10_sinc4.txt" "sinc4" \
    --panel "$DATA_DIR/td_aliasing_readside_1to10_sinc69768.txt" "sinc_69_768" \
    -o "$SCRIPT_DIR/td_aliasing_readside_1to10.png" --maxfreq 4800

python3 "$SPEC" \
    --panel "$DATA_DIR/td_aliasing_readside_1to2_sinc4.txt" "sinc4" \
    --panel "$DATA_DIR/td_aliasing_readside_1to2_sinc69768.txt" "sinc_69_768" \
    -o "$SCRIPT_DIR/td_aliasing_readside_1to2.png" --maxfreq 24000

# Constant 1000Hz probe, VariSpeedTapeDelay's ratio stepped 1.0 -> 0.1 (settling between
# steps via the transport's own accel/brake glide) - x-axis is ratio, not time.
python3 "$SPEC" \
    --panel "$DATA_DIR/td_aliasing_ratioglide_sinc4.txt" "sinc4" \
    --panel "$DATA_DIR/td_aliasing_ratioglide_sinc69768.txt" "sinc_69_768" \
    -o "$SCRIPT_DIR/td_aliasing_ratioglide.png" --maxfreq 24000 \
    --xrange 1.0 0.1 --xlabel "ratio"

echo "Done: PNGs in $SCRIPT_DIR, WAVs (for listening) and .txt data in $DATA_DIR"
