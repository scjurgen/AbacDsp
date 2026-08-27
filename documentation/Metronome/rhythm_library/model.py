"""Declarative pattern model shared by every generator and by validation/documentation.

A PatternSpec is authored with absolute-tick EventSpecs (never per-note deltas) so the
generic scheduler in midi_writer.py is the only place that computes MIDI delta times. See
PLAN.md for the musical vocabulary (Pulse/Accent/Subdivision/Cue/Silence/Groove guide) and
the note-number palette this project uses instead of General MIDI.
"""

from __future__ import annotations

from dataclasses import dataclass, field

TICKS_PER_QUARTER = 480


@dataclass(frozen=True)
class Meter:
    numerator: int
    denominator: int

    @property
    def bar_ticks(self) -> int:
        return self.numerator * (TICKS_PER_QUARTER * 4 // self.denominator)

    @property
    def unit_ticks(self) -> int:
        """Ticks per counting unit: a quarter note for simple meters (denominator 4), an
        eighth note for compound/additive meters (denominator 8)."""
        return TICKS_PER_QUARTER * 4 // self.denominator

    def __str__(self) -> str:
        return f"{self.numerator}/{self.denominator}"


@dataclass(frozen=True)
class EventSpec:
    tick: int  # absolute tick from the start of the pattern
    note: int
    velocity: int
    duration: int = 30


@dataclass(frozen=True)
class SectionSpec:
    tick: int  # absolute tick from the start of the pattern
    label: str  # e.g. "COUNT-IN", "LOOP START", "BAR 1", "TURNAROUND", "FILL CUE", "RETURN"


def total_ticks(meters: list[Meter], bar_count: int) -> int:
    """Sums bar_ticks over `bar_count` bars, cycling through `meters` (length 1 for a single
    meter, >1 for a mixed-meter bar-by-bar sequence)."""
    return sum(meters[bar % len(meters)].bar_ticks for bar in range(bar_count))


def bar_start_ticks(meters: list[Meter], bar_count: int) -> list[int]:
    """Absolute tick of the start of each bar, cycling through `meters` as in total_ticks."""
    starts: list[int] = []
    tick = 0
    for bar in range(bar_count):
        starts.append(tick)
        tick += meters[bar % len(meters)].bar_ticks
    return starts


@dataclass
class PatternSpec:
    id: str
    display_name: str
    description: str
    area: str  # "educational" | "looper" | "performance"
    family: str  # kebab-case family folder name
    meters: list[Meter]  # one meter per bar of the cycle; a mixed-meter pattern lists >1
    bpm: float
    bar_count: int
    grid_ticks: int
    events: list[EventSpec]
    loop_length_ticks: int
    tags: list[str] = field(default_factory=list)
    count_in_bars: int = 0
    sections: list[SectionSpec] = field(default_factory=list)
    recommended_use: str = ""
    grouping: list[int] | None = None
    swing_ratio: str | None = None
    source_context: str | None = None
    dominant_sounds: list[str] = field(default_factory=list)
    click_language: str = ""  # compact notation, e.g. "H L L L"
    listening_target: str = ""
    loop_cue_behaviour: str = ""
    simplification_note: str = ""

    def __post_init__(self) -> None:
        if not self.recommended_use:
            self.recommended_use = self.description

    @property
    def relative_path(self) -> str:
        return f"{self.area}/{self.family}/{self.id}.mid"

    @property
    def meter_display(self) -> str:
        if len(self.meters) == 1:
            return str(self.meters[0])
        return " -> ".join(str(m) for m in self.meters)

    @property
    def difficulty(self) -> str:
        if "advanced" in self.tags:
            return "Advanced"
        if "challenge" in self.tags:
            return "Challenge"
        if "intermediate" in self.tags:
            return "Intermediate"
        return "Easy"
