"""Looper patterns: count-ins, phrase cues, and sparse groove guides for a solo performer
establishing a clean recording cycle. See PLAN.md - these markers currently have no runtime
effect on tapelooper's own loop engine (which derives loop length purely from the last note's
tick); generated anyway, they're correct, useful in a DAW, and cost nothing.
"""

from __future__ import annotations

from rhythm_library import instruments as instr
from rhythm_library.model import EventSpec, Meter, PatternSpec, SectionSpec, total_ticks
from rhythm_library.patterns.helpers import (
    DOTTED_QUARTER,
    EIGHTH,
    TRIPLET,
    bar_markers,
    clave_events,
    fill_events,
    grouped_pulse_events,
    repeat_events_across_bars,
    swing_pair_events,
)

M44 = Meter(4, 4)
M34 = Meter(3, 4)
M68 = Meter(6, 8)
M54 = Meter(5, 4)
M78 = Meter(7, 8)
M128 = Meter(12, 8)

BPM_DEFAULT = 120.0


def _pattern(
    id_: str,
    display_name: str,
    description: str,
    family: str,
    meters: list[Meter],
    bar_count: int,
    events: list[EventSpec],
    *,
    bpm: float = BPM_DEFAULT,
    tags: list[str] = (),
    sections: list[SectionSpec] = (),
    recommended_use: str = "",
    grouping: list[int] | None = None,
    count_in_bars: int = 0,
    loop_cue_behaviour: str = "",
) -> PatternSpec:
    return PatternSpec(
        id=id_,
        display_name=display_name,
        description=description,
        area="looper",
        family=family,
        meters=meters,
        bpm=bpm,
        bar_count=bar_count,
        grid_ticks=EIGHTH,
        events=sorted(events, key=lambda e: e.tick),
        loop_length_ticks=total_ticks(meters, bar_count),
        tags=list(tags) + ["looper"],
        count_in_bars=count_in_bars,
        sections=list(sections) or bar_markers(bar_count, meters[0].bar_ticks),
        recommended_use=recommended_use,
        grouping=grouping,
        loop_cue_behaviour=loop_cue_behaviour,
    )


PATTERNS: list[PatternSpec] = []

# --- A. Count-ins ------------------------------------------------------------------------


def _count_in_bar(meter: Meter, *, bar_offset: int = 0) -> list[EventSpec]:
    return [EventSpec(bar_offset, instr.WOODBLOCK, instr.VEL_CUE, instr.CUE_DURATION_TICKS)] + grouped_pulse_events(
        meter, [meter.numerator], bar_offset=bar_offset
    )[1:]


def _count_in_pattern(id_: str, name: str, meter: Meter, count_in_bars: int, loop_bars: int = 2, *,
                       last_beat_cue: bool = False) -> PatternSpec:
    events: list[EventSpec] = []
    for bar in range(count_in_bars):
        events += _count_in_bar(meter, bar_offset=bar * meter.bar_ticks)
    if last_beat_cue:
        last_beat_tick = (count_in_bars - 1) * meter.bar_ticks + (meter.numerator - 1) * meter.unit_ticks
        events = [e for e in events if e.tick != last_beat_tick]
        events.append(EventSpec(last_beat_tick, instr.WOODBLOCK, instr.VEL_CUE, instr.CUE_DURATION_TICKS))
    loop_start_tick = count_in_bars * meter.bar_ticks
    for bar in range(loop_bars):
        offset = loop_start_tick + bar * meter.bar_ticks
        events += grouped_pulse_events(meter, [meter.numerator], bar_offset=offset)
    sections = [SectionSpec(0, "COUNT-IN"), SectionSpec(loop_start_tick, "LOOP START")]
    sections += bar_markers(loop_bars, meter.bar_ticks, start_index=1)[1:] if loop_bars > 1 else []
    return _pattern(
        id_,
        name,
        f"{count_in_bars}-bar count-in in {meter} (Woodblock cue on count-in beat 1), then {loop_bars} bars of "
        "the loop body proper, marked LOOP START.",
        "count-ins",
        [meter],
        count_in_bars + loop_bars,
        events,
        tags=["count-in"],
        sections=sections,
        count_in_bars=count_in_bars,
        recommended_use="Cue a solo performer into the downbeat before the loop's recording cycle begins.",
        loop_cue_behaviour=f"LOOP START marker at tick {loop_start_tick} (bar {count_in_bars + 1}).",
    )


PATTERNS.append(_count_in_pattern("one-bar-count-in-4-4", "One-bar count-in 4/4", M44, 1))
PATTERNS.append(_count_in_pattern("two-bar-count-in-4-4", "Two-bar count-in 4/4", M44, 2))
PATTERNS.append(_count_in_pattern("one-bar-count-in-3-4", "One-bar count-in 3/4", M34, 1))
PATTERNS.append(_count_in_pattern("one-bar-count-in-6-8", "One-bar count-in 6/8", M68, 1))

_ci_54 = _count_in_pattern("one-bar-count-in-5-4-three-plus-two", "One-bar count-in 5/4 (3+2)", M54, 1)
_ci_54.events = sorted(
    [EventSpec(0, instr.WOODBLOCK, instr.VEL_CUE, instr.CUE_DURATION_TICKS)]
    + grouped_pulse_events(M54, [3, 2], bar_offset=0)[1:]
    + [e for e in _ci_54.events if e.tick >= M54.bar_ticks],
    key=lambda e: e.tick,
)
_ci_54.grouping = [3, 2]
PATTERNS.append(_ci_54)

_ci_78 = _count_in_pattern("one-bar-count-in-7-8-two-two-three", "One-bar count-in 7/8 (2+2+3)", M78, 1)
_ci_78.events = sorted(
    [EventSpec(0, instr.WOODBLOCK, instr.VEL_CUE, instr.CUE_DURATION_TICKS)]
    + grouped_pulse_events(M78, [2, 2, 3], bar_offset=0)[1:]
    + [e for e in _ci_78.events if e.tick >= M78.bar_ticks],
    key=lambda e: e.tick,
)
_ci_78.grouping = [2, 2, 3]
PATTERNS.append(_ci_78)

PATTERNS.append(
    _count_in_pattern(
        "two-bar-count-in-with-last-beat-cue", "Two-bar count-in with last-beat cue", M44, 2, last_beat_cue=True
    )
)

# --- B. Phrase cues ------------------------------------------------------------------------


def _four_bar_loop(id_, name, description, events_per_bar, *, meter: Meter = M44, bar_count: int = 4,
                    tags: list[str] = (), grouping=None) -> PatternSpec:
    events: list[EventSpec] = []
    for bar in range(bar_count):
        offset = bar * meter.bar_ticks
        bar_events = events_per_bar(bar)
        events += [EventSpec(offset + e.tick, e.note, e.velocity, e.duration) for e in bar_events]
    return _pattern(
        id_,
        name,
        description,
        "phrase-cues",
        [meter],
        bar_count,
        events,
        tags=list(tags),
        grouping=grouping,
        recommended_use="Recognise the top of the loop and its internal phrase structure while recording.",
        loop_cue_behaviour="Bar 1 carries the strongest cue; the pattern repeats identically every cycle.",
    )


def _bar1_strong(bar: int) -> list[EventSpec]:
    accent = instr.VEL_CUE if bar == 0 else instr.VEL_ACCENT
    return grouped_pulse_events(M44, [4], accent_vel=accent)


PATTERNS.append(
    _four_bar_loop(
        "four-bar-loop-standard",
        "Four-bar loop standard",
        "Bar 1 carries a stronger cue than bars 2-4, so the top of the loop is always audible.",
        _bar1_strong,
        tags=["easy"],
    )
)

PATTERNS.append(
    _four_bar_loop(
        "four-bar-loop-with-turnaround-cue",
        "Four-bar loop with turnaround cue",
        "Standard four-bar loop with a small subdivision lift in the second half of bar 4, cueing the return to bar 1.",
        lambda bar: grouped_pulse_events(M44, [4], accent_vel=instr.VEL_CUE if bar == 0 else instr.VEL_ACCENT)
        + (fill_events([2 * M44.unit_ticks + EIGHTH, 3 * M44.unit_ticks + EIGHTH], instr.HIHAT_CLOSED, instr.VEL_LIGHT) if bar == 3 else []),
        tags=["easy"],
    )
)

PATTERNS.append(
    _four_bar_loop(
        "eight-bar-loop-with-halfway-cue",
        "Eight-bar loop with halfway cue",
        "Bar 1 opens the loop; bar 5 gets a secondary cue marking the halfway point of an eight-bar phrase.",
        lambda bar: grouped_pulse_events(
            M44, [4], accent_vel=instr.VEL_CUE if bar == 0 else (instr.VEL_SECONDARY if bar == 4 else instr.VEL_ACCENT)
        ),
        bar_count=8,
        tags=["intermediate"],
    )
)

PATTERNS.append(
    _four_bar_loop(
        "twelve-bar-loop-with-form-markers",
        "Twelve-bar loop with form markers",
        "A twelve-bar cycle (e.g. a blues form) with a cue at bar 1, bar 5, and bar 9 marking each four-bar segment.",
        lambda bar: grouped_pulse_events(
            M44, [4], accent_vel=instr.VEL_CUE if bar % 4 == 0 else instr.VEL_ACCENT
        ),
        bar_count=12,
        tags=["intermediate"],
    )
)

PATTERNS.append(
    _four_bar_loop(
        "16-bar-loop-with-4-bar-phrase-markers",
        "16-bar loop with 4-bar phrase markers",
        "A 16-bar cycle with a cue at the start of each of its four 4-bar phrases.",
        lambda bar: grouped_pulse_events(M44, [4], accent_vel=instr.VEL_CUE if bar % 4 == 0 else instr.VEL_ACCENT),
        bar_count=16,
        tags=["intermediate"],
    )
)

PATTERNS.append(
    _four_bar_loop(
        "four-bar-loop-backbeat-guide",
        "Four-bar loop backbeat guide",
        "Four-bar loop with a full backbeat (accent on 2 and 4) instead of a plain bar marker.",
        lambda bar: [
            EventSpec(0, instr.SNARE, instr.VEL_CUE if bar == 0 else instr.VEL_NORMAL, instr.CLICK_DURATION_TICKS),
            EventSpec(M44.unit_ticks, instr.SNARE, instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS),
            EventSpec(2 * M44.unit_ticks, instr.SNARE, instr.VEL_NORMAL, instr.CLICK_DURATION_TICKS),
            EventSpec(3 * M44.unit_ticks, instr.SNARE, instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS),
        ],
        tags=["easy"],
    )
)

PATTERNS.append(
    _four_bar_loop(
        "four-bar-loop-jazz-two-and-four",
        "Four-bar loop jazz two and four",
        "Four-bar loop, click only on beats 2 and 4, bar 1 slightly stronger to mark the loop start.",
        lambda bar: [
            EventSpec(M44.unit_ticks, instr.SNARE, instr.VEL_CUE if bar == 0 else instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS),
            EventSpec(3 * M44.unit_ticks, instr.SNARE, instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS),
        ],
        tags=["intermediate", "jazz"],
    )
)

PATTERNS.append(
    _four_bar_loop(
        "four-bar-loop-one-click-per-bar",
        "Four-bar loop one click per bar",
        "Only one click per bar, on beat 1 - the sparsest usable loop-boundary reference.",
        lambda bar: [EventSpec(0, instr.SNARE, instr.VEL_CUE if bar == 0 else instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS)],
        tags=["intermediate"],
    )
)

PATTERNS.append(
    _four_bar_loop(
        "four-bar-loop-six-eight",
        "Four-bar loop six-eight",
        "Four-bar loop in 6/8: two dotted-quarter pulses per bar, bar 1 cued more strongly.",
        lambda bar: grouped_pulse_events(M68, [2], unit_ticks=DOTTED_QUARTER, accent_vel=instr.VEL_CUE if bar == 0 else instr.VEL_ACCENT),
        meter=M68,
        tags=["easy"],
    )
)

PATTERNS.append(
    _four_bar_loop(
        "four-bar-loop-five-four-three-plus-two",
        "Four-bar loop five-four (3+2)",
        "Four-bar loop in 5/4, grouped 3+2, bar 1 cued more strongly.",
        lambda bar: grouped_pulse_events(M54, [3, 2], accent_vel=instr.VEL_CUE if bar == 0 else instr.VEL_ACCENT),
        meter=M54,
        tags=["intermediate", "odd-meter"],
        grouping=[3, 2],
    )
)

PATTERNS.append(
    _four_bar_loop(
        "four-bar-loop-seven-eight-two-two-three",
        "Four-bar loop seven-eight (2+2+3)",
        "Four-bar loop in 7/8, grouped 2+2+3, bar 1 cued more strongly.",
        lambda bar: grouped_pulse_events(M78, [2, 2, 3], accent_vel=instr.VEL_CUE if bar == 0 else instr.VEL_ACCENT),
        meter=M78,
        tags=["intermediate", "odd-meter"],
        grouping=[2, 2, 3],
    )
)

# --- C. Groove guides ----------------------------------------------------------------------

_ROCK = [
    EventSpec(0, instr.KICK, instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS),
    EventSpec(M44.unit_ticks, instr.SNARE, instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS),
    EventSpec(2 * M44.unit_ticks, instr.KICK, instr.VEL_NORMAL, instr.CLICK_DURATION_TICKS),
    EventSpec(3 * M44.unit_ticks, instr.SNARE, instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS),
]
PATTERNS.append(
    _pattern(
        "sparse-rock-guide-4-4",
        "Sparse rock guide 4/4",
        "A sparse two-voice rock guide: kick on 1 and 3, snare backbeat on 2 and 4.",
        "groove-guides",
        [M44],
        4,
        repeat_events_across_bars(_ROCK, M44.bar_ticks, 4),
        tags=["easy", "groove-guide"],
        recommended_use="A light rock feel to record or monitor over without a full drum groove.",
    )
)

_FUNK = [
    EventSpec(0, instr.KICK, instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS),
    EventSpec(M44.unit_ticks, instr.SNARE, instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS),
    EventSpec(2 * M44.unit_ticks + EIGHTH, instr.KICK, instr.VEL_NORMAL, instr.CLICK_DURATION_TICKS),
    EventSpec(3 * M44.unit_ticks, instr.SNARE, instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS),
]
PATTERNS.append(
    _pattern(
        "sparse-funk-guide-4-4",
        "Sparse funk guide 4/4",
        "A sparse two-voice funk guide: an off-the-beat kick anchor (and-of-3) plus the 2-and-4 backbeat.",
        "groove-guides",
        [M44],
        4,
        repeat_events_across_bars(_FUNK, M44.bar_ticks, 4),
        tags=["intermediate", "groove-guide"],
    )
)

_JAZZ_GUIDE = [
    EventSpec(0, instr.RIDE, instr.VEL_NORMAL, instr.CLICK_DURATION_TICKS),
    EventSpec(2 * TRIPLET, instr.RIDE, instr.VEL_LIGHT, instr.CLICK_DURATION_TICKS),
    EventSpec(M44.unit_ticks, instr.RIDE, instr.VEL_NORMAL, instr.CLICK_DURATION_TICKS),
    EventSpec(M44.unit_ticks + 2 * TRIPLET, instr.RIDE, instr.VEL_LIGHT, instr.CLICK_DURATION_TICKS),
    EventSpec(2 * M44.unit_ticks, instr.RIDE, instr.VEL_NORMAL, instr.CLICK_DURATION_TICKS),
    EventSpec(2 * M44.unit_ticks + 2 * TRIPLET, instr.RIDE, instr.VEL_LIGHT, instr.CLICK_DURATION_TICKS),
    EventSpec(3 * M44.unit_ticks, instr.RIDE, instr.VEL_NORMAL, instr.CLICK_DURATION_TICKS),
    EventSpec(3 * M44.unit_ticks + 2 * TRIPLET, instr.RIDE, instr.VEL_LIGHT, instr.CLICK_DURATION_TICKS),
]
PATTERNS.append(
    _pattern(
        "sparse-jazz-guide-4-4",
        "Sparse jazz guide 4/4",
        "A ride-only swing-shuffle guide (spang-a-lang) with no backbeat.",
        "groove-guides",
        [M44],
        4,
        repeat_events_across_bars(_JAZZ_GUIDE, M44.bar_ticks, 4),
        tags=["intermediate", "groove-guide", "jazz"],
    )
)

PATTERNS.append(
    _pattern(
        "sparse-shuffle-guide-4-4",
        "Sparse shuffle guide 4/4",
        "A sparse shuffle guide: kick on 1 and 3, snare on 2 and 4, with the shuffle's swung eighth ghosted on hi-hat.",
        "groove-guides",
        [M44],
        4,
        repeat_events_across_bars(
            _ROCK + fill_events([b * M44.unit_ticks + 2 * TRIPLET for b in range(4)], instr.HIHAT_CLOSED, instr.VEL_GHOST),
            M44.bar_ticks,
            4,
        ),
        tags=["intermediate", "groove-guide", "swing-shuffle"],
    )
)

PATTERNS.append(
    _pattern(
        "sparse-bossa-guide",
        "Sparse bossa guide",
        "The two-bar son-clave shape on Sidestick, standing in for a full bossa nova groove.",
        "groove-guides",
        [M44],
        2,
        clave_events("son", "3-2", note=instr.SIDESTICK),
        tags=["intermediate", "groove-guide"],
    )
)

PATTERNS.append(
    _pattern(
        "sparse-clave-guide-3-2",
        "Sparse clave guide 3-2",
        "The son clave, 3-2 direction, as a standalone loopable groove guide.",
        "groove-guides",
        [M44],
        2,
        clave_events("son", "3-2"),
        tags=["intermediate", "groove-guide", "clave-3-2"],
    )
)

PATTERNS.append(
    _pattern(
        "sparse-clave-guide-2-3",
        "Sparse clave guide 2-3",
        "The son clave, 2-3 direction, as a standalone loopable groove guide.",
        "groove-guides",
        [M44],
        2,
        clave_events("son", "2-3"),
        tags=["intermediate", "groove-guide", "clave-2-3"],
    )
)

PATTERNS.append(
    _pattern(
        "sparse-6-8-guide",
        "Sparse 6/8 guide",
        "A sparse 6/8 groove guide: kick on the two dotted-quarter pulses, snare accent on the second.",
        "groove-guides",
        [M68],
        4,
        repeat_events_across_bars(
            [
                EventSpec(0, instr.KICK, instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS),
                EventSpec(720, instr.SNARE, instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS),
            ],
            M68.bar_ticks,
            4,
        ),
        tags=["easy", "groove-guide"],
    )
)

PATTERNS.append(
    _pattern(
        "sparse-12-8-guide",
        "Sparse 12/8 guide",
        "A sparse 12/8 groove guide: kick on the downbeat, snare on the third dotted-quarter pulse.",
        "groove-guides",
        [M128],
        4,
        repeat_events_across_bars(
            [
                EventSpec(0, instr.KICK, instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS),
                EventSpec(2 * 3 * EIGHTH, instr.SNARE, instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS),
            ],
            M128.bar_ticks,
            4,
        ),
        tags=["easy", "groove-guide"],
    )
)


LOOPER_PATTERNS: list[PatternSpec] = PATTERNS
