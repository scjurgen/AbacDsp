"""Builds generated/manifest.json from the same PatternSpec objects used to write the MIDI
files - never a second, hand-maintained source of truth for meter/tags/description/etc."""

from __future__ import annotations

import json
import os

from rhythm_library import instruments as instr
from rhythm_library.model import PatternSpec, TICKS_PER_QUARTER


def _instrument_names(pattern: PatternSpec) -> list[str]:
    notes = sorted({event.note for event in pattern.events})
    return [instr.NOTE_NAMES.get(note, str(note)) for note in notes]


def pattern_manifest_entry(pattern: PatternSpec) -> dict:
    return {
        "relative_path": pattern.relative_path,
        "id": pattern.id,
        "display_name": pattern.display_name,
        "area": pattern.area,
        "family": pattern.family,
        "description": pattern.description,
        "meter": pattern.meter_display,
        "bpm": pattern.bpm,
        "ppqn": TICKS_PER_QUARTER,
        "bar_count": pattern.bar_count,
        "duration_ticks": pattern.loop_length_ticks,
        "duration_beats": pattern.loop_length_ticks / TICKS_PER_QUARTER,
        "count_in_bars": pattern.count_in_bars,
        "loopable": pattern.area in ("looper", "performance"),
        "grouping": pattern.grouping,
        "swing_ratio": pattern.swing_ratio,
        "tags": pattern.tags,
        "instruments": _instrument_names(pattern),
        "marker_positions": [{"tick": s.tick, "label": s.label} for s in pattern.sections],
        "source_context": pattern.source_context or None,
        "recommended_use": pattern.recommended_use or None,
    }


def build_manifest(patterns: list[PatternSpec]) -> list[dict]:
    return [pattern_manifest_entry(p) for p in sorted(patterns, key=lambda p: p.relative_path)]


def write_manifest(patterns: list[PatternSpec], out_dir: str) -> str:
    path = os.path.join(out_dir, "manifest.json")
    with open(path, "w", encoding="utf-8") as manifest_file:
        json.dump(build_manifest(patterns), manifest_file, indent=2)
        manifest_file.write("\n")
    return path
