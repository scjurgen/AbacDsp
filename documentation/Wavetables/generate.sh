#!/usr/bin/env bash
# Builds WavetablesExplore, runs it, and renders all seven plots via PyConPlot.py /
# spectrogram_plot.py. Self-contained: sets up its own build dir and Python venv, both
# gitignored. Run from anywhere; every path below is resolved relative to this script's
# own location.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -DEXPLORE_STUFF=ON -DCMAKE_BUILD_TYPE=Release -DPACKAGE_TESTS=OFF
cmake --build "$BUILD_DIR" --target WavetablesExplore

DATA_DIR="$SCRIPT_DIR/generated"
mkdir -p "$DATA_DIR"

"$BUILD_DIR/documentation/Wavetables/WavetablesExplore" \
    "$DATA_DIR/wt_pitch_range.txt" "$DATA_DIR/wt_mip_boundary.txt" "$DATA_DIR/wt_pwm.txt" \
    "$DATA_DIR/wt_morph.txt" "$DATA_DIR/wt_change_vs_set_frequency.txt" "$DATA_DIR/wt_pitch_bend.txt" \
    "$DATA_DIR/wt_pitch_bend_spectrogram_up.txt" "$DATA_DIR/wt_pitch_bend_spectrogram_down.txt" \
    "$DATA_DIR/wt_pitch_bend_up.wav" "$DATA_DIR/wt_pitch_bend_down.wav"

if [ ! -d "$SCRIPT_DIR/.venv" ]; then
    python3 -m venv "$SCRIPT_DIR/.venv"
fi
source "$SCRIPT_DIR/.venv/bin/activate"
pip install --quiet --upgrade pip
pip install --quiet -r "$SCRIPT_DIR/requirements.txt"

python3 "$ROOT_DIR/documentation/Plot/PyConPlot.py" -f "$DATA_DIR/wt_pitch_range.txt" \
    -o "$SCRIPT_DIR/wt_pitch_range.png" \
    --labelx "frequency (Hz)" --labely "magnitude (dB)" --width 1200 --height 350 --cols 1

python3 "$ROOT_DIR/documentation/Plot/PyConPlot.py" -f "$DATA_DIR/wt_mip_boundary.txt" \
    -o "$SCRIPT_DIR/wt_mip_boundary.png" \
    --labelx "frequency (Hz)" --labely "magnitude (dB)" --width 1200 --height 400 --cols 1

python3 "$ROOT_DIR/documentation/Plot/PyConPlot.py" -f "$DATA_DIR/wt_pwm.txt" -o "$SCRIPT_DIR/wt_pwm.png" \
    --labelx "frequency (Hz)" --labely "magnitude (dB)" --width 1200 --height 350 --cols 1

python3 "$ROOT_DIR/documentation/Plot/PyConPlot.py" -f "$DATA_DIR/wt_morph.txt" -o "$SCRIPT_DIR/wt_morph.png" \
    --labelx "frequency (Hz)" --labely "magnitude (dB)" --width 1200 --height 400 --cols 1

python3 "$ROOT_DIR/documentation/Plot/PyConPlot.py" -f "$DATA_DIR/wt_change_vs_set_frequency.txt" \
    -o "$SCRIPT_DIR/wt_change_vs_set_frequency.png" \
    --labelx "frequency (Hz)" --labely "magnitude (dB)" --width 1200 --height 500 --cols 1

python3 "$ROOT_DIR/documentation/Plot/PyConPlot.py" -f "$DATA_DIR/wt_pitch_bend.txt" \
    -o "$SCRIPT_DIR/wt_pitch_bend.png" \
    --labelx "frequency (Hz)" --labely "magnitude (dB)" --width 1200 --height 350 --cols 1

python3 "$SCRIPT_DIR/spectrogram_plot.py" \
    --panel "$DATA_DIR/wt_pitch_bend_spectrogram_up.txt" "Sine at 880Hz, bent up 1 octave to 1760Hz" \
    --panel "$DATA_DIR/wt_pitch_bend_spectrogram_down.txt" "Sine at 880Hz, bent down 1 octave to 440Hz" \
    -o "$SCRIPT_DIR/wt_pitch_bend_spectrogram.png" --width 12 --height 3.5 --maxfreq 4000

echo "Done: $SCRIPT_DIR/wt_pitch_range.png, wt_mip_boundary.png, wt_pwm.png, wt_morph.png," \
     "wt_change_vs_set_frequency.png, wt_pitch_bend.png, wt_pitch_bend_spectrogram.png"
echo "Listening test audio (not checked in): $DATA_DIR/wt_pitch_bend_up.wav, wt_pitch_bend_down.wav"
