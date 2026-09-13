#!/usr/bin/env python3
"""CLI: python -m rhythm_library.deploy [--generated DIR] [--midi-drums DIR]

Copies the reviewed rhythm-guide library from its staging folder (generated/<area>/<family>/)
into MidiDrums/<area>/<family>/, the folder GrooveKit actually scans. GrooveKit's
splitStyleAndVariation (GrooveKit.h) silently skips any file whose stem doesn't end in
"_v<digits>" - our generator's own filenames don't carry that suffix (each pattern is a single,
deterministic take, not one of several round-robin variations), so this step appends "_v1" to
every copied filename, matching the convention generate_metronome_midi.py's retired click groove
used for the same reason. Alongside each ".mid" this also writes a MidiDrums-style sidecar
".json" (see MidiDrums/README.md), built from the same PatternSpec the .mid came from - the only
place in this project where these grooves' idealBpm/feel/timeSignature/bars/dominantSounds are
derived, so GrooveKit's bulk browsing scan never needs to reopen the .mid file. Directory
nesting is copied as-is (GrooveKit's recursive scan handles arbitrary depth).
"""

from __future__ import annotations

import argparse
import json
import os
import shutil
from collections import Counter

from rhythm_library import instruments as instr
from rhythm_library.generate import AREAS
from rhythm_library.model import PatternSpec
from rhythm_library.patterns import all_patterns

# "2:1" is a hard-triplet shuffle; any other non-null swing_ratio (e.g. "60:40")
# reads as a lighter swing feel rather than a full shuffle.
_SHUFFLE_RATIO = "2:1"


def _default_generated_dir() -> str:
    script_dir = os.path.dirname(os.path.abspath(__file__))
    return os.path.join(os.path.dirname(script_dir), "generated")


def _default_midi_drums_dir() -> str:
    script_dir = os.path.dirname(os.path.abspath(__file__))
    repo_root = os.path.dirname(os.path.dirname(os.path.dirname(script_dir)))
    return os.path.join(repo_root, "MidiDrums")


def _feel(pattern: PatternSpec) -> str:
    if pattern.swing_ratio is None:
        return "even"
    return "shuffle" if pattern.swing_ratio == _SHUFFLE_RATIO else "swing"


def _time_signature(pattern: PatternSpec) -> str:
    return "mixed" if len(pattern.meters) > 1 else pattern.meter_display


def _dominant_sounds(pattern: PatternSpec, top_n: int = 2) -> list[str]:
    hits: Counter[str] = Counter()
    for event in pattern.events:
        category = instr.NOTE_CATEGORY.get(event.note)
        if category is not None:
            hits[category] += 1
    return [category for category, _count in hits.most_common(top_n)]


def sidecar_json(pattern: PatternSpec) -> dict:
    return {
        "idealBpm": pattern.bpm,
        "rhythm": {"feel": _feel(pattern), "timeSignature": _time_signature(pattern)},
        "bars": pattern.bar_count,
        "dominantSounds": _dominant_sounds(pattern),
    }


def deploy(generated_dir: str, midi_drums_dir: str) -> int:
    patterns_by_relative_path = {p.relative_path: p for p in all_patterns()}
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
                relative_path = os.path.join(relative_dir, filename)
                target_dir = os.path.join(midi_drums_dir, relative_dir)
                os.makedirs(target_dir, exist_ok=True)
                target_stem = os.path.join(target_dir, f"{stem}_v1")
                shutil.copyfile(os.path.join(root, filename), f"{target_stem}.mid")

                pattern = patterns_by_relative_path.get(relative_path.replace(os.sep, "/"))
                if pattern is not None:
                    with open(f"{target_stem}.json", "w", encoding="utf-8") as sidecar_file:
                        json.dump(sidecar_json(pattern), sidecar_file, indent=2)
                        sidecar_file.write("\n")
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
