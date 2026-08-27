#!/usr/bin/env python3
"""Generates tapelooper's built-in "Metronome" groove-style MIDI files.

Each pattern is one bar of two notes - CLICK_LOW (downbeat/accent) and
CLICK_HIGH (everything else) - repeated BARS_PER_FILE times and written
with mido as an ordinary Standard MIDI File. GrooveKit derives a groove's
own loop length from its last note's tick (rounded up to the next whole
beat), so the file's total length becomes the loop length; the engine
loops it on its own after that. See README.md in this folder for the
musical reasoning behind each pattern and its velocity choices.

Output goes to MidiDrums/Metronome/, next to the existing MidiDrums/Reggae
Beats/ folder GrooveKit already scans. Each pattern also gets a small JSON
sidecar (bars/feel/tempo hint), matching every other groove file's own
convention (see GrooveKit.h's GrooveSidecar).
"""

from __future__ import annotations

import json
import os
from dataclasses import dataclass

import mido

TICKS_PER_QUARTER = 480
BEATS_PER_BAR = 4
BARS_PER_FILE = 4
CLICK_LOW = 100
CLICK_HIGH = 101
NOTE_DURATION_TICKS = 10

# Cubic velocity-to-gain curve applied downstream (GrooveKit.h: (v/127)^3), so these
# velocities were chosen by ear against that curve, not on a linear scale.
VEL_ACCENT = 127  # downbeat
VEL_BEAT = 100  # other quarter-note beats
VEL_GHOST_MEDIUM = 60  # 8th-note offbeats, triplet partials, the shuffle's swung note
VEL_GHOST_QUIET = 32  # 16th-note subdivisions that aren't also an 8th-note offbeat


@dataclass
class Hit:
    tick: int
    note: int
    velocity: int


@dataclass
class Pattern:
    filename: str
    feel: str
    time_signature: str
    ideal_bpm: float
    hits: list[Hit]


def _quarter_hits() -> list[Hit]:
    return [
        Hit(beat * TICKS_PER_QUARTER, CLICK_LOW if beat == 0 else CLICK_HIGH, VEL_ACCENT if beat == 0 else VEL_BEAT)
        for beat in range(BEATS_PER_BAR)
    ]


def straight_pattern() -> list[Hit]:
    return _quarter_hits()


def eighths_pattern() -> list[Hit]:
    hits = _quarter_hits()
    eighth = TICKS_PER_QUARTER // 2
    for beat in range(BEATS_PER_BAR):
        hits.append(Hit(beat * TICKS_PER_QUARTER + eighth, CLICK_HIGH, VEL_GHOST_MEDIUM))
    return sorted(hits, key=lambda h: h.tick)


def sixteenths_pattern() -> list[Hit]:
    hits = eighths_pattern()
    sixteenth = TICKS_PER_QUARTER // 4
    for beat in range(BEATS_PER_BAR):
        base = beat * TICKS_PER_QUARTER
        hits.append(Hit(base + sixteenth, CLICK_HIGH, VEL_GHOST_QUIET))
        hits.append(Hit(base + 3 * sixteenth, CLICK_HIGH, VEL_GHOST_QUIET))
    return sorted(hits, key=lambda h: h.tick)


def triplets_pattern() -> list[Hit]:
    hits = _quarter_hits()
    triplet = TICKS_PER_QUARTER // 3
    for beat in range(BEATS_PER_BAR):
        base = beat * TICKS_PER_QUARTER
        hits.append(Hit(base + triplet, CLICK_HIGH, VEL_GHOST_MEDIUM))
        hits.append(Hit(base + 2 * triplet, CLICK_HIGH, VEL_GHOST_MEDIUM))
    return sorted(hits, key=lambda h: h.tick)


def shuffle_pattern() -> list[Hit]:
    # A swung/shuffled beat: the middle triplet partial is dropped, leaving a
    # long-short (2:1) pair - the classic shuffle feel, not a straight triplet.
    hits = _quarter_hits()
    triplet = TICKS_PER_QUARTER // 3
    for beat in range(BEATS_PER_BAR):
        hits.append(Hit(beat * TICKS_PER_QUARTER + 2 * triplet, CLICK_HIGH, VEL_GHOST_MEDIUM))
    return sorted(hits, key=lambda h: h.tick)


PATTERNS = [
    Pattern("straight_4#4_v1", "straight", "4/4", 100.0, straight_pattern()),
    Pattern("eighths_4#4_v1", "straight", "4/4", 100.0, eighths_pattern()),
    Pattern("sixteenths_4#4_v1", "straight", "4/4", 100.0, sixteenths_pattern()),
    Pattern("triplets_4#4_v1", "triplet", "4/4", 100.0, triplets_pattern()),
    Pattern("shuffle_4#4_v1", "shuffle", "4/4", 100.0, shuffle_pattern()),
]


def write_pattern(pattern: Pattern, out_dir: str) -> None:
    midi_file = mido.MidiFile(type=0, ticks_per_beat=TICKS_PER_QUARTER)
    track = mido.MidiTrack()
    midi_file.tracks.append(track)
    numerator, denominator = (int(part) for part in pattern.time_signature.split("/"))
    track.append(mido.MetaMessage("time_signature", numerator=numerator, denominator=denominator, time=0))
    track.append(mido.MetaMessage("set_tempo", tempo=mido.bpm2tempo(pattern.ideal_bpm), time=0))

    bar_ticks = BEATS_PER_BAR * TICKS_PER_QUARTER
    events: list[tuple[int, bool, int, int]] = []
    for bar in range(BARS_PER_FILE):
        for hit in pattern.hits:
            tick = bar * bar_ticks + hit.tick
            events.append((tick, True, hit.note, hit.velocity))
            events.append((tick + NOTE_DURATION_TICKS, False, hit.note, 0))
    events.sort(key=lambda e: e[0])

    last_tick = 0
    for tick, is_on, note, velocity in events:
        message_type = "note_on" if is_on else "note_off"
        track.append(mido.Message(message_type, note=note, velocity=velocity, time=tick - last_tick, channel=9))
        last_tick = tick

    os.makedirs(out_dir, exist_ok=True)
    midi_file.save(os.path.join(out_dir, pattern.filename + ".mid"))

    sidecar = {
        "idealBpm": pattern.ideal_bpm,
        "rhythm": {"feel": pattern.feel, "timeSignature": pattern.time_signature},
        "dominantSounds": ["click_low", "click_high"],
    }
    with open(os.path.join(out_dir, pattern.filename + ".json"), "w", encoding="utf-8") as sidecar_file:
        json.dump(sidecar, sidecar_file, indent=2)
        sidecar_file.write("\n")


def main() -> None:
    script_dir = os.path.dirname(os.path.abspath(__file__))
    root_dir = os.path.abspath(os.path.join(script_dir, "..", ".."))
    out_dir = os.path.join(root_dir, "MidiDrums", "Metronome")
    for pattern in PATTERNS:
        write_pattern(pattern, out_dir)
        print(f"wrote {os.path.join(out_dir, pattern.filename)}.mid (+.json)")


if __name__ == "__main__":
    main()
