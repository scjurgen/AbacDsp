#!/usr/bin/env python3
"""CLI: python -m rhythm_library.generate [--output DIR] [--area AREA] [--family FAMILY] [--clean]"""

from __future__ import annotations

import argparse
import os
import shutil

from rhythm_library.documentation import generate_all_documentation
from rhythm_library.manifest import write_manifest
from rhythm_library.midi_writer import write_pattern
from rhythm_library.patterns import all_patterns

AREAS = ("educational", "looper", "performance")


def _default_output_dir() -> str:
    script_dir = os.path.dirname(os.path.abspath(__file__))
    return os.path.join(os.path.dirname(script_dir), "generated")


def _clean(output_dir: str) -> None:
    """Removes only what this library itself writes (the three area subdirectories,
    manifest.json, and README.md) - `--output` may point at a directory shared with
    generate_metronome_midi.py's own flat click-groove output, which must survive untouched."""
    for area in AREAS:
        area_dir = os.path.join(output_dir, area)
        if os.path.isdir(area_dir):
            shutil.rmtree(area_dir)
    for filename in ("manifest.json", "README.md"):
        path = os.path.join(output_dir, filename)
        if os.path.isfile(path):
            os.remove(path)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", default=_default_output_dir())
    parser.add_argument("--area", default=None, help="Only generate this area (educational|looper|performance).")
    parser.add_argument("--family", default=None, help="Only generate this family.")
    parser.add_argument("--clean", action="store_true", help="Remove this library's own previous output (not the whole --output directory) before generating.")
    args = parser.parse_args()

    if args.clean:
        _clean(args.output)

    patterns = all_patterns()
    if args.area:
        patterns = [p for p in patterns if p.area == args.area]
    if args.family:
        patterns = [p for p in patterns if p.family == args.family]

    if not patterns:
        print("No patterns matched --area/--family filters.")
        return

    for pattern in patterns:
        path = write_pattern(pattern, args.output)
        print(f"wrote {path}")

    manifest_path = write_manifest(patterns, args.output)
    print(f"wrote {manifest_path}")

    doc_paths = generate_all_documentation(patterns, args.output)
    print(f"wrote {len(doc_paths)} README files")

    print(f"\n{len(patterns)} pattern(s) generated to {args.output}")
    for area in sorted({p.area for p in patterns}):
        print(f"  {area}: {len([p for p in patterns if p.area == area])}")


if __name__ == "__main__":
    main()
