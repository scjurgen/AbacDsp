"""Generates generated/README.md, generated/<area>/README.md, and
generated/<area>/<family>/README.md from the same PatternSpec/manifest data used to write the
MIDI files - never a second, hand-maintained documentation database. See PLAN.md's
"Hierarchical README requirements" for the required structure.
"""

from __future__ import annotations

import os
from collections import defaultdict

from rhythm_library.model import PatternSpec

AREA_TITLES = {
    "educational": "Educational patterns",
    "looper": "Looper patterns",
    "performance": "Performance patterns",
}

AREA_PURPOSE = {
    "educational": "isolated timing, meter, subdivision, feel, and internal-time drills",
    "looper": "count-ins, clear loop boundaries, phrase navigation, and sparse recording guides",
    "performance": "longer repeating guide tracks with loop-start and turnaround cues",
}


def _escape(text: str) -> str:
    return text.replace("|", "\\|")


def humanize(slug: str) -> str:
    return " ".join(word.capitalize() for word in slug.split("-"))


def group_by(patterns: list[PatternSpec], key) -> dict[str, list[PatternSpec]]:
    grouped: dict[str, list[PatternSpec]] = defaultdict(list)
    for pattern in patterns:
        grouped[key(pattern)].append(pattern)
    return grouped


def render_click_language(pattern: PatternSpec) -> str:
    if pattern.click_language:
        return f"`{pattern.click_language}`"
    if pattern.grouping:
        return f"grouped `{'+'.join(str(g) for g in pattern.grouping)}`"
    return ""


def render_form_diagram(pattern: PatternSpec) -> str:
    if not pattern.sections:
        return ""
    return " | ".join(section.label for section in pattern.sections)


def render_pattern_table(patterns: list[PatternSpec]) -> str:
    lines = ["| File | Meter | Bars | Main pattern / grouping | Click language | Difficulty | Best use | Notes |",
             "|---|---:|---:|---|---|---|---|---|"]
    for pattern in sorted(patterns, key=lambda p: p.id):
        grouping = "+".join(str(g) for g in pattern.grouping) if pattern.grouping else "-"
        click = render_click_language(pattern) or "-"
        best_use = _escape(pattern.recommended_use or pattern.description)
        notes = _escape(pattern.simplification_note or pattern.source_context or "")
        lines.append(
            f"| [{pattern.id}.mid]({pattern.id}.mid) | {pattern.meter_display} | {pattern.bar_count} | "
            f"{grouping} | {click} | {pattern.difficulty} | {best_use} | {notes} |"
        )
    return "\n".join(lines)


def render_pattern_notes(patterns: list[PatternSpec]) -> str:
    blocks = []
    for pattern in sorted(patterns, key=lambda p: p.id):
        lines = [f"### {pattern.display_name}", ""]
        lines.append(f"- File: [{pattern.id}.mid]({pattern.id}.mid)")
        lines.append(f"- Meter and length: {pattern.meter_display}, {pattern.bar_count} bar(s), {pattern.loop_length_ticks} ticks")
        if pattern.grouping:
            lines.append(f"- Pulse/grouping: {'+'.join(str(g) for g in pattern.grouping)}")
        if pattern.click_language:
            lines.append(f"- Events: `{pattern.click_language}`")
        if pattern.recommended_use:
            lines.append(f"- Use: {pattern.recommended_use}")
        if pattern.listening_target:
            lines.append(f"- Listening target: {pattern.listening_target}")
        if pattern.loop_cue_behaviour:
            lines.append(f"- Loop/cue behaviour: {pattern.loop_cue_behaviour}")
        if pattern.source_context:
            lines.append(f"- Cultural context: {pattern.source_context}")
        if pattern.simplification_note:
            lines.append(f"- Simplification note: {pattern.simplification_note}")
        blocks.append("\n".join(lines))
    return "\n\n".join(blocks)


def generate_family_readme(area: str, family: str, patterns: list[PatternSpec], output_dir: str) -> str:
    title = humanize(family)
    lines = [f"# {title}", ""]
    lines.append(
        f"{len(patterns)} pattern(s) in `{area}/{family}/`. "
        + {
            "educational": "Each isolates one timing, meter, subdivision, or feel concept.",
            "looper": "Each helps a solo performer recognise the loop's boundary and navigate its phrase.",
            "performance": "Each is a longer running guide track with a clear loop-start cue and a sparse turnaround.",
        }.get(area, "")
    )
    lines.append("")
    lines.append("## Quick selection")
    lines.append("")
    for pattern in sorted(patterns, key=lambda p: p.id)[:5]:
        lines.append(f"- Choose `{pattern.id}.mid` when {pattern.recommended_use or pattern.description[0].lower() + pattern.description[1:]}")
    lines.append("")
    lines.append("## Pattern catalog")
    lines.append("")
    lines.append(render_pattern_table(patterns))
    lines.append("")
    lines.append("## Pattern notes")
    lines.append("")
    lines.append(render_pattern_notes(patterns))
    lines.append("")
    path = os.path.join(output_dir, area, family, "README.md")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as readme:
        readme.write("\n".join(lines))
    return path


def generate_area_readme(area: str, patterns: list[PatternSpec], output_dir: str) -> str:
    by_family = group_by(patterns, lambda p: p.family)
    lines = [f"# {AREA_TITLES[area]}", ""]
    lines.append(f"{len(patterns)} pattern(s) covering {AREA_PURPOSE[area]}.")
    lines.append("")
    if area == "educational":
        lines += [
            "Material progresses roughly:",
            "",
            "1. fundamentals",
            "2. straightforward meters",
            "3. subdivisions and feel",
            "4. odd and mixed meters",
            "5. syncopation/polyrhythm",
            "6. sparse-click and silent-bar internal-time challenges",
            "7. culturally specific rhythm guides",
            "",
            "Suggested workflow: begin with conventional downbeat markers, move to backbeat and sparse clicks, "
            "add subdivisions, practice meter grouping, remove click information with gap patterns, and treat "
            "idiomatic rhythm guides as musical vocabulary studies, not merely click exercises.",
            "",
            "\"Easy\", \"challenge\", and \"tricky\" are expressed as tags and recommended-use metadata rather "
            "than rigid folder names, since a pattern may belong to several pedagogical categories at once.",
            "",
        ]
    elif area == "looper":
        lines += [
            "**Count-in**: a cue bar before the loop body. **Loop start**: bar 1's cue. **Bar marker**: a "
            "marker at every bar boundary. **Phrase marker**: a cue at a sub-phrase boundary (e.g. bar 5 of an "
            "eight-bar loop). **Turnaround cue** / **fill cue**: a small pickup in the loop's final bar. "
            "**Return**: the point where the loop cycles back to bar 1.",
            "",
            "Design convention for four-bar loop files: bar 1 has a distinctive loop-start cue, bars 2-3 are "
            "comparatively restrained, bar 4 may contain a sparse turnaround or subdivision lift, and the last "
            "event leads cleanly into the next bar 1.",
            "",
        ]
    elif area == "performance":
        lines += [
            "Running guide tracks for rehearsal, live use, or accompaniment/lead-track workflows - deliberately "
            "not full drum arrangements. The common form is four bars: bar 1 opens with a loop-start cue, and "
            "the final bar carries a turnaround leading seamlessly back to bar 1.",
            "",
            "Choose straight for a plain reference, swing/shuffle for jazz or blues feel, compound/odd meter "
            "for 6/8-12/8 or additive grouping, Afro-Cuban/Afro-diasporic or Middle Eastern guides for those "
            "idioms, and mixed meter when the piece itself changes meter.",
            "",
        ]
    lines.append("## Families")
    lines.append("")
    lines.append("| Family | Pattern count | Documentation |")
    lines.append("|---|---:|---|")
    for family in sorted(by_family):
        lines.append(f"| {humanize(family)} | {len(by_family[family])} | [Open]({family}/README.md) |")
    lines.append("")
    path = os.path.join(output_dir, area, "README.md")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as readme:
        readme.write("\n".join(lines))
    return path


def generate_root_readme(patterns: list[PatternSpec], output_root: str) -> str:
    by_area = group_by(patterns, lambda p: p.area)
    by_family = group_by(patterns, lambda p: (p.area, p.family))
    lines = [
        "# Metronome / rhythm-guide SMF library",
        "",
        "A generated SMF0 click, rhythmic-guide, looper, and performance-cue library. Standard MIDI File "
        "type 0, 480 ticks per quarter note, General MIDI percussion channel 10 (zero-based channel 9), "
        "default 120 BPM unless a pattern states otherwise. Tempo and grouping are metadata: DAWs may follow "
        "or override embedded tempo/time-signature meta events differently, so check import settings.",
        "",
        "SMF0 has exactly one MIDI track (`MTrk`), but that one track still carries every meta and channel "
        "event - track name, description, time signature, tempo, section markers, and every note.",
        "",
        "## How to browse the library",
        "",
        f"- **Educational** ({AREA_PURPOSE['educational']}): [Educational](educational/README.md)",
        f"- **Looper** ({AREA_PURPOSE['looper']}): [Looper](looper/README.md)",
        f"- **Performance** ({AREA_PURPOSE['performance']}): [Performance](performance/README.md)",
        "",
        "## Click language / note legend",
        "",
        "This project uses its own note map (see `src/includes/Sampler/GrooveNoteMap.h` and "
        "`MidiDrums/README.md`), not literal General MIDI drum notes.",
        "",
        "| Symbol / role | Note | Instrument | Typical meaning |",
        "|---|---:|---|---|",
        "| X | 56 | Woodblock | Strong count-in, loop-start, or section cue |",
        "| H | 38 | Snare | Primary accent, bar start, strong backbeat |",
        "| h | 36 | Kick | Low primary pulse or grounded downbeat |",
        "| L | 37 | Rimshot | Normal metronome click |",
        "| l | 42 | Hihat closed | Light subdivision |",
        "| D | 36 | Kick | \"Dum\" in Middle Eastern rhythmic-cycle guides |",
        "| T | 37 | Rimshot | \"Tak\" in Middle Eastern rhythmic-cycle guides |",
        "| - | n/a | Silence | Intentional absence of a click |",
        "",
        "Symbols are explanatory notation only; a file may combine several notes at one musical position.",
        "",
        "## General usage",
        "",
        "Count-ins, loop starts, bar markers, and turnaround cues are encoded as `marker`/`text`/`track_name` "
        "meta events; swing timing and silence/gap practice are encoded as real tick positions, never merely "
        "implied by the filename; mixed-meter patterns carry a fresh `time_signature` event at every bar where "
        "the meter changes. These are intentionally sparse guide tracks, not fully orchestrated drum grooves - "
        "import a pattern into your DAW or looper and verify tempo, bar alignment, and whether the host follows "
        "embedded tempo/time-signature metadata before relying on it.",
        "",
        "## Cultural-context note",
        "",
        "The Afro-Cuban, Afro-diasporic/Brazilian, and Middle Eastern patterns in this library are introductory "
        "rhythm guides. Names, phrasing, sound choice, tempo, swing, articulation, ensemble roles, and regional "
        "variants matter and are simplified here - treat these files as a starting point for study, not as "
        "authoritative substitutes for the traditions. See "
        "[educational/afro-cuban](educational/afro-cuban/README.md), "
        "[educational/afro-diasporic](educational/afro-diasporic/README.md), and "
        "[educational/middle-eastern](educational/middle-eastern/README.md).",
        "",
        "## Full navigation",
        "",
        "| Area | Family | Pattern count | Documentation |",
        "|---|---|---:|---|",
    ]
    for area in ("educational", "looper", "performance"):
        for family in sorted({f for (a, f) in by_family if a == area}):
            count = len(by_family[(area, family)])
            lines.append(f"| {AREA_TITLES[area].split()[0]} | {humanize(family)} | {count} | [Open]({area}/{family}/README.md) |")
    lines.append("")
    lines.append(f"Total: {len(patterns)} patterns across {len(by_area)} areas.")
    lines.append("")
    path = os.path.join(output_root, "README.md")
    os.makedirs(output_root, exist_ok=True)
    with open(path, "w", encoding="utf-8") as readme:
        readme.write("\n".join(lines))
    return path


def generate_all_documentation(patterns: list[PatternSpec], output_root: str) -> list[str]:
    written = [generate_root_readme(patterns, output_root)]
    by_area = group_by(patterns, lambda p: p.area)
    for area, area_patterns in by_area.items():
        written.append(generate_area_readme(area, area_patterns, output_root))
        by_family = group_by(area_patterns, lambda p: p.family)
        for family, family_patterns in by_family.items():
            written.append(generate_family_readme(area, family, family_patterns, output_root))
    return written
