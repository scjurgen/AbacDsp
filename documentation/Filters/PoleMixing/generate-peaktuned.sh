#!/usr/bin/env bash
# Runs PeakTunedExplore through dev-scripts/dev-explore.sh and renders its charts via PyConPlot.py.
# Self-contained: sets up its own Python venv (gitignored). Run from anywhere; every path below is
# resolved relative to this script's own location.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../../.." && pwd)"
DATA_DIR="$SCRIPT_DIR/generated"
mkdir -p "$DATA_DIR"

EXPLORE_OUT_DIR="$DATA_DIR" "$ROOT_DIR/dev-scripts/dev-explore.sh" "$SCRIPT_DIR/PeakTunedExplore.cpp"

if [ ! -d "$SCRIPT_DIR/.venv" ]; then
    python3 -m venv "$SCRIPT_DIR/.venv"
fi
source "$SCRIPT_DIR/.venv/bin/activate"
pip install --quiet --upgrade pip
pip install --quiet -r "$SCRIPT_DIR/requirements.txt"

PLOT="$ROOT_DIR/documentation/Plot/PyConPlot.py"

python3 "$PLOT" -f "$DATA_DIR/pt_tuning.txt" -o "$SCRIPT_DIR/pt_tuning.png" \
    --labelx "requested peak frequency (Hz)" --labely "response peak / requested" --width 1200 --height 500 --cols 1 --logx

python3 "$PLOT" -f "$DATA_DIR/pt_pitch.txt" -o "$SCRIPT_DIR/pt_pitch.png" \
    --labelx "requested frequency (Hz)" --labely "oscillation / requested" --width 1200 --height 500 --cols 1 --logx

python3 "$PLOT" -f "$DATA_DIR/pt_waveform.txt" -o "$SCRIPT_DIR/pt_waveform.png" \
    --labelx "time (ms)" --labely "output" --width 1200 --height 500 --cols 1

python3 "$PLOT" -f "$DATA_DIR/pt_glide_note.txt" -o "$SCRIPT_DIR/pt_glide_note.png" \
    --labelx "time (ms)" --labely "note" --width 1200 --height 300 --cols 1

python3 "$PLOT" -f "$DATA_DIR/pt_glide_hz.txt" -o "$SCRIPT_DIR/pt_glide_hz.png" \
    --labelx "time (ms)" --labely "frequency (Hz)" --width 1200 --height 300 --cols 1

echo "Done: pt_tuning.png, pt_pitch.png, pt_waveform.png, pt_glide_note.png, pt_glide_hz.png in $SCRIPT_DIR"
