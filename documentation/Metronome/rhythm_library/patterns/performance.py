"""Performance patterns: longer running guide tracks for rehearsal, live use, or accompaniment.
Central design rule (PLAN.md): bar 1 carries a clear loop-start cue; the final bar carries a
small, sparse turnaround (increased subdivision density or a compact pickup gesture, never a
full drummer fill) that leads seamlessly back to bar 1.
"""

from __future__ import annotations

from rhythm_library import instruments as instr
from rhythm_library.model import EventSpec, Meter, PatternSpec, SectionSpec, bar_start_ticks, total_ticks
from rhythm_library.patterns.helpers import (
    DOTTED_QUARTER,
    EIGHTH,
    SIXTEENTH,
    TRIPLET,
    bar_markers,
    clave_events,
    fill_events,
    grouped_pulse_events,
    swing_pair_events,
)

M44 = Meter(4, 4)
M68 = Meter(6, 8)
M98 = Meter(9, 8)
M128 = Meter(12, 8)
M54 = Meter(5, 4)
M78 = Meter(7, 8)
M118 = Meter(11, 8)
M34 = Meter(3, 4)
M58 = Meter(5, 8)
M24 = Meter(2, 4)
M108 = Meter(10, 8)

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
    swing_ratio: str | None = None,
    count_in_bars: int = 0,
    loop_cue_behaviour: str = "",
    source_context: str = "",
    simplification_note: str = "",
) -> PatternSpec:
    return PatternSpec(
        id=id_,
        display_name=display_name,
        description=description,
        area="performance",
        family=family,
        meters=meters,
        bpm=bpm,
        bar_count=bar_count,
        grid_ticks=EIGHTH,
        events=sorted(events, key=lambda e: e.tick),
        loop_length_ticks=total_ticks(meters, bar_count),
        tags=list(tags) + ["performance"],
        count_in_bars=count_in_bars,
        sections=list(sections) or (
            bar_markers(bar_count, meters[0].bar_ticks)[:-1] + [SectionSpec((bar_count - 1) * meters[0].bar_ticks, "TURNAROUND")]
        ),
        recommended_use=recommended_use,
        grouping=grouping,
        swing_ratio=swing_ratio,
        loop_cue_behaviour=loop_cue_behaviour or "Bar 1 carries the loop-start cue; the final bar carries a sparse turnaround leading back to bar 1.",
        source_context=source_context,
        simplification_note=simplification_note,
    )


def _loop(meter: Meter, bar_count: int, per_bar) -> list[EventSpec]:
    events: list[EventSpec] = []
    for bar in range(bar_count):
        offset = bar * meter.bar_ticks
        events += [EventSpec(offset + e.tick, e.note, e.velocity, e.duration) for e in per_bar(bar)]
    return events


PATTERNS: list[PatternSpec] = []

# --- A. Straight -----------------------------------------------------------------------


def _straight_bar(bar: int, last: int, *, turnaround: bool = True) -> list[EventSpec]:
    base = grouped_pulse_events(M44, [4], accent_vel=instr.VEL_CUE if bar == 0 else instr.VEL_ACCENT)
    if turnaround and bar == last:
        base += fill_events([3 * M44.unit_ticks + EIGHTH], instr.HIHAT_CLOSED, instr.VEL_LIGHT)
    return base


PATTERNS.append(
    _pattern(
        "four-bar-straight-bar-marker",
        "Four-bar straight bar marker",
        "Four-bar straight loop: bar 1 cued, bars 2-3 plain, bar 4 carries a light turnaround into bar 1.",
        "straight",
        [M44],
        4,
        _loop(M44, 4, lambda bar: _straight_bar(bar, 3)),
        tags=["easy"],
    )
)

PATTERNS.append(
    _pattern(
        "four-bar-straight-backbeat-guide",
        "Four-bar straight backbeat guide",
        "Four-bar loop with a full backbeat (accent on 2 and 4); bar 4 adds a light turnaround.",
        "straight",
        [M44],
        4,
        _loop(
            M44,
            4,
            lambda bar: [
                EventSpec(0, instr.SNARE, instr.VEL_CUE if bar == 0 else instr.VEL_NORMAL, instr.CLICK_DURATION_TICKS),
                EventSpec(M44.unit_ticks, instr.SNARE, instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS),
                EventSpec(2 * M44.unit_ticks, instr.SNARE, instr.VEL_NORMAL, instr.CLICK_DURATION_TICKS),
                EventSpec(3 * M44.unit_ticks, instr.SNARE, instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS),
            ]
            + (fill_events([3 * M44.unit_ticks + EIGHTH], instr.HIHAT_CLOSED, instr.VEL_LIGHT) if bar == 3 else []),
        ),
        tags=["easy"],
    )
)

PATTERNS.append(
    _pattern(
        "four-bar-straight-one-and-three-guide",
        "Four-bar straight one-and-three guide",
        "Four-bar loop, click on beats 1 and 3 only; bar 4 adds a light turnaround.",
        "straight",
        [M44],
        4,
        _loop(
            M44,
            4,
            lambda bar: [
                EventSpec(0, instr.SNARE, instr.VEL_CUE if bar == 0 else instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS),
                EventSpec(2 * M44.unit_ticks, instr.SNARE, instr.VEL_NORMAL, instr.CLICK_DURATION_TICKS),
            ]
            + (fill_events([3 * M44.unit_ticks + EIGHTH], instr.HIHAT_CLOSED, instr.VEL_LIGHT) if bar == 3 else []),
        ),
        tags=["easy"],
    )
)

_ROCK_BAR = [
    EventSpec(0, instr.KICK, instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS),
    EventSpec(M44.unit_ticks, instr.SNARE, instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS),
    EventSpec(2 * M44.unit_ticks, instr.KICK, instr.VEL_NORMAL, instr.CLICK_DURATION_TICKS),
    EventSpec(3 * M44.unit_ticks, instr.SNARE, instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS),
]
PATTERNS.append(
    _pattern(
        "four-bar-straight-sparse-rock-guide",
        "Four-bar straight sparse rock guide",
        "A sparse two-voice rock guide over four bars; bar 4 adds a light kick pickup leading to bar 1.",
        "straight",
        [M44],
        4,
        _loop(
            M44,
            4,
            lambda bar: _ROCK_BAR + (fill_events([3 * M44.unit_ticks + EIGHTH], instr.KICK, instr.VEL_LIGHT) if bar == 3 else []),
        ),
        tags=["easy"],
    )
)

_FUNK_BAR = [
    EventSpec(0, instr.KICK, instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS),
    EventSpec(M44.unit_ticks, instr.SNARE, instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS),
    EventSpec(2 * M44.unit_ticks + EIGHTH, instr.KICK, instr.VEL_NORMAL, instr.CLICK_DURATION_TICKS),
    EventSpec(3 * M44.unit_ticks, instr.SNARE, instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS),
]
PATTERNS.append(
    _pattern(
        "four-bar-straight-sparse-funk-guide",
        "Four-bar straight sparse funk guide",
        "A sparse funk guide over four bars; bar 4 adds a light 16th-note kick pickup leading to bar 1.",
        "straight",
        [M44],
        4,
        _loop(
            M44,
            4,
            lambda bar: _FUNK_BAR + (fill_events([3 * M44.unit_ticks + 3 * SIXTEENTH], instr.KICK, instr.VEL_LIGHT) if bar == 3 else []),
        ),
        tags=["intermediate"],
    )
)

PATTERNS.append(
    _pattern(
        "eight-bar-straight-phrase-guide",
        "Eight-bar straight phrase guide",
        "An eight-bar straight loop with a halfway cue at bar 5 and a turnaround in bar 8.",
        "straight",
        [M44],
        8,
        _loop(M44, 8, lambda bar: _straight_bar(bar, 7) if bar not in (4,) else grouped_pulse_events(M44, [4], accent_vel=instr.VEL_SECONDARY)),
        tags=["intermediate"],
    )
)

PATTERNS.append(
    _pattern(
        "four-bar-straight-with-turnaround",
        "Four-bar straight with turnaround",
        "Four-bar loop with an explicit, denser turnaround across the second half of bar 4 (still sparse, not a full fill).",
        "straight",
        [M44],
        4,
        _loop(
            M44,
            4,
            lambda bar: grouped_pulse_events(M44, [4], accent_vel=instr.VEL_CUE if bar == 0 else instr.VEL_ACCENT)
            + (fill_events([2 * M44.unit_ticks + EIGHTH, 3 * M44.unit_ticks + EIGHTH, 3 * M44.unit_ticks + 3 * SIXTEENTH], instr.HIHAT_CLOSED, instr.VEL_LIGHT) if bar == 3 else []),
        ),
        tags=["easy"],
        sections=bar_markers(4, M44.bar_ticks)[:-1] + [SectionSpec(3 * M44.bar_ticks, "FILL CUE"), SectionSpec(4 * M44.bar_ticks, "RETURN")],
    )
)

PATTERNS.append(
    _pattern(
        "four-bar-straight-with-last-bar-subdivision-lift",
        "Four-bar straight with last-bar subdivision lift",
        "Four-bar loop where all of bar 4 (not just its second half) steps up to eighth-note subdivision.",
        "straight",
        [M44],
        4,
        _loop(
            M44,
            4,
            lambda bar: grouped_pulse_events(M44, [4], accent_vel=instr.VEL_CUE if bar == 0 else instr.VEL_ACCENT)
            + (fill_events([b * M44.unit_ticks + EIGHTH for b in range(4)], instr.HIHAT_CLOSED, instr.VEL_LIGHT) if bar == 3 else []),
        ),
        tags=["easy"],
    )
)

PATTERNS.append(
    _pattern(
        "four-bar-straight-with-pickup-cue",
        "Four-bar straight with pickup cue",
        "Four-bar loop with a single Woodblock pickup note on the last eighth of bar 4, a compact gesture rather than a density increase.",
        "straight",
        [M44],
        4,
        _loop(
            M44,
            4,
            lambda bar: grouped_pulse_events(M44, [4], accent_vel=instr.VEL_CUE if bar == 0 else instr.VEL_ACCENT)
            + (fill_events([3 * M44.unit_ticks + EIGHTH], instr.WOODBLOCK, instr.VEL_CUE) if bar == 3 else []),
        ),
        tags=["easy"],
    )
)

_count_in_bar_events = list(grouped_pulse_events(M44, [4], bar_offset=0))
_count_in_bar_events[0] = EventSpec(0, instr.WOODBLOCK, instr.VEL_CUE, instr.CUE_DURATION_TICKS)
_count_in_and_loop_events = list(_count_in_bar_events)
for _bar in range(4):
    _bar_events = _straight_bar(_bar, 3, turnaround=(_bar == 3))
    _count_in_and_loop_events += [
        EventSpec((_bar + 1) * M44.bar_ticks + e.tick, e.note, e.velocity, e.duration) for e in _bar_events
    ]
PATTERNS.append(
    _pattern(
        "four-bar-count-in-and-loop",
        "Four-bar count-in and loop",
        "A one-bar count-in (Woodblock cue on beat 1) followed by a four-bar straight loop with a turnaround in its last bar.",
        "straight",
        [M44],
        5,
        _count_in_and_loop_events,
        tags=["easy"],
        count_in_bars=1,
        sections=[SectionSpec(0, "COUNT-IN"), SectionSpec(M44.bar_ticks, "LOOP START")] + bar_markers(3, M44.bar_ticks, start_index=2) + [SectionSpec(4 * M44.bar_ticks, "TURNAROUND")],
    )
)

# --- B. Swing and shuffle ----------------------------------------------------------------

PATTERNS.append(
    _pattern(
        "four-bar-jazz-two-and-four",
        "Four-bar jazz two and four",
        "Four-bar loop, click only on beats 2 and 4; bar 4 adds a light ride pickup on the 'and' of 4.",
        "swing-shuffle",
        [M44],
        4,
        _loop(
            M44,
            4,
            lambda bar: [
                EventSpec(M44.unit_ticks, instr.SNARE, instr.VEL_CUE if bar == 0 else instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS),
                EventSpec(3 * M44.unit_ticks, instr.SNARE, instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS),
            ]
            + (fill_events([3 * M44.unit_ticks + EIGHTH], instr.RIDE, instr.VEL_LIGHT) if bar == 3 else []),
        ),
        tags=["intermediate", "jazz"],
    )
)

_RIDE_BAR = [
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
        "four-bar-jazz-ride-like-guide",
        "Four-bar jazz ride-like guide",
        "A ride-only spang-a-lang guide over four bars, no backbeat.",
        "swing-shuffle",
        [M44],
        4,
        _loop(M44, 4, lambda bar: _RIDE_BAR),
        tags=["intermediate", "jazz"],
    )
)

PATTERNS.append(
    _pattern(
        "four-bar-shuffle-guide",
        "Four-bar shuffle guide",
        "Kick/snare shuffle guide with the swung triplet partial ghosted on hi-hat, four bars.",
        "swing-shuffle",
        [M44],
        4,
        _loop(
            M44,
            4,
            lambda bar: _ROCK_BAR + fill_events([b * M44.unit_ticks + 2 * TRIPLET for b in range(4)], instr.HIHAT_CLOSED, instr.VEL_GHOST),
        ),
        tags=["intermediate", "swing-shuffle"],
        swing_ratio="2:1",
    )
)

for _id, _name, _percent in (("four-bar-swing-light", "Four-bar swing light", 55), ("four-bar-swing-medium", "Four-bar swing medium", 60)):
    PATTERNS.append(
        _pattern(
            _id,
            _name,
            f"Quarter-note bar marker with a {_percent}:{100 - _percent} swung eighth-note guide, four bars.",
            "swing-shuffle",
            [M44],
            4,
            _loop(
                M44,
                4,
                lambda bar, p=_percent: grouped_pulse_events(M44, [4], accent_vel=instr.VEL_CUE if bar == 0 else instr.VEL_ACCENT) + swing_pair_events(M44, p),
            ),
            tags=["intermediate", "swing-shuffle"],
            swing_ratio=f"{_percent}:{100 - _percent}",
        )
    )

PATTERNS.append(
    _pattern(
        "four-bar-swing-with-turnaround",
        "Four-bar swing with turnaround",
        "60:40 swing guide over four bars, with a denser swung turnaround in bar 4.",
        "swing-shuffle",
        [M44],
        4,
        _loop(
            M44,
            4,
            lambda bar: grouped_pulse_events(M44, [4], accent_vel=instr.VEL_CUE if bar == 0 else instr.VEL_ACCENT)
            + swing_pair_events(M44, 60)
            + (fill_events([3 * M44.unit_ticks + round(M44.unit_ticks * 0.6)], instr.RIDE, instr.VEL_LIGHT) if bar == 3 else []),
        ),
        tags=["intermediate", "swing-shuffle"],
        swing_ratio="60:40",
    )
)

PATTERNS.append(
    _pattern(
        "eight-bar-jazz-phrase-guide",
        "Eight-bar jazz phrase guide",
        "Ride-only spang-a-lang guide over eight bars, with a light backbeat accent added in the second half.",
        "swing-shuffle",
        [M44],
        8,
        _loop(M44, 8, lambda bar: _RIDE_BAR + ([EventSpec(M44.unit_ticks, instr.SNARE, instr.VEL_LIGHT, instr.CLICK_DURATION_TICKS)] if bar >= 4 else [])),
        tags=["intermediate", "jazz"],
    )
)

# --- C. Compound and odd meter -------------------------------------------------------------

PATTERNS.append(
    _pattern(
        "four-bar-six-eight-guide",
        "Four-bar six-eight guide",
        "Two dotted-quarter pulses per bar over four bars, bar 1 cued, bar 4 lightly filled.",
        "compound-meter",
        [M68],
        4,
        _loop(
            M68,
            4,
            lambda bar: grouped_pulse_events(M68, [2], unit_ticks=DOTTED_QUARTER, accent_vel=instr.VEL_CUE if bar == 0 else instr.VEL_ACCENT)
            + (fill_events([720 + EIGHTH], instr.HIHAT_CLOSED, instr.VEL_LIGHT) if bar == 3 else []),
        ),
        tags=["easy"],
    )
)

PATTERNS.append(
    _pattern(
        "four-bar-twelve-eight-blues-guide",
        "Four-bar twelve-eight blues guide",
        "The 12/8 blues shuffle feel (dotted-quarter pulses, middle eighth dropped) over four bars.",
        "compound-meter",
        [M128],
        4,
        _loop(
            M128,
            4,
            lambda bar: grouped_pulse_events(M128, [4], unit_ticks=DOTTED_QUARTER, accent_vel=instr.VEL_CUE if bar == 0 else instr.VEL_ACCENT)
            + fill_events([g * 3 * EIGHTH + 2 * EIGHTH for g in range(4)], instr.HIHAT_CLOSED, instr.VEL_GHOST),
        ),
        tags=["intermediate"],
    )
)

for _id, _name, _grouping in (
    ("four-bar-five-four-three-plus-two", "Four-bar five-four (3+2)", [3, 2]),
    ("four-bar-five-four-two-plus-three", "Four-bar five-four (2+3)", [2, 3]),
):
    PATTERNS.append(
        _pattern(
            _id,
            _name,
            f"5/4 grouped {'+'.join(map(str, _grouping))}, four bars, bar 1 cued, bar 4 lightly filled.",
            "odd-meter",
            [M54],
            4,
            _loop(
                M54,
                4,
                lambda bar, g=_grouping: grouped_pulse_events(M54, g, accent_vel=instr.VEL_CUE if bar == 0 else instr.VEL_ACCENT)
                + (fill_events([4 * M54.unit_ticks + EIGHTH], instr.HIHAT_CLOSED, instr.VEL_LIGHT) if bar == 3 else []),
            ),
            tags=["intermediate", "odd-meter"],
            grouping=_grouping,
        )
    )

for _id, _name, _grouping in (
    ("four-bar-seven-eight-two-two-three", "Four-bar seven-eight (2+2+3)", [2, 2, 3]),
    ("four-bar-seven-eight-two-three-two", "Four-bar seven-eight (2+3+2)", [2, 3, 2]),
    ("four-bar-seven-eight-three-two-two", "Four-bar seven-eight (3+2+2)", [3, 2, 2]),
):
    PATTERNS.append(
        _pattern(
            _id,
            _name,
            f"7/8 grouped {'+'.join(map(str, _grouping))}, four bars, bar 1 cued, bar 4 lightly filled.",
            "odd-meter",
            [M78],
            4,
            _loop(
                M78,
                4,
                lambda bar, g=_grouping: grouped_pulse_events(M78, g, accent_vel=instr.VEL_CUE if bar == 0 else instr.VEL_ACCENT)
                + (fill_events([6 * EIGHTH + EIGHTH // 2], instr.HIHAT_CLOSED, instr.VEL_LIGHT) if bar == 3 else []),
            ),
            tags=["intermediate", "odd-meter"],
            grouping=_grouping,
        )
    )

PATTERNS.append(
    _pattern(
        "four-bar-nine-eight-aksak-grouping",
        "Four-bar nine-eight aksak grouping",
        "9/8 grouped 2+2+2+3 (aksak), four bars, bar 1 cued, bar 4 lightly filled.",
        "odd-meter",
        [M98],
        4,
        _loop(
            M98,
            4,
            lambda bar: grouped_pulse_events(M98, [2, 2, 2, 3], accent_vel=instr.VEL_CUE if bar == 0 else instr.VEL_ACCENT)
            + (fill_events([8 * EIGHTH], instr.HIHAT_CLOSED, instr.VEL_LIGHT) if bar == 3 else []),
        ),
        tags=["intermediate", "odd-meter"],
        grouping=[2, 2, 2, 3],
    )
)

PATTERNS.append(
    _pattern(
        "four-bar-eleven-eight-guide",
        "Four-bar eleven-eight guide",
        "11/8 grouped 3+3+3+2, four bars, bar 1 cued, bar 4 lightly filled.",
        "odd-meter",
        [M118],
        4,
        _loop(
            M118,
            4,
            lambda bar: grouped_pulse_events(M118, [3, 3, 3, 2], accent_vel=instr.VEL_CUE if bar == 0 else instr.VEL_ACCENT)
            + (fill_events([10 * EIGHTH], instr.HIHAT_CLOSED, instr.VEL_LIGHT) if bar == 3 else []),
        ),
        tags=["challenge", "odd-meter"],
        grouping=[3, 3, 3, 2],
    )
)

# --- D. Afro-Cuban and Afro-diasporic ------------------------------------------------------

AFRO_CUBAN_CONTEXT = (
    "Introductory Afro-Cuban pulse/clave guide, not a substitute for learning the tradition and its "
    "performance practice from qualified sources."
)
AFRO_DIASPORIC_CONTEXT = (
    "Introductory Afro-diasporic/Brazilian guide - a simplified pulse/bell reference, not a claim to "
    "represent an entire genre's rhythmic vocabulary."
)

for _id, _name, _variant, _direction in (
    ("two-bar-son-clave-3-2-loop", "Two-bar son clave 3-2 loop", "son", "3-2"),
    ("two-bar-son-clave-2-3-loop", "Two-bar son clave 2-3 loop", "son", "2-3"),
    ("two-bar-rumba-clave-3-2-loop", "Two-bar rumba clave 3-2 loop", "rumba", "3-2"),
    ("two-bar-rumba-clave-2-3-loop", "Two-bar rumba clave 2-3 loop", "rumba", "2-3"),
):
    PATTERNS.append(
        _pattern(
            _id,
            _name,
            f"A repeating two-bar {_variant} clave, {_direction} direction, on a single Woodblock voice.",
            "afro-cuban",
            [M44],
            2,
            clave_events(_variant, _direction),
            tags=["intermediate", "afro-cuban", f"clave-{_direction}"],
            grouping=[3, 2] if _direction == "3-2" else [2, 3],
            source_context=AFRO_CUBAN_CONTEXT,
            simplification_note="Played on a single Woodblock voice, standing in for a real claves pair.",
            sections=[SectionSpec(0, "BAR 1"), SectionSpec(M44.bar_ticks, "BAR 2")],
            loop_cue_behaviour="The clave shape itself is the loop cue - its asymmetry marks bar 1 vs bar 2.",
        )
    )

PATTERNS.append(
    _pattern(
        "four-bar-bossa-guide",
        "Four-bar bossa guide",
        "The son-clave shape (Sidestick) repeated over four bars as a bossa nova rhythm guide.",
        "afro-cuban",
        [M44],
        4,
        clave_events("son", "3-2", note=instr.SIDESTICK) + [
            EventSpec(M44.bar_ticks * 2 + e.tick, e.note, e.velocity, e.duration) for e in clave_events("son", "3-2", note=instr.SIDESTICK)
        ],
        tags=["intermediate", "afro-cuban"],
        source_context=AFRO_CUBAN_CONTEXT,
        simplification_note="A pulse guide, not a transcription of a specific bossa nova arrangement.",
    )
)

_samba_pulse_bar = [
    EventSpec(0, instr.KICK, instr.VEL_LIGHT, instr.CLICK_DURATION_TICKS),
    EventSpec(2 * M44.unit_ticks, instr.KICK, instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS),
]
PATTERNS.append(
    _pattern(
        "four-bar-samba-pulse-guide",
        "Four-bar samba pulse guide",
        "A single-voice surdo-style pulse guide (accent on beat 3) over four bars.",
        "afro-diasporic",
        [M44],
        4,
        _loop(M44, 4, lambda bar: _samba_pulse_bar),
        tags=["intermediate", "afro-diasporic"],
        source_context=AFRO_DIASPORIC_CONTEXT,
        simplification_note="A single-voice surdo guide; a real bateria layers multiple surdo pitches plus caixa, tamborim and agogo.",
    )
)

_bell_12_8 = fill_events([i * EIGHTH for i in (0, 3, 5, 6, 8, 10)], instr.WOODBLOCK, instr.VEL_ACCENT)
PATTERNS.append(
    _pattern(
        "four-bar-12-8-afro-bell-guide",
        "Four-bar 12/8 Afro bell guide",
        "The 12/8 standard-pattern bell rhythm repeated over four bars.",
        "afro-diasporic",
        [M128],
        4,
        _loop(M128, 4, lambda bar: _bell_12_8),
        tags=["intermediate", "afro-diasporic"],
        source_context=AFRO_DIASPORIC_CONTEXT,
    )
)

_bell_6_8 = fill_events([0, 2 * EIGHTH, 3 * EIGHTH, 5 * EIGHTH], instr.WOODBLOCK, instr.VEL_ACCENT)
PATTERNS.append(
    _pattern(
        "four-bar-6-8-afro-bell-guide",
        "Four-bar 6/8 Afro bell guide",
        "A commonly cited 6/8 excerpt of the standard-pattern bell rhythm, repeated over four bars.",
        "afro-diasporic",
        [M68],
        4,
        _loop(M68, 4, lambda bar: _bell_6_8),
        tags=["intermediate", "afro-diasporic"],
        source_context=AFRO_DIASPORIC_CONTEXT,
    )
)

_tresillo_bar = fill_events([0, 3 * EIGHTH, 6 * EIGHTH], instr.WOODBLOCK, instr.VEL_ACCENT)
PATTERNS.append(
    _pattern(
        "four-bar-tresillo-guide",
        "Four-bar tresillo guide",
        "The tresillo (3+3+2) repeated over four bars.",
        "afro-diasporic",
        [M44],
        4,
        _loop(M44, 4, lambda bar: _tresillo_bar),
        tags=["easy", "afro-diasporic"],
        grouping=[3, 3, 2],
        source_context=AFRO_DIASPORIC_CONTEXT,
    )
)

_habanera_bar = fill_events([0, 3 * SIXTEENTH, 4 * SIXTEENTH, 6 * SIXTEENTH], instr.WOODBLOCK, instr.VEL_ACCENT)
PATTERNS.append(
    _pattern(
        "four-bar-habanera-guide",
        "Four-bar habanera guide",
        "The habanera figure repeated over four bars.",
        "afro-diasporic",
        [M44],
        4,
        _loop(M44, 4, lambda bar: _habanera_bar),
        tags=["intermediate", "afro-diasporic"],
        source_context=AFRO_DIASPORIC_CONTEXT,
    )
)

# --- E. Middle Eastern -----------------------------------------------------------------

DUM_TAK_DISCLAIMER = (
    "Grouping and basic dum/tak placement here follow commonly cited introductory descriptions of this "
    "rhythmic cycle; exact stroke pattern, ornamentation and regional practice vary - treat this as a "
    "schematic starting point, not a transcription of a specific performance tradition."
)

_maqsum_bar = [
    EventSpec(tick, instr.DUM if tick in (0, 4 * EIGHTH) else instr.TAK, instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS)
    for tick in (0, EIGHTH, 3 * EIGHTH, 4 * EIGHTH, 6 * EIGHTH)
]
PATTERNS.append(
    _pattern(
        "four-bar-maqsum-guide",
        "Four-bar maqsum guide",
        "The maqsum dum-tak cycle repeated over four bars.",
        "middle-eastern",
        [M44],
        4,
        _loop(M44, 4, lambda bar: _maqsum_bar),
        tags=["intermediate", "middle-eastern"],
        source_context=DUM_TAK_DISCLAIMER,
    )
)

_baladi_bar = [
    EventSpec(0, instr.DUM, instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS),
    EventSpec(EIGHTH, instr.TAK, instr.VEL_NORMAL, instr.CLICK_DURATION_TICKS),
    EventSpec(3 * EIGHTH, instr.TAK, instr.VEL_NORMAL, instr.CLICK_DURATION_TICKS),
    EventSpec(4 * EIGHTH, instr.DUM, instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS),
    EventSpec(5 * EIGHTH, instr.DUM, instr.VEL_SECONDARY, instr.CLICK_DURATION_TICKS),
    EventSpec(6 * EIGHTH, instr.TAK, instr.VEL_NORMAL, instr.CLICK_DURATION_TICKS),
]
PATTERNS.append(
    _pattern(
        "four-bar-baladi-guide",
        "Four-bar baladi guide",
        "The baladi dum-tak cycle repeated over four bars.",
        "middle-eastern",
        [M44],
        4,
        _loop(M44, 4, lambda bar: _baladi_bar),
        tags=["intermediate", "middle-eastern"],
        source_context=DUM_TAK_DISCLAIMER,
    )
)

_saidi_bar = [
    EventSpec(0, instr.DUM, instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS),
    EventSpec(EIGHTH, instr.DUM, instr.VEL_SECONDARY, instr.CLICK_DURATION_TICKS),
    EventSpec(2 * EIGHTH, instr.TAK, instr.VEL_NORMAL, instr.CLICK_DURATION_TICKS),
    EventSpec(4 * EIGHTH, instr.DUM, instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS),
    EventSpec(5 * EIGHTH, instr.TAK, instr.VEL_NORMAL, instr.CLICK_DURATION_TICKS),
    EventSpec(6 * EIGHTH, instr.TAK, instr.VEL_NORMAL, instr.CLICK_DURATION_TICKS),
]
PATTERNS.append(
    _pattern(
        "four-bar-saidi-guide",
        "Four-bar saidi guide",
        "The saidi dum-tak cycle repeated over four bars.",
        "middle-eastern",
        [M44],
        4,
        _loop(M44, 4, lambda bar: _saidi_bar),
        tags=["intermediate", "middle-eastern"],
        source_context=DUM_TAK_DISCLAIMER,
    )
)

PATTERNS.append(
    _pattern(
        "four-bar-malfuf-guide",
        "Four-bar malfuf guide",
        "The fast 2/4 malfuf dum-tak alternation repeated over four bars.",
        "middle-eastern",
        [M24],
        4,
        _loop(M24, 4, lambda bar: [
            EventSpec(0, instr.DUM, instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS),
            EventSpec(2 * EIGHTH, instr.TAK, instr.VEL_NORMAL, instr.CLICK_DURATION_TICKS),
        ]),
        bpm=140.0,
        tags=["intermediate", "middle-eastern"],
        source_context=DUM_TAK_DISCLAIMER,
    )
)

PATTERNS.append(
    _pattern(
        "two-bar-samaai-thaqil-guide",
        "Two-bar samaai thaqil guide",
        "A 10/8 cycle grouped 3+2+2+3, two bars, dum on each group's first unit and tak elsewhere.",
        "middle-eastern",
        [M108],
        2,
        _loop(M108, 2, lambda bar: grouped_pulse_events(M108, [3, 2, 2, 3], accent_note=instr.DUM, normal_note=instr.TAK)),
        bpm=80.0,
        tags=["advanced", "middle-eastern", "odd-meter"],
        grouping=[3, 2, 2, 3],
        source_context=DUM_TAK_DISCLAIMER,
    )
)

PATTERNS.append(
    _pattern(
        "four-bar-karsilama-guide",
        "Four-bar karsilama guide",
        "9/8 grouped 2+2+2+3 (karsilama), four bars, dum on each group's first unit and tak elsewhere.",
        "middle-eastern",
        [M98],
        4,
        _loop(M98, 4, lambda bar: grouped_pulse_events(M98, [2, 2, 2, 3], accent_note=instr.DUM, normal_note=instr.TAK)),
        tags=["advanced", "middle-eastern", "odd-meter"],
        grouping=[2, 2, 2, 3],
        source_context=DUM_TAK_DISCLAIMER,
    )
)

# --- F. Mixed meter --------------------------------------------------------------------


def _mixed_events(meters: list[Meter], bar_count: int) -> list[EventSpec]:
    events: list[EventSpec] = []
    tick = 0
    for bar in range(bar_count):
        meter = meters[bar % len(meters)]
        grouping = [3, meter.numerator - 3] if meter.numerator > 4 and meter.denominator == 8 else [meter.numerator]
        accent_vel = instr.VEL_CUE if bar == 0 else instr.VEL_ACCENT
        events += [EventSpec(tick + e.tick, e.note, e.velocity, e.duration) for e in grouped_pulse_events(meter, grouping, accent_vel=accent_vel)]
        tick += meter.bar_ticks
    return events


for _id, _name, _meters in (
    ("four-bar-alternating-3-4-4-4", "Four-bar alternating 3/4-4/4", [M34, M44]),
    ("four-bar-alternating-6-8-3-4", "Four-bar alternating 6/8-3/4", [M68, M34]),
    ("four-bar-alternating-5-8-7-8", "Four-bar alternating 5/8-7/8", [M58, M78]),
    ("four-bar-alternating-7-8-4-4", "Four-bar alternating 7/8-4/4", [M78, M44]),
):
    PATTERNS.append(
        _pattern(
            _id,
            _name,
            f"{_meters[0]} and {_meters[1]} alternate bar by bar over a four-bar loop, looping seamlessly back to bar 1.",
            "mixed-meter",
            _meters,
            4,
            _mixed_events(_meters, 4),
            tags=["challenge", "mixed-meter"],
            sections=[SectionSpec(t, f"BAR {i + 1}") for i, t in enumerate(bar_start_ticks(_meters, 4))],
        )
    )

PATTERNS.append(
    _pattern(
        "eight-bar-mixed-meter-phrase-guide",
        "Eight-bar mixed-meter phrase guide",
        "7/8 and 4/4 alternate bar by bar across an eight-bar phrase, looping seamlessly back to bar 1.",
        "mixed-meter",
        [M78, M44],
        8,
        _mixed_events([M78, M44], 8),
        tags=["advanced", "mixed-meter"],
    )
)


PERFORMANCE_PATTERNS: list[PatternSpec] = PATTERNS
