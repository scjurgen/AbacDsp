#!/usr/bin/env python3
"""Read-only survey: for every MidiDrums/**/*.mid, is its End_of_Track tick within
1 tick of an exact multiple of its own bar length (ticks_per_beat * 4 * numerator /
denominator - the real musical bar length, accounting for the time signature's
denominator)? A file outside that 1-tick tolerance is not cleanly loopable at its
declared length.

Note: as of this writing, GrooveKit.h's readGrooveMetadata() and
MidiDrums/analyze_variations.py's compute_bars() do NOT account for the denominator
(they divide by the numerator alone), so their own "bars" output is wrong by a
factor of denominator/4 for any non-quarter-note denominator (6/8, 7/8, 9/8, 12/8,
5/8, ...) - this script's numbers are the corrected reference, not theirs.

Never modifies any file - a diagnostic pass only, to see where the library actually
stands before deciding how (or whether) to correct any of it.

Usage:
    analyze_eot_alignment.py [--root DIR] [--out FILE]
"""
from __future__ import annotations

import argparse
from pathlib import Path

import mido


def _default_root() -> Path:
    script_dir = Path(__file__).resolve().parent
    return script_dir.parent.parent / "MidiDrums"


def analyze(mid_path: Path) -> dict:
    midi_file = mido.MidiFile(str(mid_path), clip=True)
    ticks_per_quarter_note = midi_file.ticks_per_beat
    end_of_track_tick = 0
    max_note_on_tick = 0
    time_signatures: set[tuple[int, int]] = set()
    for track in midi_file.tracks:
        absolute_tick = 0
        for msg in track:
            absolute_tick += msg.time
            if msg.type == "end_of_track":
                end_of_track_tick = max(end_of_track_tick, absolute_tick)
            elif msg.type == "note_on" and msg.velocity > 0:
                max_note_on_tick = max(max_note_on_tick, absolute_tick)
            elif msg.type == "time_signature":
                time_signatures.add((msg.numerator, msg.denominator))

    result = {
        "path": mid_path,
        "tpqn": ticks_per_quarter_note,
        "eot": end_of_track_tick,
        "last_note": max_note_on_tick,
        "time_signatures": time_signatures,
    }

    if len(time_signatures) != 1:
        result["category"] = "no time signature" if not time_signatures else "mixed meter (skipped)"
        return result

    numerator, denominator = next(iter(time_signatures))
    ticks_per_bar = ticks_per_quarter_note * 4 * numerator // denominator
    result["ticks_per_bar"] = ticks_per_bar

    if end_of_track_tick == 0:
        result["category"] = "missing EOT"
    elif end_of_track_tick <= max_note_on_tick:
        result["category"] = "EOT at/before last note"
    else:
        remainder = end_of_track_tick % ticks_per_bar
        if remainder == 0:
            result["category"] = "clean"
        elif remainder == ticks_per_bar - 1:
            result["category"] = "1-tick-short quirk"
        else:
            off_by = min(remainder, ticks_per_bar - remainder)
            result["category"] = "flagged"
            result["off_by"] = off_by
    return result


def write_report(results: list[dict], root: Path, report_path: Path) -> None:
    by_category: dict[str, list[dict]] = {}
    for r in results:
        by_category.setdefault(r["category"], []).append(r)

    lines = ["# EOT alignment survey", "", f"{len(results)} .mid file(s) scanned under `{root}`.", ""]
    for category, items in sorted(by_category.items(), key=lambda kv: -len(kv[1])):
        lines.append(f"## {category} ({len(items)})")
        if category == "clean":
            lines.append("(not itemised - this is the expected/healthy state)")
            lines.append("")
            continue
        for r in items:
            rel = r["path"].relative_to(root)
            detail = f"tpqn={r['tpqn']} eot={r['eot']} last_note={r['last_note']}"
            if "ticks_per_bar" in r:
                detail += f" ticks_per_bar={r['ticks_per_bar']}"
            if "off_by" in r:
                detail += f" off_by={r['off_by']}"
            if r["time_signatures"]:
                detail += f" time_sig={sorted(r['time_signatures'])}"
            lines.append(f"- `{rel}`: {detail}")
        lines.append("")

    report_path.write_text("\n".join(lines), encoding="utf-8")


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--root", type=Path, default=_default_root())
    ap.add_argument("--out", type=Path, default=Path(__file__).resolve().parent / "eot_alignment_report.md")
    args = ap.parse_args()

    mid_paths = sorted(args.root.rglob("*.mid"))
    results = [analyze(p) for p in mid_paths]

    by_category: dict[str, list[dict]] = {}
    for r in results:
        by_category.setdefault(r["category"], []).append(r)

    print(f"{len(results)} .mid file(s) scanned under {args.root}\n")
    for category, items in sorted(by_category.items(), key=lambda kv: -len(kv[1])):
        print(f"  {len(items):5d}  {category}")

    write_report(results, args.root, args.out)
    print(f"\nFull report written to {args.out}")


if __name__ == "__main__":
    main()
