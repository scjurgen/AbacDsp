#!/usr/bin/env bash
# Generates the rhythm-guide MIDI library into generated/ (a local, gitignored staging folder
# reviewed here before being copied into MidiDrums/) and the clicklow/clickhigh round-robin
# samples into samples/drums/reggae/.
# Self-contained: sets up its own local, gitignored Python venv. Run from anywhere;
# every path is resolved relative to this script's own location.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [ ! -d "$SCRIPT_DIR/.venv" ]; then
    python3 -m venv "$SCRIPT_DIR/.venv"
fi
source "$SCRIPT_DIR/.venv/bin/activate"
pip install --quiet --upgrade pip
pip install --quiet -r "$SCRIPT_DIR/requirements.txt"

PYTHONPATH="$SCRIPT_DIR" python3 -m rhythm_library.generate --output "$SCRIPT_DIR/generated"
python3 "$SCRIPT_DIR/generate_click_samples.py"
