"""Shared event-building helpers for every pattern family. Every function returns absolute-tick
EventSpec/SectionSpec lists for one bar (callers offset by bar_index * meter.bar_ticks) or for a
whole multi-bar cycle where the musical idea genuinely spans bars (clave, silence gaps).
"""

from __future__ import annotations

from rhythm_library import instruments as instr
from rhythm_library.model import EventSpec, Meter, SectionSpec

EIGHTH = 240
SIXTEENTH = 120
TRIPLET = 160
DOTTED_QUARTER = 720


def grouped_pulse_events(
    meter: Meter,
    group_sizes: list[int],
    *,
    accent_note: int = instr.SNARE,
    accent_vel: int = instr.VEL_ACCENT,
    normal_note: int = instr.SNARE,
    normal_vel: int = instr.VEL_NORMAL,
    bar_offset: int = 0,
    duration: int = instr.CLICK_DURATION_TICKS,
    unit_ticks: int | None = None,
) -> list[EventSpec]:
    """One event per counting unit (`unit_ticks` apart, default meter.unit_ticks - override to
    DOTTED_QUARTER for a compound meter's sparse main-pulse patterns); the first unit of each
    group in `group_sizes` is accented, marking additive-grouping boundaries. `group_sizes ==
    [N]` is an ordinary simple-meter pulse train with N units, accent on the downbeat only."""
    step = unit_ticks if unit_ticks is not None else meter.unit_ticks
    events: list[EventSpec] = []
    unit_index = 0
    for group_size in group_sizes:
        for offset_in_group in range(group_size):
            accent = offset_in_group == 0
            tick = bar_offset + unit_index * step
            events.append(
                EventSpec(
                    tick,
                    accent_note if accent else normal_note,
                    accent_vel if accent else normal_vel,
                    duration,
                )
            )
            unit_index += 1
    return events


def fill_events(ticks: list[int], note: int, velocity: int, *, bar_offset: int = 0,
                 duration: int = instr.CLICK_DURATION_TICKS) -> list[EventSpec]:
    """Extra hits at explicit tick offsets within a bar - subdivisions, ghost notes, cues."""
    return [EventSpec(bar_offset + tick, note, velocity, duration) for tick in ticks]


def swing_pair_events(meter: Meter, long_percent: int, *, note: int = instr.SNARE,
                       velocity: int = instr.VEL_GHOST, bar_offset: int = 0,
                       duration: int = instr.CLICK_DURATION_TICKS) -> list[EventSpec]:
    """The off-beat half of a swung eighth pair for every beat in `meter` - the on-beat eighth
    occupies `long_percent` of the beat, so the off-beat lands at that offset (an actual tick
    position, never just a filename label)."""
    offset = round(meter.unit_ticks * long_percent / 100)
    beats = meter.numerator if meter.denominator == 4 else meter.numerator // 2
    return [
        EventSpec(bar_offset + beat * meter.unit_ticks + offset, note, velocity, duration)
        for beat in range(beats)
    ]


def repeat_events_across_bars(one_bar_events: list[EventSpec], bar_ticks: int, bar_count: int) -> list[EventSpec]:
    """Repeats a single bar's events for every bar in the file - the common case for a pattern
    whose musical idea is one bar long but whose file should actually demonstrate looping."""
    events: list[EventSpec] = []
    for bar in range(bar_count):
        offset = bar * bar_ticks
        events += [EventSpec(offset + e.tick, e.note, e.velocity, e.duration) for e in one_bar_events]
    return events


def section(tick: int, label: str, *, bar_offset: int = 0) -> SectionSpec:
    return SectionSpec(bar_offset + tick, label)


def bar_markers(bar_count: int, bar_ticks: int, *, start_index: int = 1) -> list[SectionSpec]:
    return [SectionSpec(bar * bar_ticks, f"BAR {bar + start_index}") for bar in range(bar_count)]


# --- Clave rhythms (see PLAN.md: son and rumba differ only in the 3-side's final note) ---


def clave_3_side_ticks(variant: str) -> list[int]:
    """Tresillo-shaped 3-side of a 4/4 clave: hits on beat 1, the "and" of beat 2, and (son)
    beat 4 or (rumba) the "e" of beat 4 - the one note that distinguishes the two claves."""
    if variant == "rumba":
        return [0, 720, 1560]
    return [0, 720, 1440]  # son


def clave_2_side_ticks() -> list[int]:
    return [480, 960]  # beat 2, beat 3 - identical for son and rumba


def clave_events(variant: str, direction: str, *, note: int = instr.WOODBLOCK,
                  velocity: int = instr.VEL_ACCENT, duration: int = instr.CLICK_DURATION_TICKS) -> list[EventSpec]:
    """Two-bar son/rumba clave. `direction` is "3-2" (3-side first) or "2-3" (2-side first)."""
    bar_ticks = Meter(4, 4).bar_ticks
    three_side = clave_3_side_ticks(variant)
    two_side = clave_2_side_ticks()
    first, second = (three_side, two_side) if direction == "3-2" else (two_side, three_side)
    events = [EventSpec(tick, note, velocity, duration) for tick in first]
    events += [EventSpec(bar_ticks + tick, note, velocity, duration) for tick in second]
    return events
