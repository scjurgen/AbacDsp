#!/usr/bin/env bash
# Runs StampedYinExplore (and StampedYinGuitarExplore when GUITAR_WAV is set) through dev-scripts/dev-explore.sh and
# renders the plots via PyConPlot.py. Self-contained: sets up its own Python venv (gitignored). Run from anywhere;
# every path below is resolved relative to this script's own location.
#
#   GUITAR_WAV=<recording.wav> [GUITAR_SCORE=<score>] ./generate.sh
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
DATA_DIR="$SCRIPT_DIR/generated"
mkdir -p "$DATA_DIR"

EXPLORE_OUT_DIR="$DATA_DIR" "$ROOT_DIR/dev-scripts/dev-explore.sh" "$SCRIPT_DIR/StampedYinExplore.cpp"

if [ -n "${GUITAR_WAV:-}" ]; then
    GUITAR_SCORE="${GUITAR_SCORE:-$ROOT_DIR/documentation/NoteSegmenter/scores/guitar_c_major_scale.score}" \
        GUITAR_WAV="$GUITAR_WAV" EXPLORE_OUT_DIR="$DATA_DIR" \
        "$ROOT_DIR/dev-scripts/dev-explore.sh" "$SCRIPT_DIR/StampedYinGuitarExplore.cpp" -- \
        -DNDEBUG -I"$ROOT_DIR/3rdparty/AudioFile"
fi

if [ ! -d "$SCRIPT_DIR/.venv" ]; then
    python3 -m venv "$SCRIPT_DIR/.venv"
fi
source "$SCRIPT_DIR/.venv/bin/activate"
pip install --quiet --upgrade pip
pip install --quiet -r "$SCRIPT_DIR/requirements.txt"

PLOT="$ROOT_DIR/documentation/Plot/PyConPlot.py"

python3 "$PLOT" -f "$DATA_DIR/yin_range.txt" -o "$SCRIPT_DIR/yin_range.png" \
    --labelx "frequency (Hz)" --labely "error (cent)" --width 1200 --height 450 --cols 1 --logx

python3 "$PLOT" -f "$DATA_DIR/yin_latency.txt" -o "$SCRIPT_DIR/yin_latency.png" \
    --labelx "frequency (Hz)" --labely "time (ms)" --width 1200 --height 450 --cols 1 --logx

python3 "$PLOT" -f "$DATA_DIR/yin_noise.txt" -o "$SCRIPT_DIR/yin_noise.png" \
    --labelx "noise amplitude" --labely "readings (%)" --width 1200 --height 350 --cols 1 --logx

if [ -f "$DATA_DIR/yin_guitar.txt" ]; then
    python3 "$PLOT" -f "$DATA_DIR/yin_guitar.txt" -o "$SCRIPT_DIR/yin_guitar.png" \
        --labelx "time (s)" --labely "MIDI note" --width 1200 --height 450 --cols 1
fi

echo "Done: the yin_*.png files in $SCRIPT_DIR"
