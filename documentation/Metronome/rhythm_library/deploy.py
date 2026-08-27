#!/usr/bin/env python3
"""CLI: python -m rhythm_library.deploy [--generated DIR] [--midi-drums DIR]

Copies the reviewed rhythm-guide library from its staging folder (generated/<area>/<family>/)
into MidiDrums/<area>/<family>/, the folder GrooveKit actually scans. GrooveKit's
splitStyleAndVariation (GrooveKit.h) silently skips any file whose stem doesn't end in
"_v<digits>" - our generator's own filenames don't carry that suffix (each pattern is a single,
deterministic take, not one of several round-robin variations), so this step appends "_v1" to
every copied filename, matching the convention generate_metronome_midi.py's retired click groove
used for the same reason. This is the only difference between generated/ and MidiDrums/ output;
directory nesting is copied as-is (GrooveKit's recursive scan handles arbitrary depth).
"""

from __future__ import annotations

import argparse
import os
import shutil

from rhythm_library.generate import AREAS


def _default_generated_dir() -> str:
    script_dir = os.path.dirname(os.path.abspath(__file__))
    return os.path.join(os.path.dirname(script_dir), "generated")


def _default_midi_drums_dir() -> str:
    script_dir = os.path.dirname(os.path.abspath(__file__))
    repo_root = os.path.dirname(os.path.dirname(os.path.dirname(script_dir)))
    return os.path.join(repo_root, "MidiDrums")


def deploy(generated_dir: str, midi_drums_dir: str) -> int:
    copied = 0
    for area in AREAS:
        area_dir = os.path.join(generated_dir, area)
        if not os.path.isdir(area_dir):
            continue
        for root, _dirs, files in os.walk(area_dir):
            for filename in files:
                if not filename.endswith(".mid"):
                    continue
                stem = filename[: -len(".mid")]
                relative_dir = os.path.relpath(root, generated_dir)
                target_dir = os.path.join(midi_drums_dir, relative_dir)
                os.makedirs(target_dir, exist_ok=True)
                target_path = os.path.join(target_dir, f"{stem}_v1.mid")
                shutil.copyfile(os.path.join(root, filename), target_path)
                copied += 1
    return copied


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--generated", default=_default_generated_dir())
    parser.add_argument("--midi-drums", default=_default_midi_drums_dir())
    args = parser.parse_args()

    if not os.path.isdir(args.generated):
        raise SystemExit(f"generated dir not found: {args.generated} (run rhythm_library.generate first)")

    count = deploy(args.generated, args.midi_drums)
    print(f"copied {count} file(s) into {args.midi_drums}")


if __name__ == "__main__":
    main()
