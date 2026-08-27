#!/usr/bin/env python3
"""CLI: python -m rhythm_library.validation <generated-dir>

Structural validation only (see tests/ for the full musical/catalog test suite): every .mid
opens in mido, is type 0 with one track, and appears in manifest.json; every manifest entry
points at a file that exists.
"""

from __future__ import annotations

import glob
import json
import os
import sys

import mido

from rhythm_library.generate import AREAS


def validate(output_dir: str) -> list[str]:
    """Validates only what rhythm_library itself writes (the area subdirectories, manifest.json,
    README.md) - `output_dir` may be shared with generate_metronome_midi.py's own flat
    click-groove output, which is a different project's files and out of scope here."""
    problems: list[str] = []
    manifest_path = os.path.join(output_dir, "manifest.json")
    if not os.path.isfile(manifest_path):
        return [f"manifest.json missing at {manifest_path}"]
    with open(manifest_path, encoding="utf-8") as manifest_file:
        manifest = json.load(manifest_file)
    manifest_paths = {entry["relative_path"] for entry in manifest}

    mid_files = [
        path
        for area in AREAS
        for path in glob.glob(os.path.join(output_dir, area, "**", "*.mid"), recursive=True)
    ]
    disk_paths = {os.path.relpath(path, output_dir).replace(os.sep, "/") for path in mid_files}

    for path in sorted(disk_paths - manifest_paths):
        problems.append(f"{path}: on disk but not in manifest.json")
    for path in sorted(manifest_paths - disk_paths):
        problems.append(f"{path}: in manifest.json but missing on disk")

    for path in mid_files:
        relative = os.path.relpath(path, output_dir).replace(os.sep, "/")
        try:
            midi_file = mido.MidiFile(path)
        except Exception as exc:  # noqa: BLE001 - report any parse failure as a validation problem
            problems.append(f"{relative}: failed to open ({exc})")
            continue
        if midi_file.type != 0:
            problems.append(f"{relative}: type {midi_file.type}, expected 0")
        if len(midi_file.tracks) != 1:
            problems.append(f"{relative}: {len(midi_file.tracks)} tracks, expected 1")
    return problems


def main() -> None:
    if len(sys.argv) != 2:
        print("usage: python -m rhythm_library.validation <generated-dir>")
        sys.exit(2)
    problems = validate(sys.argv[1])
    if problems:
        print(f"{len(problems)} problem(s):")
        for problem in problems:
            print(f"  {problem}")
        sys.exit(1)
    print("OK")


if __name__ == "__main__":
    main()
