#!/usr/bin/env python3
"""Characterizes how drummers build a fill, from the dedicated "FILLS" (and "INTRO_FILL(S)")
subfolders reference packs already carry alongside their verse/chorus grooves.

Like analyze_reference_grooves.py, this reads only aggregate/structural numbers - hit count,
which instrument category dominates, whether velocity ramps up across the bar, how much of the
bar toms occupy - and classifies each fill into one of a handful of named archetypes. It never
stores or reproduces an actual note sequence, so the output (see write_report()) is safe to keep
even though the source material is copyrighted. The idea (not any specific fill) is what
generate_classic_beats.py's fill code then applies.

Usage:
    analyze_fill_archetypes.py --root DIR [--root DIR ...] [--out FILE] [--max-per-genre N]
"""
from __future__ import annotations

import argparse
import io
import statistics
import subprocess
import csv
from collections import Counter
from dataclasses import dataclass, field
from pathlib import Path

from analyze_reference_grooves import (
    CATEGORY_NOTES,
    GENRE_SOURCE_FOLDERS,
    KICK_NOTES,
    find_source_folders,
)

TOM_CATEGORY = "tom"
SNARE_CATEGORY = "snare"


def full_category(note: int) -> str:
    if note in KICK_NOTES:
        return "kick"
    for category, notes in CATEGORY_NOTES.items():
        if note in notes:
            return category
    return "other"


def find_fill_files(root: Path, folder_names: list[str]) -> list[Path]:
    files: list[Path] = []
    for folder in find_source_folders(root, folder_names):
        for sub in folder.rglob("*"):
            if sub.is_dir() and "FILL" in sub.name.upper():
                files.extend(sub.glob("*.mid"))
    return files


def read_note_events(path: Path) -> list[tuple[int, int, int]] | None:
    try:
        result = subprocess.run(["midicsv", str(path)], capture_output=True, text=True, check=True, timeout=10)
    except (subprocess.CalledProcessError, subprocess.TimeoutExpired, OSError):
        return None
    events = []
    for row in csv.reader(io.StringIO(result.stdout)):
        row = [c.strip() for c in row]
        if len(row) >= 6 and row[2] == "Note_on_c":
            tick, note, velocity = int(row[1]), int(row[4]), int(row[5])
            if velocity > 0:
                events.append((tick, note, velocity))
    return sorted(events)


def velocity_slope(events: list[tuple[int, int, int]]) -> float:
    """Normalized linear-regression slope of velocity against event order, roughly -1..1."""
    if len(events) < 3:
        return 0.0
    velocities = [v for _, _, v in events]
    indices = list(range(len(events)))
    mean_i, mean_v = statistics.mean(indices), statistics.mean(velocities)
    numerator = sum((i - mean_i) * (v - mean_v) for i, v in zip(indices, velocities))
    denominator = sum((i - mean_i) ** 2 for i in indices)
    if denominator == 0:
        return 0.0
    slope = numerator / denominator
    velocity_range = max(velocities) - min(velocities) or 1
    return max(-1.0, min(1.0, slope * len(events) / velocity_range))


def classify(events: list[tuple[int, int, int]]) -> str:
    if not events:
        return "empty"
    note_counts = Counter(note for _, note, _ in events)
    dominant_note, dominant_count = note_counts.most_common(1)[0]
    dominant_fraction = dominant_count / len(events)
    tom_fraction = sum(1 for _, note, _ in events if full_category(note) == TOM_CATEGORY) / len(events)
    slope = velocity_slope(events)

    if dominant_fraction >= 0.6 and slope >= 0.3:
        return "crescendo_roll"
    if tom_fraction >= 0.4 and len({note for _, note, _ in events if full_category(note) == TOM_CATEGORY}) >= 2:
        return "descending_tom_run"
    if len(events) <= 6:
        return "sparse_punctuation"
    return "busy_groove_variation"


@dataclass
class GenreFillStats:
    genre: str
    source_folders: list[str] = field(default_factory=list)
    files_analyzed: int = 0
    archetype_counts: Counter[str] = field(default_factory=Counter)
    note_counts: list[int] = field(default_factory=list)
    tom_fractions: list[float] = field(default_factory=list)
    snare_fractions: list[float] = field(default_factory=list)


def collect(genre: str, folder_names: list[str], roots: list[Path], max_files: int) -> GenreFillStats:
    stats = GenreFillStats(genre=genre)
    all_files: list[Path] = []
    for root in roots:
        for folder in find_source_folders(root, folder_names):
            for sub in folder.rglob("*"):
                if sub.is_dir() and "FILL" in sub.name.upper():
                    matches = sorted(sub.glob("*.mid"))
                    if matches:
                        stats.source_folders.append(str(sub))
                        all_files.extend(matches)

    for path in all_files[:max_files]:
        events = read_note_events(path)
        if not events:
            continue
        stats.files_analyzed += 1
        stats.archetype_counts[classify(events)] += 1
        stats.note_counts.append(len(events))
        stats.tom_fractions.append(sum(1 for _, note, _ in events if full_category(note) == TOM_CATEGORY) / len(events))
        stats.snare_fractions.append(sum(1 for _, note, _ in events if full_category(note) == SNARE_CATEGORY) / len(events))

    return stats


def write_report(all_stats: list[GenreFillStats], out_path: Path) -> None:
    lines = [
        "# Classic-beats fill archetypes",
        "",
        "Aggregate/structural statistics only, derived from the dedicated \"FILLS\" (and "
        "\"INTRO_FILL(S)\") subfolders reference packs already carry - no note sequence from "
        "those packs is reproduced here or in the generated grooves. Each fill file is "
        "classified into one of four archetypes by its hit count, which instrument dominates, "
        "and whether velocity ramps up across the bar - see analyze_fill_archetypes.py's "
        "classify(). generate_classic_beats.py implements each archetype as an original, "
        "parameterized fill generator, picked per groove by what's idiomatic here.",
        "",
        "Archetypes: **crescendo_roll** (one voice, e.g. snare, with velocity ramping up across "
        "the bar - the classic drum-roll buildup), **descending_tom_run** (two or more toms, no "
        "strong single-voice dominance), **sparse_punctuation** (6 or fewer hits - a couple of "
        "accented hits, not a dense run), **busy_groove_variation** (denser, mixed "
        "kick/snare/hihat - closer to a varied groove bar than a distinct fill gesture).",
        "",
    ]
    total_by_archetype: Counter[str] = Counter()
    for stats in all_stats:
        lines.append(f"## {stats.genre}")
        if stats.files_analyzed == 0:
            lines.append("No FILLS-labeled reference material found.")
            lines.append("")
            continue
        lines.append(f"Files analyzed: {stats.files_analyzed} (from {len(set(stats.source_folders))} FILLS folder(s))")
        total = stats.files_analyzed
        breakdown = ", ".join(f"{k}:{v} ({v / total:.0%})" for k, v in stats.archetype_counts.most_common())
        lines.append(f"Archetype breakdown: {breakdown}")
        lines.append(f"Median hit count: {statistics.median(stats.note_counts):.0f} (range {min(stats.note_counts)}-{max(stats.note_counts)})")
        lines.append(f"Mean tom fraction: {statistics.mean(stats.tom_fractions):.0%}, mean snare fraction: {statistics.mean(stats.snare_fractions):.0%}")
        lines.append("")
        total_by_archetype.update(stats.archetype_counts)

    grand_total = sum(total_by_archetype.values()) or 1
    lines.append("## Overall (all genres combined)")
    for archetype, count in total_by_archetype.most_common():
        lines.append(f"- {archetype}: {count} ({count / grand_total:.0%})")
    lines.append("")

    out_path.write_text("\n".join(lines), encoding="utf-8")


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--root", type=Path, action="append", required=True)
    ap.add_argument("--out", type=Path, default=Path(__file__).resolve().parent / "fill_archetypes.md")
    ap.add_argument("--max-per-genre", type=int, default=200)
    args = ap.parse_args()

    all_stats = [collect(genre, folder_names, args.root, args.max_per_genre) for genre, folder_names in GENRE_SOURCE_FOLDERS.items()]
    write_report(all_stats, args.out)
    print(f"wrote {args.out}")


if __name__ == "__main__":
    main()
