#!/usr/bin/env bash
# Builds SamplerateConverterExplore, runs it, and renders every aliasing spectrogram
# comparison PNG via spectrogram_plot.py. Self-contained: sets up its own build dir and
# Python venv, both gitignored. Run from anywhere; every path below is resolved relative
# to this script's own location.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -DEXPLORE_STUFF=ON -DCMAKE_BUILD_TYPE=Release -DPACKAGE_TESTS=OFF
cmake --build "$BUILD_DIR" --target SamplerateConverterExplore

DATA_DIR="$SCRIPT_DIR/generated"
mkdir -p "$DATA_DIR"

"$BUILD_DIR/documentation/SamplerateConverter/SamplerateConverterExplore" "$DATA_DIR"

if [ ! -d "$SCRIPT_DIR/.venv" ]; then
    python3 -m venv "$SCRIPT_DIR/.venv"
fi
source "$SCRIPT_DIR/.venv/bin/activate"
pip install --quiet --upgrade pip
pip install --quiet -r "$SCRIPT_DIR/requirements.txt"

SPEC="$SCRIPT_DIR/spectrogram_plot.py"
FILTERS=(sinc4 init_7_128 init_11_128 init_21_512 init_33_512 init_69_768)

for cls in push pull; do
  for ratio in "0.1" "1.0" "4.0"; do
    PANELS=()
    for f in "${FILTERS[@]}"; do
      PANELS+=(--panel "$DATA_DIR/sr_aliasing_${cls}_${ratio}_${f}.txt" "$f")
    done
    python3 "$SPEC" "${PANELS[@]}" \
        -o "$SCRIPT_DIR/sr_aliasing_${cls}_${ratio}.png" --height 2.0
  done
done

echo "Done: PNGs in $SCRIPT_DIR, WAVs (for listening) and .txt data (incl. sr_snr_table.txt) in $DATA_DIR"
