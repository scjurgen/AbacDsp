#!/usr/bin/env bash
# Builds VariSpeedTapeDelayExplore/WobbleDelayExplore/MultiTapDelayExplore, runs each, and
# renders every plot into its class's own subfolder via PyConPlot.py/spectrogram_plot.py.
# Self-contained: sets up its own build dir and Python venv, both gitignored. Run from
# anywhere; every path below is resolved relative to this script's own location.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -DEXPLORE_STUFF=ON -DCMAKE_BUILD_TYPE=Release -DPACKAGE_TESTS=OFF
cmake --build "$BUILD_DIR" --target VariSpeedTapeDelayExplore WobbleDelayExplore MultiTapDelayExplore

DATA_DIR="$SCRIPT_DIR/generated"
mkdir -p "$DATA_DIR"

"$BUILD_DIR/documentation/Delays/VariSpeedTapeDelayExplore" "$DATA_DIR"
"$BUILD_DIR/documentation/Delays/WobbleDelayExplore" "$DATA_DIR"
"$BUILD_DIR/documentation/Delays/MultiTapDelayExplore" "$DATA_DIR"

if [ ! -d "$SCRIPT_DIR/.venv" ]; then
    python3 -m venv "$SCRIPT_DIR/.venv"
fi
source "$SCRIPT_DIR/.venv/bin/activate"
pip install --quiet --upgrade pip
pip install --quiet -r "$SCRIPT_DIR/requirements.txt"

PLOT="$ROOT_DIR/documentation/Plot/PyConPlot.py"
SPEC="$SCRIPT_DIR/spectrogram_plot.py"

VARI="$SCRIPT_DIR/VariSpeedTapeDelay"
WOBBLE="$SCRIPT_DIR/WobbleDelay"
MULTI="$SCRIPT_DIR/MultiTapDelay"

python3 "$PLOT" -f "$DATA_DIR/td_pitchwobble.txt" -o "$VARI/td_pitchwobble.png" \
    --labelx "time (s)" --labely "tracked pitch (Hz)" --width 1200 --height 400 --cols 1

python3 "$PLOT" -f "$DATA_DIR/td_speedglide.txt" -o "$VARI/td_speedglide.png" \
    --labelx "time (s)" --labely "transport ratio" --width 1200 --height 400 --cols 1

for ratio in "0.1" "0.25" "0.5" "0.99" "1.5"; do
  python3 "$SPEC" \
      --panel "$DATA_DIR/td_aliasing_writeside_${ratio}_sinc4.txt" "sinc4" \
      --panel "$DATA_DIR/td_aliasing_writeside_${ratio}_sinc69.txt" "sinc_69_768" \
      -o "$VARI/td_aliasing_writeside_${ratio}.png" --maxfreq 24000
done

# Constant 1000Hz probe, VariSpeedTapeDelay's ratio stepped 1.0 -> 0.1 (settling between
# steps via the transport's own accel/brake glide) - x-axis is ratio, not time.
python3 "$SPEC" \
    --panel "$DATA_DIR/td_aliasing_ratioglide_sinc4.txt" "sinc4" \
    --panel "$DATA_DIR/td_aliasing_ratioglide_sinc69.txt" "sinc_69_768" \
    -o "$VARI/td_aliasing_ratioglide.png" --maxfreq 24000 \
    --xrange 1.0 0.1 --xlabel "ratio"

python3 "$PLOT" -f "$DATA_DIR/wd_pitchwobble.txt" -o "$WOBBLE/wd_pitchwobble.png" \
    --labelx "time (s)" --labely "tracked pitch (Hz)" --width 1200 --height 400 --cols 1

python3 "$PLOT" -f "$DATA_DIR/wd_delaystability.txt" -o "$WOBBLE/wd_delaystability.png" \
    --labelx "time (s)" --labely "distance (samples)" --width 1200 --height 400 --cols 1

python3 "$PLOT" -f "$DATA_DIR/wd_retune.txt" -o "$WOBBLE/wd_retune.png" \
    --labelx "time (s)" --labely "distance (samples)" --width 1200 --height 400 --cols 1

python3 "$SPEC" \
    --panel "$DATA_DIR/wd_sweep_plain.txt" "no modulation" \
    --panel "$DATA_DIR/wd_sweep_flutter.txt" "flutter" \
    --panel "$DATA_DIR/wd_sweep_wow.txt" "wow" \
    --panel "$DATA_DIR/wd_sweep_both.txt" "wow + flutter" \
    -o "$WOBBLE/wd_sweep.png" --maxfreq 24000 --height 2.0

python3 "$SPEC" \
    --panel "$DATA_DIR/wd_sweep_retune.txt" "retune 200 -> 6000 samples" \
    -o "$WOBBLE/wd_sweep_retune.png" --maxfreq 24000 --height 2.0

python3 "$PLOT" -f "$DATA_DIR/td_multitap.txt" -o "$MULTI/td_multitap.png" \
    --labelx "sample index" --labely "amplitude" --width 1200 --height 400 --cols 1

echo "Done: PNGs in $VARI, $WOBBLE and $MULTI; WAVs (for listening) and .txt data in $DATA_DIR"
