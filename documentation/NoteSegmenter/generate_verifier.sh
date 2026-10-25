#!/usr/bin/env bash
# Builds NoteVerifierExplore, verifies a local recording against its score and renders the three plots via PyConPlot.py.
# usage: generate_verifier.sh [recording.wav] [score file]
# The recordings are not part of the repository; the defaults expect the guitar take in guitar-samples/ at the root.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"
RECORDING="${1:-$ROOT_DIR/guitar-samples/scale C3-C4 from bar 1 and C4-C5 from bar 4-120 BPM.wav}"
SCORE="${2:-$SCRIPT_DIR/scores/guitar_c_major_scale.score}"

if [ ! -f "$RECORDING" ]; then
    echo "recording not found: $RECORDING" >&2
    exit 1
fi

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -DEXPLORE_STUFF=ON -DCMAKE_BUILD_TYPE=Release -DPACKAGE_TESTS=OFF
cmake --build "$BUILD_DIR" --target NoteVerifierExplore

DATA_DIR="$SCRIPT_DIR/generated"
mkdir -p "$DATA_DIR"

"$BUILD_DIR/documentation/NoteSegmenter/NoteVerifierExplore" "$RECORDING" "$SCORE" "$DATA_DIR" | tee "$DATA_DIR/nv_report.txt"

if [ ! -d "$SCRIPT_DIR/.venv" ]; then
    python3 -m venv "$SCRIPT_DIR/.venv"
fi
source "$SCRIPT_DIR/.venv/bin/activate"
pip install --quiet --upgrade pip
pip install --quiet -r "$SCRIPT_DIR/requirements.txt"

PLOT="$ROOT_DIR/documentation/Plot/PyConPlot.py"

python3 "$PLOT" -f "$DATA_DIR/nv_full.txt" -o "$SCRIPT_DIR/nv_full.png" --width 1600 --height 450 --cols 1
python3 "$PLOT" -f "$DATA_DIR/nv_empty.txt" -o "$SCRIPT_DIR/nv_empty.png" --width 1400 --height 450 --cols 1
python3 "$PLOT" -f "$DATA_DIR/nv_close.txt" -o "$SCRIPT_DIR/nv_close.png" --width 900 --height 350 --cols 2

echo "Done: $SCRIPT_DIR/nv_full.png, nv_empty.png, nv_close.png and $DATA_DIR/nv_report.txt"
