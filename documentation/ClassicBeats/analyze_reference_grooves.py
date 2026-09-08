#!/usr/bin/env python3
"""Derives per-genre statistical profiles from reference drum-groove MIDI packs.

Reads only aggregate numbers (hit-probability grids, BPM/feel distributions, ghost-note and
hihat-open ratios, fill density) via the `midicsv` CLI - never reproduces or stores any actual
note sequence, so its output (see write_report()) is safe to keep even though the source
material itself (this repo's own MidiDrums/<Genre>/ packs, and any external reference library
passed via --root) is copyrighted and not redistributed.

Usage:
    analyze_reference_grooves.py --root DIR [--root DIR ...] [--out FILE]
                                  [--max-variations-per-genre N]

Each --root is scanned for the GENRE_SOURCE_FOLDERS mapping below (case-insensitive, matched
against the last path component of every folder directly under that root - so it works against
both this project's flat MidiDrums/<Genre>/*.mid layout and an external library's nested
<Genre>/<feel_timesig>/<section>/Variation_NN.mid layout, since files are found by recursive
search regardless of the folders between them).
"""
from __future__ import annotations

import argparse
import csv
import io
import json
import re
import subprocess
from collections import Counter
from dataclasses import dataclass, field
from pathlib import Path

# Genre -> the top-level reference-pack folder names that count as source material for it (see
# .claude/classic-beats-full-kit.md for how this table was chosen). A folder may serve more than
# one genre. Matching is case-insensitive against a folder's own name, spaces/hyphens/underscores
# treated as equivalent.
GENRE_SOURCE_FOLDERS: dict[str, list[str]] = {
    "Rock": ["British Invasion Grooves", "AOR Grooves", "High Energy Grooves", "Action Drums"],
    "Blues": ["Blues", "The Blues"],
    "Jazz Swing": ["Urban Jazz Grooves"],
    "Latin": ["Latin Jazz Grooves"],
    "Funk": ["Funk", "Modern Funk Grooves"],
    "Fusion": ["Fusion Grooves", "Linear Fusion", "Metal Fusion", "Deathlike Fusion", "Progressive Fusion"],
    "Hip-Hop": ["Hip-Hop Offbeats", "Hip-Hop Grooves", "EZX Hip-Hop!"],
    "Progressive": ["Progressive", "Progressive Fusion", "The Progressive Foundry", "Progressive Metal", "EZX Progressive"],
    "Reggae": ["Reggae Beats", "EZX Reggae"],
    "Pop": ["UK Pop Grooves", "Sixties Pop Grooves", "Singer Songwriter Grooves", "Indiependent", "EZX Dream Pop", "EZX Number 1 Hits"],
    "Soul": ["Soul Grooves", "Gospel Grooves", "Contemporary R&B Grooves", "Urban Jazz Grooves"],
}

# Six broad categories, matching the vocabulary MidiDrums/README.md and analyze_variations.py
# already use. Kick is tracked separately (near-omnipresent, not useful as a grid signal).
KICK_NOTES = {35, 36}
SNARE_FAMILY_NOTES = {37, 38, 39, 40, 71}  # snare + rimshot/sidestick, for ghost-note rate
CATEGORY_NOTES: dict[str, set[int]] = {
    "snare": {37, 38, 39, 40, 71},
    "tom": {41, 43, 45, 47, 48, *range(72, 83)},
    "hihat": {12, 13, 14, 15, 16, 21, 22, 23, 24, 25, 26, 42, 44, 46, 49, 61, 62, 63, 64, 65},
    "cymbal": {27, 28, 29, 30, 31, 32, 50, 51, 52, 53, 54, 55, 57, 58, 59, 60},
    "percussion": {56, 66, 67, 68, 69, 70},
}
HIHAT_OPEN_NOTES = {12, 13, 14, 15, 16, 23, 24, 25, 26, 46, 49, 65}
HIHAT_CLOSED_NOTES = {21, 22, 42, 44, 61, 62, 63, 64}
GRID_STEPS = 16
GHOST_VELOCITY_THRESHOLD = 60
FEEL_KEYWORDS = ("STRAIGHT", "SWING", "SHUFFLE")
FEEL_PREFIXES = {"str": "straight", "swg": "swing", "shf": "shuffle"}  # MidiDrums/README.md filename convention
FEEL_JSON_MAP = {"even": "straight", "swing": "swing", "shuffle": "shuffle"}
VARIATION1_RE = re.compile(r"(?:^|_)v0*1$", re.IGNORECASE)  # local: base_v1.mid
VARIATION1_EXTERNAL_RE = re.compile(r"^Variation_0*1$", re.IGNORECASE)  # external: Variation_01.mid


def category_of(note: int) -> str | None:
    for category, notes in CATEGORY_NOTES.items():
        if note in notes:
            return category
    return None


@dataclass
class FileStats:
    bpm: float | None
    feel: str | None
    bar_ticks: int
    grid_hits: dict[str, set[int]]  # category -> set of 16th-grid steps hit anywhere in the file
    ghost_count: int
    snare_family_count: int
    hihat_open_count: int
    hihat_closed_count: int
    bar_hit_counts: list[int]  # hits per bar, for fill-density


def is_variation_one(path: Path) -> bool:
    return bool(VARIATION1_RE.search(path.stem) or VARIATION1_EXTERNAL_RE.match(path.stem))


def detect_feel(path: Path) -> str | None:
    json_path = path.with_suffix(".json")
    if json_path.exists():
        try:
            data = json.loads(json_path.read_text(encoding="utf-8"))
        except (json.JSONDecodeError, OSError):
            data = {}
        feel = data.get("rhythm", {}).get("feel")
        if feel in FEEL_JSON_MAP:
            return FEEL_JSON_MAP[feel]
    upper = str(path).upper()
    for keyword in FEEL_KEYWORDS:
        if keyword in upper:
            return keyword.lower()
    prefix = path.stem.split("_", 1)[0].lower()
    return FEEL_PREFIXES.get(prefix)


def read_midicsv(path: Path) -> list[list[str]] | None:
    try:
        result = subprocess.run(["midicsv", str(path)], capture_output=True, text=True, check=True, timeout=10)
    except (subprocess.CalledProcessError, subprocess.TimeoutExpired, OSError):
        return None
    return list(csv.reader(io.StringIO(result.stdout)))


def analyze_file(path: Path) -> FileStats | None:
    rows = read_midicsv(path)
    if not rows:
        return None

    ticks_per_quarter = 480
    bpm: float | None = None
    numerator, denom_exp = 4, 2
    note_events: list[tuple[int, int, int]] = []  # tick, note, velocity

    for row in rows:
        row = [c.strip() for c in row]
        if len(row) < 3:
            continue
        kind = row[2]
        if kind == "Header" and len(row) >= 6:
            ticks_per_quarter = int(row[5])
        elif kind == "Tempo" and len(row) >= 4 and bpm is None:
            microseconds_per_quarter = int(row[3])
            if microseconds_per_quarter > 0:
                bpm = 60_000_000.0 / microseconds_per_quarter
        elif kind == "Time_signature" and len(row) >= 5:
            numerator, denom_exp = int(row[3]), int(row[4])
        elif kind == "Note_on_c" and len(row) >= 6:
            tick, note, velocity = int(row[1]), int(row[4]), int(row[5])
            if velocity > 0:
                note_events.append((tick, note, velocity))

    if not note_events:
        return None

    bar_ticks = ticks_per_quarter * 4 * numerator // (2**denom_exp)
    if bar_ticks <= 0:
        bar_ticks = ticks_per_quarter * 4

    grid_hits: dict[str, set[int]] = {c: set() for c in CATEGORY_NOTES}
    ghost_count = 0
    snare_family_count = 0
    hihat_open_count = 0
    hihat_closed_count = 0
    bar_counter: Counter[int] = Counter()

    for tick, note, velocity in note_events:
        bar_counter[tick // bar_ticks] += 1
        if note in SNARE_FAMILY_NOTES:
            snare_family_count += 1
            if velocity < GHOST_VELOCITY_THRESHOLD:
                ghost_count += 1
        if note in HIHAT_OPEN_NOTES:
            hihat_open_count += 1
        elif note in HIHAT_CLOSED_NOTES:
            hihat_closed_count += 1
        category = category_of(note)
        if category is not None:
            step = int((tick % bar_ticks) / bar_ticks * GRID_STEPS) % GRID_STEPS
            grid_hits[category].add(step)

    bar_count = max(bar_counter) + 1 if bar_counter else 1
    bar_hit_counts = [bar_counter.get(b, 0) for b in range(bar_count)]

    return FileStats(
        bpm=bpm,
        feel=detect_feel(path),
        bar_ticks=bar_ticks,
        grid_hits=grid_hits,
        ghost_count=ghost_count,
        snare_family_count=snare_family_count,
        hihat_open_count=hihat_open_count,
        hihat_closed_count=hihat_closed_count,
        bar_hit_counts=bar_hit_counts,
    )


def normalize(name: str) -> str:
    return re.sub(r"[\s_-]+", " ", name).strip().lower()


def find_source_folders(root: Path, folder_names: list[str]) -> list[Path]:
    wanted = {normalize(n) for n in folder_names}
    if not root.is_dir():
        return []
    return [p for p in root.iterdir() if p.is_dir() and normalize(p.name.split("@", 1)[-1]) in wanted]


@dataclass
class GenreProfile:
    genre: str
    source_folders: list[str] = field(default_factory=list)
    files_analyzed: int = 0
    variation1_files: int = 0
    bpms: list[float] = field(default_factory=list)
    feel_counts: Counter[str] = field(default_factory=Counter)
    grid_hits_by_category: dict[str, list[int]] = field(default_factory=lambda: {c: [0] * GRID_STEPS for c in CATEGORY_NOTES})
    ghost_count: int = 0
    snare_family_count: int = 0
    hihat_open_count: int = 0
    hihat_closed_count: int = 0
    fill_ratios: list[float] = field(default_factory=list)

    def add(self, stats: FileStats, *, is_v1: bool) -> None:
        self.files_analyzed += 1
        if is_v1:
            self.variation1_files += 1
        if stats.bpm:
            self.bpms.append(stats.bpm)
        self.feel_counts[stats.feel or "unknown"] += 1
        for category, steps in stats.grid_hits.items():
            for step in steps:
                self.grid_hits_by_category[category][step] += 1
        self.ghost_count += stats.ghost_count
        self.snare_family_count += stats.snare_family_count
        self.hihat_open_count += stats.hihat_open_count
        self.hihat_closed_count += stats.hihat_closed_count
        if len(stats.bar_hit_counts) >= 2:
            other_bars = stats.bar_hit_counts[:-1]
            average_other = sum(other_bars) / len(other_bars) if other_bars else 0.0
            if average_other > 0:
                self.fill_ratios.append(stats.bar_hit_counts[-1] / average_other)


def grid_symbol(probability: float) -> str:
    if probability >= 0.8:
        return "#"
    if probability >= 0.5:
        return "+"
    if probability >= 0.2:
        return ":"
    return "."


def collect_profile(genre: str, folder_names: list[str], roots: list[Path], max_variations: int) -> GenreProfile:
    profile = GenreProfile(genre=genre)
    all_files: list[Path] = []
    for root in roots:
        for folder in find_source_folders(root, folder_names):
            profile.source_folders.append(str(folder))
            all_files.extend(folder.rglob("*.mid"))

    v1_files = [f for f in all_files if is_variation_one(f)]
    other_files = [f for f in all_files if f not in v1_files]

    budget = max(max_variations - len(v1_files), 0)
    selected = v1_files + other_files[:budget]

    for path in selected:
        stats = analyze_file(path)
        if stats is not None:
            profile.add(stats, is_v1=path in v1_files)

    return profile


def format_grid(profile: GenreProfile) -> list[str]:
    lines = []
    for category in CATEGORY_NOTES:
        counts = profile.grid_hits_by_category[category]
        symbols = "".join(grid_symbol(c / profile.files_analyzed) for c in counts) if profile.files_analyzed else "." * GRID_STEPS
        lines.append(f"  {category:<10} `{symbols}`")
    return lines


def write_report(profiles: list[GenreProfile], out_path: Path) -> None:
    lines = [
        "# Classic-beats reference-groove profiles",
        "",
        "Aggregate statistics only, derived from copyrighted reference packs via "
        "`analyze_reference_grooves.py` - no note sequence from those packs is reproduced here "
        "or in the generated grooves; these numbers only inform what's genre-idiomatic (typical "
        "kick/snare/hihat placement, tempo range, ghost-note and fill density). See "
        "`.claude/classic-beats-full-kit.md` for the genre-to-source-folder mapping rationale.",
        "",
        "Grid columns are 16th-note steps 1-16 of a 4/4 bar; symbols are the fraction of "
        "analyzed files with at least one hit of that category at that step: "
        "`#` >=80%, `+` >=50%, `:` >=20%, `.` <20%. Non-4/4 files are folded into the same "
        "16-step grid on a proportional basis, so treat the grid as a rough shape, not a "
        "literal 16th-note transcription.",
        "",
    ]
    for profile in profiles:
        lines.append(f"## {profile.genre}")
        if not profile.source_folders:
            lines.append("No matching source folder found - grooves for this genre are composed from general genre knowledge, not a derived profile.")
            lines.append("")
            continue
        lines.append(f"Source folders: {', '.join(profile.source_folders)}")
        lines.append(f"Files analyzed: {profile.files_analyzed} ({profile.variation1_files} variation-1 blueprints)")
        if profile.bpms:
            lines.append(f"BPM: {min(profile.bpms):.0f}-{max(profile.bpms):.0f} (median {sorted(profile.bpms)[len(profile.bpms) // 2]:.0f})")
        feel_total = sum(profile.feel_counts.values()) or 1
        feel_text = ", ".join(f"{k}:{v / feel_total:.0%}" for k, v in profile.feel_counts.most_common())
        lines.append(f"Feel distribution: {feel_text}")
        ghost_rate = profile.ghost_count / profile.snare_family_count if profile.snare_family_count else 0.0
        lines.append(f"Snare/rimshot ghost-note rate: {ghost_rate:.0%}")
        hihat_total = profile.hihat_open_count + profile.hihat_closed_count
        open_rate = profile.hihat_open_count / hihat_total if hihat_total else 0.0
        lines.append(f"Hihat open ratio: {open_rate:.0%}")
        if profile.fill_ratios:
            avg_fill = sum(profile.fill_ratios) / len(profile.fill_ratios)
            lines.append(f"Last-bar density vs. other bars: {avg_fill:.1f}x")
        lines.append("Hit-probability grid:")
        lines.extend(format_grid(profile))
        lines.append("")

    out_path.write_text("\n".join(lines), encoding="utf-8")


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--root", type=Path, action="append", required=True, help="a MidiDrums-shaped root; pass multiple times")
    ap.add_argument("--out", type=Path, default=Path(__file__).resolve().parent / "genre_profiles.md")
    ap.add_argument("--max-variations-per-genre", type=int, default=250)
    args = ap.parse_args()

    profiles = [
        collect_profile(genre, folder_names, args.root, args.max_variations_per_genre)
        for genre, folder_names in GENRE_SOURCE_FOLDERS.items()
    ]
    write_report(profiles, args.out)
    print(f"wrote {args.out}")


if __name__ == "__main__":
    main()
