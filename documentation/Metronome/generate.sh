#!/usr/bin/env bash
# Generates the "Metronome" groove-style MIDI files into MidiDrums/Metronome/ and
# the clicklow/clickhigh round-robin samples into samples/drums/reggae/.
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

python3 "$SCRIPT_DIR/generate_metronome_midi.py"
python3 "$SCRIPT_DIR/generate_click_samples.py"
