"""Section A-H of PLAN.md: fundamentals through internal-time/challenge patterns."""

from __future__ import annotations

import random

from rhythm_library import instruments as instr
from rhythm_library.model import EventSpec, Meter, PatternSpec, SectionSpec, bar_start_ticks, total_ticks
from rhythm_library.patterns.helpers import (
    EIGHTH,
    SIXTEENTH,
    TRIPLET,
    bar_markers,
    fill_events,
    grouped_pulse_events,
    repeat_events_across_bars,
    swing_pair_events,
)

M44 = Meter(4, 4)
M34 = Meter(3, 4)
M68 = Meter(6, 8)
M98 = Meter(9, 8)
M128 = Meter(12, 8)
M54 = Meter(5, 4)
M58 = Meter(5, 8)
M78 = Meter(7, 8)
M74 = Meter(7, 4)
M118 = Meter(11, 8)

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
    grid_ticks: int = SIXTEENTH,
    tags: list[str] = (),
    sections: list[SectionSpec] = (),
    recommended_use: str = "",
    grouping: list[int] | None = None,
    swing_ratio: str | None = None,
    click_language: str = "",
    dominant_sounds: list[str] = (),
    listening_target: str = "",
) -> PatternSpec:
    return PatternSpec(
        id=id_,
        display_name=display_name,
        description=description,
        area="educational",
        family=family,
        meters=meters,
        bpm=bpm,
        bar_count=bar_count,
        grid_ticks=grid_ticks,
        events=sorted(events, key=lambda e: e.tick),
        loop_length_ticks=total_ticks(meters, bar_count),
        tags=list(tags),
        sections=list(sections) or bar_markers(bar_count, meters[0].bar_ticks),
        recommended_use=recommended_use,
        grouping=grouping,
        swing_ratio=swing_ratio,
        dominant_sounds=list(dominant_sounds),
        click_language=click_language,
        listening_target=listening_target,
    )


def _quarter_pulse(active: list[bool], accent_first_only: bool = True) -> list[EventSpec]:
    """4 quarter-note positions; `active[i]` False means silence at that beat."""
    events: list[EventSpec] = []
    for beat, on in enumerate(active):
        if not on:
            continue
        accent = accent_first_only and beat == 0
        events.append(
            EventSpec(
                beat * M44.unit_ticks,
                instr.SNARE if accent else instr.SNARE,
                instr.VEL_ACCENT if accent else instr.VEL_NORMAL,
                instr.CLICK_DURATION_TICKS,
            )
        )
    return events


PATTERNS: list[PatternSpec] = []

# --- A. Fundamentals --------------------------------------------------------------------

PATTERNS.append(
    _pattern(
        "flat-mechanical-4-4",
        "Flat mechanical 4/4",
        "Four equal quarter-note clicks, no accent at all - a purely mechanical pulse reference.",
        "fundamentals",
        [M44],
        2,
        repeat_events_across_bars(
            [EventSpec(b * M44.unit_ticks, instr.SNARE, instr.VEL_NORMAL, instr.CLICK_DURATION_TICKS) for b in range(4)],
            M44.bar_ticks,
            2,
        ),
        tags=["easy", "fundamentals"],
        click_language="L L L L",
        recommended_use="Absolute baseline pulse reference before any accent concept is introduced.",
    )
)

PATTERNS.append(
    _pattern(
        "conventional-bar-marker-4-4",
        "Conventional bar marker 4/4",
        "Beat 1 accented, beats 2-4 at normal click velocity - the ordinary bar-orientation click.",
        "fundamentals",
        [M44],
        2,
        repeat_events_across_bars(grouped_pulse_events(M44, [4]), M44.bar_ticks, 2),
        tags=["easy", "fundamentals"],
        click_language="H L L L",
        recommended_use="Establish basic bar orientation before any other concept.",
    )
)

PATTERNS.append(
    _pattern(
        "strong-weak-duple-4-4",
        "Strong-weak duple 4/4",
        "Alternating strong/weak quarter notes (1 and 3 strong, 2 and 4 weak) - the duple-metre feel.",
        "fundamentals",
        [M44],
        2,
        repeat_events_across_bars(
            [
                EventSpec(b * M44.unit_ticks, instr.SNARE, instr.VEL_ACCENT if b % 2 == 0 else instr.VEL_NORMAL, instr.CLICK_DURATION_TICKS)
                for b in range(4)
            ],
            M44.bar_ticks,
            2,
        ),
        tags=["easy", "fundamentals"],
        click_language="H L H L",
        recommended_use="Feel the duple grouping of 4/4 as two strong-weak pairs, not four equal beats.",
    )
)

PATTERNS.append(
    _pattern(
        "two-beat-cut-time",
        "Two-beat cut time",
        "Half-note pulse over a notated 4/4 bar - only beats 1 and 3 click, tagged as cut-time feel.",
        "fundamentals",
        [M44],
        2,
        repeat_events_across_bars(
            [
                EventSpec(0, instr.SNARE, instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS),
                EventSpec(2 * M44.unit_ticks, instr.SNARE, instr.VEL_NORMAL, instr.CLICK_DURATION_TICKS),
            ],
            M44.bar_ticks,
            2,
        ),
        tags=["easy", "fundamentals", "cut-time"],
        click_language="H L",
        recommended_use="Practice feeling two beats per bar (cut time) inside a notated 4/4 bar.",
    )
)

PATTERNS.append(
    _pattern(
        "downbeat-only-4-4",
        "Downbeat only 4/4",
        "Click on beat 1 only, silence on beats 2-4 - internalise the bar length between downbeats.",
        "fundamentals",
        [M44],
        2,
        repeat_events_across_bars([EventSpec(0, instr.SNARE, instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS)], M44.bar_ticks, 2),
        tags=["intermediate", "fundamentals", "internal-time"],
        click_language="H - - -",
        recommended_use="Hold internal tempo across a full silent bar between downbeats.",
    )
)

PATTERNS.append(
    _pattern(
        "two-bar-downbeat",
        "Two-bar downbeat",
        "Downbeat-only click, with bar 2's downbeat at a lower accent - distinguishes bar 1 from bar 2.",
        "fundamentals",
        [M44],
        2,
        [
            EventSpec(0, instr.SNARE, instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS),
            EventSpec(M44.bar_ticks, instr.SNARE, instr.VEL_SECONDARY, instr.CLICK_DURATION_TICKS),
        ],
        tags=["intermediate", "fundamentals", "internal-time"],
        click_language="H - - - | h - - -",
        recommended_use="Distinguish odd/even bars while holding tempo across two silent bars.",
    )
)

_four_bar_phrase_events = [EventSpec(0, instr.WOODBLOCK, instr.VEL_CUE, instr.CUE_DURATION_TICKS)]
_four_bar_phrase_events += grouped_pulse_events(M44, [4], bar_offset=0)[1:]
_phrase_bars = []
for _bar in range(4):
    _bar_events = grouped_pulse_events(M44, [4], bar_offset=_bar * M44.bar_ticks)
    if _bar == 0:
        _bar_events[0] = EventSpec(0, instr.WOODBLOCK, instr.VEL_CUE, instr.CUE_DURATION_TICKS)
    _phrase_bars += _bar_events
PATTERNS.append(
    _pattern(
        "four-bar-phrase-marker",
        "Four-bar phrase marker",
        "Bar 1 opens with a distinct cue note, bars 1-4 each accent their own downbeat - marks a four-bar phrase.",
        "fundamentals",
        [M44],
        4,
        _phrase_bars,
        tags=["easy", "fundamentals"],
        click_language="X L L L | H L L L | H L L L | H L L L",
        recommended_use="Recognise a four-bar phrase boundary by ear.",
    )
)

PATTERNS.append(
    _pattern(
        "backbeat-2-and-4",
        "Backbeat 2 and 4",
        "Accent on beats 2 and 4, normal click on 1 and 3 - the fundamental backbeat feel.",
        "fundamentals",
        [M44],
        2,
        repeat_events_across_bars(
            [
                EventSpec(0 * M44.unit_ticks, instr.SNARE, instr.VEL_NORMAL, instr.CLICK_DURATION_TICKS),
                EventSpec(1 * M44.unit_ticks, instr.SNARE, instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS),
                EventSpec(2 * M44.unit_ticks, instr.SNARE, instr.VEL_NORMAL, instr.CLICK_DURATION_TICKS),
                EventSpec(3 * M44.unit_ticks, instr.SNARE, instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS),
            ],
            M44.bar_ticks,
            2,
        ),
        tags=["easy", "fundamentals", "backbeat"],
        click_language="L H L H",
        recommended_use="Internalise the backbeat before playing along with real drum grooves.",
    )
)

PATTERNS.append(
    _pattern(
        "jazz-two-and-four",
        "Jazz two and four",
        "Click only on beats 2 and 4, complete silence on 1 and 3 - jazz-style time reference.",
        "fundamentals",
        [M44],
        2,
        repeat_events_across_bars(
            [
                EventSpec(1 * M44.unit_ticks, instr.SNARE, instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS),
                EventSpec(3 * M44.unit_ticks, instr.SNARE, instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS),
            ],
            M44.bar_ticks,
            2,
        ),
        tags=["intermediate", "fundamentals", "jazz"],
        click_language="- H - H",
        recommended_use="Jazz phrasing practice without a click on every beat.",
    )
)

PATTERNS.append(
    _pattern(
        "beat-one-and-three",
        "Beat one and three",
        "Click on beats 1 and 3 only - a sparser downbeat/mid-bar reference.",
        "fundamentals",
        [M44],
        2,
        repeat_events_across_bars(
            [
                EventSpec(0, instr.SNARE, instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS),
                EventSpec(2 * M44.unit_ticks, instr.SNARE, instr.VEL_NORMAL, instr.CLICK_DURATION_TICKS),
            ],
            M44.bar_ticks,
            2,
        ),
        tags=["easy", "fundamentals"],
        click_language="H - L -",
        recommended_use="Bridge between downbeat-only and full quarter-note click.",
    )
)

PATTERNS.append(
    _pattern(
        "offbeat-eighths",
        "Offbeat eighths",
        "Click only on every eighth-note \"and\" position, none on the beat itself.",
        "fundamentals",
        [M44],
        2,
        repeat_events_across_bars(
            fill_events([b * M44.unit_ticks + EIGHTH for b in range(4)], instr.HIHAT_CLOSED, instr.VEL_LIGHT),
            M44.bar_ticks,
            2,
        ),
        tags=["intermediate", "fundamentals", "syncopation"],
        click_language="(silent) . x . x . x . x",
        recommended_use="Feel the offbeat as its own reference point, independent of the downbeat.",
    )
)

PATTERNS.append(
    _pattern(
        "sparse-beat-one-and-three",
        "Sparse beat one and three",
        "Same as beat-one-and-three but at ghost velocity throughout - a near-silent internal-time aid.",
        "fundamentals",
        [M44],
        2,
        repeat_events_across_bars(
            [
                EventSpec(0, instr.SNARE, instr.VEL_GHOST, instr.CLICK_DURATION_TICKS),
                EventSpec(2 * M44.unit_ticks, instr.SNARE, instr.VEL_GHOST, instr.CLICK_DURATION_TICKS),
            ],
            M44.bar_ticks,
            2,
        ),
        tags=["challenge", "fundamentals", "internal-time"],
        click_language="h - l -",
        recommended_use="Advanced internal-time practice: barely audible reference points only.",
    )
)

# --- B. Four-four ------------------------------------------------------------------------

PATTERNS.append(
    _pattern(
        "beat-1-only",
        "Beat 1 only",
        "Click on beat 1 of every bar, nothing else.",
        "four-four",
        [M44],
        4,
        repeat_events_across_bars([EventSpec(0, instr.SNARE, instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS)], M44.bar_ticks, 4),
        tags=["intermediate", "four-four", "internal-time"],
        click_language="H - - -",
    )
)

PATTERNS.append(
    _pattern(
        "beats-1-and-3",
        "Beats 1 and 3",
        "Click on beats 1 and 3 only.",
        "four-four",
        [M44],
        4,
        repeat_events_across_bars(
            [
                EventSpec(0, instr.SNARE, instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS),
                EventSpec(2 * M44.unit_ticks, instr.SNARE, instr.VEL_NORMAL, instr.CLICK_DURATION_TICKS),
            ],
            M44.bar_ticks,
            4,
        ),
        tags=["easy", "four-four"],
        click_language="H - L -",
    )
)

PATTERNS.append(
    _pattern(
        "beats-2-and-4",
        "Beats 2 and 4",
        "Click on beats 2 and 4 only - pure backbeat reference, no downbeat.",
        "four-four",
        [M44],
        4,
        repeat_events_across_bars(
            [
                EventSpec(1 * M44.unit_ticks, instr.SNARE, instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS),
                EventSpec(3 * M44.unit_ticks, instr.SNARE, instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS),
            ],
            M44.bar_ticks,
            4,
        ),
        tags=["intermediate", "four-four", "backbeat"],
        click_language="- H - H",
    )
)

PATTERNS.append(
    _pattern(
        "conventional-bar-marker",
        "Conventional bar marker",
        "Beat 1 accented, beats 2-4 normal - the everyday four-bar click.",
        "four-four",
        [M44],
        4,
        repeat_events_across_bars(grouped_pulse_events(M44, [4]), M44.bar_ticks, 4),
        tags=["easy", "four-four"],
        click_language="H L L L",
    )
)

PATTERNS.append(
    _pattern(
        "backbeat",
        "Backbeat",
        "Accent on 2 and 4, normal click on 1 and 3, four bars.",
        "four-four",
        [M44],
        4,
        repeat_events_across_bars(
            [
                EventSpec(0, instr.SNARE, instr.VEL_NORMAL, instr.CLICK_DURATION_TICKS),
                EventSpec(1 * M44.unit_ticks, instr.SNARE, instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS),
                EventSpec(2 * M44.unit_ticks, instr.SNARE, instr.VEL_NORMAL, instr.CLICK_DURATION_TICKS),
                EventSpec(3 * M44.unit_ticks, instr.SNARE, instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS),
            ],
            M44.bar_ticks,
            4,
        ),
        tags=["easy", "four-four", "backbeat"],
        click_language="L H L H",
    )
)

PATTERNS.append(
    _pattern(
        "eighth-note-subdivision",
        "Eighth-note subdivision",
        "Quarter-note pulse with the eighth-note offbeat filled in at light velocity.",
        "four-four",
        [M44],
        4,
        repeat_events_across_bars(
            grouped_pulse_events(M44, [4]) + fill_events([b * M44.unit_ticks + EIGHTH for b in range(4)], instr.HIHAT_CLOSED, instr.VEL_LIGHT),
            M44.bar_ticks,
            4,
        ),
        tags=["easy", "four-four", "subdivisions"],
        click_language="H l L l L l L l",
    )
)

PATTERNS.append(
    _pattern(
        "sixteenth-note-subdivision",
        "Sixteenth-note subdivision",
        "Full sixteenth-note grid: quarter pulse, eighth offbeats, and both remaining sixteenth landmarks.",
        "four-four",
        [M44],
        4,
        repeat_events_across_bars(
            grouped_pulse_events(M44, [4])
            + fill_events([b * M44.unit_ticks + EIGHTH for b in range(4)], instr.HIHAT_CLOSED, instr.VEL_LIGHT)
            + fill_events(
                [b * M44.unit_ticks + o for b in range(4) for o in (SIXTEENTH, 3 * SIXTEENTH)],
                instr.HIHAT_CLOSED,
                instr.VEL_GHOST,
            ),
            M44.bar_ticks,
            4,
        ),
        tags=["intermediate", "four-four", "subdivisions"],
        click_language="H l l l L l l l L l l l L l l l",
    )
)

PATTERNS.append(
    _pattern(
        "sixteenth-landmarks-beat-and-and",
        "Sixteenth landmarks: beat and and",
        "Only the beat and the eighth-note \"and\" click - the coarse half of the sixteenth grid.",
        "four-four",
        [M44],
        4,
        repeat_events_across_bars(
            grouped_pulse_events(M44, [4]) + fill_events([b * M44.unit_ticks + EIGHTH for b in range(4)], instr.HIHAT_CLOSED, instr.VEL_LIGHT),
            M44.bar_ticks,
            4,
        ),
        tags=["intermediate", "four-four", "subdivisions"],
        click_language="H . l . L . l . L . l . L . l .",
        recommended_use="Isolate the beat and the 'and' before adding the 'e' and 'a' sixteenths.",
    )
)

PATTERNS.append(
    _pattern(
        "sixteenth-landmarks-e-and-a",
        "Sixteenth landmarks: e and a",
        "Only the 'e' and 'a' sixteenth-note landmarks click, with the beat itself silent.",
        "four-four",
        [M44],
        4,
        repeat_events_across_bars(
            fill_events(
                [b * M44.unit_ticks + o for b in range(4) for o in (SIXTEENTH, 3 * SIXTEENTH)],
                instr.HIHAT_CLOSED,
                instr.VEL_NORMAL,
            ),
            M44.bar_ticks,
            4,
        ),
        tags=["challenge", "four-four", "subdivisions"],
        click_language=". l . l . l . l . l . l . l . l",
        recommended_use="Practice placing the 'e' and 'a' sixteenths accurately without a beat reference.",
    )
)

PATTERNS.append(
    _pattern(
        "eighth-note-accent-every-3-subdivisions",
        "Eighth-note accent every 3 subdivisions",
        "A continuous eighth-note grid accented every third eighth - a 3-against-2 cross-accent over 4/4.",
        "four-four",
        [M44],
        4,
        repeat_events_across_bars(
            [
                EventSpec(
                    i * EIGHTH,
                    instr.HIHAT_CLOSED,
                    instr.VEL_ACCENT if i % 3 == 0 else instr.VEL_LIGHT,
                    instr.CLICK_DURATION_TICKS,
                )
                for i in range(8)
            ],
            M44.bar_ticks,
            4,
        ),
        tags=["challenge", "four-four", "polyrhythm"],
        recommended_use="Hear a 3-eighth-note accent group cut across the underlying 4/4 grid.",
    )
)

PATTERNS.append(
    _pattern(
        "quarter-note-triplet-guide",
        "Quarter-note triplet guide",
        "Quarter-note triplets across two beats, accented on each triplet's first note.",
        "four-four",
        [M44],
        4,
        repeat_events_across_bars(
            [
                EventSpec(i * (2 * TRIPLET + 1), instr.HIHAT_CLOSED, instr.VEL_ACCENT if i % 3 == 0 else instr.VEL_LIGHT, instr.CLICK_DURATION_TICKS)
                for i in range(6)
            ],
            M44.bar_ticks,
            4,
        ),
        tags=["challenge", "four-four", "polyrhythm"],
        recommended_use="Feel quarter-note triplets (3 against 2 beats) as their own pulse.",
    )
)

PATTERNS.append(
    _pattern(
        "3-over-4-guide",
        "3-over-4 guide",
        "Three evenly-spaced accents across one bar of 4/4 (a 3:4 polyrhythm), on top of the quarter pulse.",
        "four-four",
        [M44],
        4,
        repeat_events_across_bars(
            grouped_pulse_events(M44, [4], normal_vel=instr.VEL_LIGHT)
            + fill_events([round(i * M44.bar_ticks / 3) for i in range(3)], instr.HIHAT_CLOSED, instr.VEL_ACCENT),
            M44.bar_ticks,
            4,
        ),
        tags=["challenge", "four-four", "polyrhythm"],
        recommended_use="Hear 3 evenly spaced accents against the steady quarter-note pulse.",
    )
)

PATTERNS.append(
    _pattern(
        "4-over-3-guide",
        "4-over-3 guide",
        "Four evenly-spaced accents across a bar felt in 3 (a 4:3 polyrhythm reference).",
        "four-four",
        [M34],
        4,
        repeat_events_across_bars(
            grouped_pulse_events(M34, [3], normal_vel=instr.VEL_LIGHT)
            + fill_events([round(i * M34.bar_ticks / 4) for i in range(4)], instr.HIHAT_CLOSED, instr.VEL_ACCENT),
            M34.bar_ticks,
            4,
        ),
        tags=["challenge", "four-four", "polyrhythm"],
        recommended_use="Hear 4 evenly spaced accents against a 3/4 pulse.",
    )
)

PATTERNS.append(
    _pattern(
        "5-over-4-guide",
        "5-over-4 guide",
        "Five evenly-spaced accents across one bar of 4/4 (a 5:4 polyrhythm reference).",
        "four-four",
        [M44],
        4,
        repeat_events_across_bars(
            grouped_pulse_events(M44, [4], normal_vel=instr.VEL_LIGHT)
            + fill_events([round(i * M44.bar_ticks / 5) for i in range(5)], instr.HIHAT_CLOSED, instr.VEL_ACCENT),
            M44.bar_ticks,
            4,
        ),
        tags=["challenge", "four-four", "polyrhythm"],
        recommended_use="Hear 5 evenly spaced accents against the steady quarter-note pulse.",
    )
)

_syncopated_and_of_4 = grouped_pulse_events(M44, [4]) + fill_events([3 * M44.unit_ticks + EIGHTH], instr.HIHAT_CLOSED, instr.VEL_ACCENT)
PATTERNS.append(
    _pattern(
        "syncopated-anticipated-and-of-4",
        "Syncopated: anticipated and-of-4",
        "Normal bar marker with an added accent on the 'and' of beat 4, anticipating the next downbeat.",
        "four-four",
        [M44],
        4,
        repeat_events_across_bars(_syncopated_and_of_4, M44.bar_ticks, 4),
        tags=["intermediate", "four-four", "syncopation"],
        click_language="H L L L +",
        recommended_use="Hear a syncopated anticipation of beat 1 landing early, on the 'and' of 4.",
    )
)

for _label, _offset in (("e", SIXTEENTH), ("and", EIGHTH), ("a", 3 * SIXTEENTH)):
    PATTERNS.append(
        _pattern(
            f"displaced-click-on-{_label}",
            f"Displaced click on {_label}",
            f"The main click is displaced off the beat entirely, landing only on the '{_label}' subdivision of each beat.",
            "four-four",
            [M44],
            4,
            repeat_events_across_bars(
                fill_events([b * M44.unit_ticks + _offset for b in range(4)], instr.SNARE, instr.VEL_ACCENT),
                M44.bar_ticks,
                4,
            ),
            tags=["challenge", "four-four", "syncopation"],
            recommended_use=f"Practice locking to a displaced click that never lands on the beat, only its '{_label}'.",
        )
    )

# --- C. Three-four -------------------------------------------------------------------------

PATTERNS.append(
    _pattern(
        "waltz-downbeat",
        "Waltz downbeat",
        "Beat 1 accented, beats 2-3 normal - the classic waltz bar marker.",
        "three-four",
        [M34],
        4,
        repeat_events_across_bars(grouped_pulse_events(M34, [3]), M34.bar_ticks, 4),
        tags=["easy", "three-four"],
        click_language="H L L",
    )
)

PATTERNS.append(
    _pattern(
        "waltz-beat-1-only",
        "Waltz beat 1 only",
        "Click on beat 1 of every 3/4 bar only.",
        "three-four",
        [M34],
        4,
        repeat_events_across_bars([EventSpec(0, instr.SNARE, instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS)], M34.bar_ticks, 4),
        tags=["intermediate", "three-four", "internal-time"],
        click_language="H - -",
    )
)

PATTERNS.append(
    _pattern(
        "waltz-2-and-3",
        "Waltz 2 and 3",
        "Click on beats 2 and 3 only, beat 1 silent - waltz backbeat-style reference.",
        "three-four",
        [M34],
        4,
        repeat_events_across_bars(
            [
                EventSpec(1 * M34.unit_ticks, instr.SNARE, instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS),
                EventSpec(2 * M34.unit_ticks, instr.SNARE, instr.VEL_NORMAL, instr.CLICK_DURATION_TICKS),
            ],
            M34.bar_ticks,
            4,
        ),
        tags=["intermediate", "three-four"],
        click_language="- H L",
    )
)

_hemiola = grouped_pulse_events(M34, [3]) + [EventSpec(M34.bar_ticks + i * M34.unit_ticks, instr.SNARE, instr.VEL_ACCENT if i % 2 == 0 else instr.VEL_NORMAL, instr.CLICK_DURATION_TICKS) for i in range(3)]
PATTERNS.append(
    _pattern(
        "hemiola-accent-over-two-bars",
        "Hemiola accent over two bars",
        "3/4 underlying meter, but the second bar's accents fall in a 2+2+2 grouping across the six beats - the classic hemiola.",
        "three-four",
        [M34],
        2,
        _hemiola,
        tags=["challenge", "three-four", "polyrhythm"],
        recommended_use="Hear a 2-against-3 hemiola: six 3/4 beats reorganised into three groups of two.",
    )
)

PATTERNS.append(
    _pattern(
        "three-four-eighth-subdivision",
        "Three-four eighth subdivision",
        "Waltz downbeat pattern with the eighth-note offbeat filled in.",
        "three-four",
        [M34],
        4,
        repeat_events_across_bars(
            grouped_pulse_events(M34, [3]) + fill_events([b * M34.unit_ticks + EIGHTH for b in range(3)], instr.HIHAT_CLOSED, instr.VEL_LIGHT),
            M34.bar_ticks,
            4,
        ),
        tags=["easy", "three-four", "subdivisions"],
        click_language="H l L l L l",
    )
)

PATTERNS.append(
    _pattern(
        "three-four-offbeat-eighths",
        "Three-four offbeat eighths",
        "Click only on the eighth-note offbeats of a 3/4 bar, none on the beat.",
        "three-four",
        [M34],
        4,
        repeat_events_across_bars(
            fill_events([b * M34.unit_ticks + EIGHTH for b in range(3)], instr.HIHAT_CLOSED, instr.VEL_NORMAL),
            M34.bar_ticks,
            4,
        ),
        tags=["intermediate", "three-four", "syncopation"],
    )
)

# --- D. Compound meter -----------------------------------------------------------------

_dotted_quarter = 720

PATTERNS.append(
    _pattern(
        "six-eight-two-dotted-quarter-pulses",
        "Six-eight: two dotted-quarter pulses",
        "Two dotted-quarter pulses per bar - the compound-meter beat, not six equal eighths.",
        "compound-meter",
        [M68],
        4,
        repeat_events_across_bars(
            grouped_pulse_events(M68, [2], normal_note=instr.SNARE, unit_ticks=_dotted_quarter), M68.bar_ticks, 4
        ),
        tags=["easy", "compound-meter"],
        grouping=[3, 3],
        click_language="H l l H l l",
    )
)

PATTERNS.append(
    _pattern(
        "six-eight-eighth-note-subdivision",
        "Six-eight eighth-note subdivision",
        "Full eighth-note subdivision underneath the two dotted-quarter pulses.",
        "compound-meter",
        [M68],
        4,
        repeat_events_across_bars(
            [
                EventSpec(i * EIGHTH, instr.SNARE if i % 3 == 0 else instr.HIHAT_CLOSED, instr.VEL_ACCENT if i == 0 else (instr.VEL_NORMAL if i % 3 == 0 else instr.VEL_LIGHT), instr.CLICK_DURATION_TICKS)
                for i in range(6)
            ],
            M68.bar_ticks,
            4,
        ),
        tags=["easy", "compound-meter", "subdivisions"],
    )
)

PATTERNS.append(
    _pattern(
        "six-eight-two-main-pulses-only",
        "Six-eight: two main pulses only",
        "Sparse dotted-quarter pulse only, no subdivision - a bare compound-meter reference.",
        "compound-meter",
        [M68],
        4,
        repeat_events_across_bars(
            [
                EventSpec(0, instr.SNARE, instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS),
                EventSpec(_dotted_quarter, instr.SNARE, instr.VEL_NORMAL, instr.CLICK_DURATION_TICKS),
            ],
            M68.bar_ticks,
            4,
        ),
        tags=["easy", "compound-meter"],
    )
)

PATTERNS.append(
    _pattern(
        "six-eight-shuffle-guide",
        "Six-eight shuffle guide",
        "Dotted-quarter pulses with the second eighth of each group ghosted, giving a shuffled compound feel.",
        "compound-meter",
        [M68],
        4,
        repeat_events_across_bars(
            grouped_pulse_events(M68, [2], unit_ticks=_dotted_quarter)
            + fill_events([EIGHTH, _dotted_quarter + EIGHTH], instr.HIHAT_CLOSED, instr.VEL_GHOST),
            M68.bar_ticks,
            4,
        ),
        tags=["intermediate", "compound-meter", "swing-shuffle"],
    )
)

PATTERNS.append(
    _pattern(
        "nine-eight-three-plus-three-plus-three",
        "Nine-eight: 3+3+3",
        "Three dotted-quarter pulses per bar of 9/8, evenly grouped (no single pulse standing out over the others).",
        "compound-meter",
        [M98],
        4,
        repeat_events_across_bars(
            grouped_pulse_events(M98, [1, 1, 1], unit_ticks=_dotted_quarter), M98.bar_ticks, 4
        ),
        tags=["easy", "compound-meter"],
        grouping=[3, 3, 3],
    )
)

PATTERNS.append(
    _pattern(
        "twelve-eight-four-dotted-quarter-pulses",
        "Twelve-eight: four dotted-quarter pulses",
        "Four dotted-quarter pulses per bar of 12/8.",
        "compound-meter",
        [M128],
        4,
        repeat_events_across_bars(grouped_pulse_events(M128, [4], unit_ticks=_dotted_quarter), M128.bar_ticks, 4),
        tags=["easy", "compound-meter"],
        grouping=[3, 3, 3, 3],
    )
)

PATTERNS.append(
    _pattern(
        "twelve-eight-blues-guide",
        "Twelve-eight blues guide",
        "Four dotted-quarter pulses, each with its middle eighth dropped - the classic 12/8 blues shuffle feel.",
        "compound-meter",
        [M128],
        4,
        repeat_events_across_bars(
            grouped_pulse_events(M128, [4], unit_ticks=_dotted_quarter)
            + fill_events([g * 3 * EIGHTH + 2 * EIGHTH for g in range(4)], instr.HIHAT_CLOSED, instr.VEL_GHOST),
            M128.bar_ticks,
            4,
        ),
        tags=["intermediate", "compound-meter", "swing-shuffle"],
        grouping=[3, 3, 3, 3],
    )
)

PATTERNS.append(
    _pattern(
        "twelve-eight-sparse-main-pulses",
        "Twelve-eight sparse main pulses",
        "Only the downbeat and the third dotted-quarter pulse click - a very sparse 12/8 reference.",
        "compound-meter",
        [M128],
        4,
        repeat_events_across_bars(
            [
                EventSpec(0, instr.SNARE, instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS),
                EventSpec(2 * 3 * EIGHTH, instr.SNARE, instr.VEL_NORMAL, instr.CLICK_DURATION_TICKS),
            ],
            M128.bar_ticks,
            4,
        ),
        tags=["challenge", "compound-meter", "internal-time"],
    )
)

# --- E. Odd meter ----------------------------------------------------------------------

_odd_meter_specs = [
    ("five-four-three-plus-two", "Five-four: 3+2", M54, [3, 2], "H L L H L"),
    ("five-four-two-plus-three", "Five-four: 2+3", M54, [2, 3], "H L H L L"),
    ("five-eight-three-plus-two", "Five-eight: 3+2", M58, [3, 2], "H L L H L"),
    ("five-eight-two-plus-three", "Five-eight: 2+3", M58, [2, 3], "H L H L L"),
    ("seven-eight-two-plus-two-plus-three", "Seven-eight: 2+2+3", M78, [2, 2, 3], "H L H L H L L"),
    ("seven-eight-two-plus-three-plus-two", "Seven-eight: 2+3+2", M78, [2, 3, 2], "H L H L L H L"),
    ("seven-eight-three-plus-two-plus-two", "Seven-eight: 3+2+2", M78, [3, 2, 2], "H L L H L H L"),
    ("seven-four-two-plus-two-plus-three", "Seven-four: 2+2+3", M74, [2, 2, 3], "H L H L H L L"),
    ("nine-eight-two-plus-two-plus-two-plus-three", "Nine-eight: 2+2+2+3", M98, [2, 2, 2, 3], "H L H L H L H L L"),
    ("eleven-eight-three-plus-three-plus-three-plus-two", "Eleven-eight: 3+3+3+2", M118, [3, 3, 3, 2], "H L L H L L H L L H L"),
    (
        "eleven-eight-two-plus-two-plus-three-plus-two-plus-two",
        "Eleven-eight: 2+2+3+2+2",
        M118,
        [2, 2, 3, 2, 2],
        "H L H L H L L H L H L",
    ),
]
for _id, _name, _meter, _grouping, _click in _odd_meter_specs:
    PATTERNS.append(
        _pattern(
            _id,
            _name,
            f"Additive grouping {'+'.join(str(g) for g in _grouping)} in {_meter}: accent marks the first unit of each group, "
            "never an equally-accented run of beats.",
            "odd-meter",
            [_meter],
            4,
            repeat_events_across_bars(grouped_pulse_events(_meter, _grouping), _meter.bar_ticks, 4),
            tags=["intermediate", "odd-meter"],
            grouping=_grouping,
            click_language=_click,
            recommended_use=f"Internalise {_meter} as {'+'.join(str(g) for g in _grouping)}, counted 'ONE-two"
            f"{'-three' if _grouping[0] == 3 else ''}, ...' by group, not as equal beats.",
        )
    )

# --- F. Mixed meter ----------------------------------------------------------------------

_mixed_specs = [
    ("alternating-3-4-and-4-4", "Alternating 3/4 and 4/4", [M34, M44]),
    ("alternating-4-4-and-3-4", "Alternating 4/4 and 3/4", [M44, M34]),
    ("alternating-6-8-and-3-4", "Alternating 6/8 and 3/4", [M68, M34]),
    ("alternating-5-8-and-7-8", "Alternating 5/8 and 7/8", [M58, M78]),
    ("alternating-7-8-and-4-4", "Alternating 7/8 and 4/4", [M78, M44]),
]
for _id, _name, _meters in _mixed_specs:
    _events: list[EventSpec] = []
    for _bar, _start in enumerate(bar_start_ticks(_meters, 4)):
        _m = _meters[_bar % len(_meters)]
        _grouping_for_bar = [3, _m.numerator - 3] if _m.numerator > 4 and _m.denominator == 8 else [_m.numerator]
        _events += grouped_pulse_events(_m, _grouping_for_bar, bar_offset=_start)
    PATTERNS.append(
        _pattern(
            _id,
            _name,
            f"{_meters[0]} and {_meters[1]} alternate bar by bar, with a fresh time_signature meta event at every "
            "change - the meter itself changes, not just the accent.",
            "mixed-meter",
            _meters,
            4,
            _events,
            tags=["challenge", "mixed-meter"],
            recommended_use="Practice switching meter cleanly at the bar line, not drifting the tempo.",
        )
    )

_additive_2_2_3 = grouped_pulse_events(M78, [2, 2, 3], bar_offset=0) + grouped_pulse_events(M78, [3, 2, 2], bar_offset=M78.bar_ticks)
PATTERNS.append(
    _pattern(
        "additive-2-2-3-and-3-2-2",
        "Additive 2+2+3 and 3+2+2",
        "Two bars of 7/8: bar 1 grouped 2+2+3, bar 2 grouped 3+2+2 - the grouping itself changes, not the meter.",
        "mixed-meter",
        [M78],
        2,
        _additive_2_2_3,
        tags=["challenge", "mixed-meter", "odd-meter"],
        grouping=[2, 2, 3],
        recommended_use="Hear the same 7/8 meter reorganised into a different additive grouping bar to bar.",
    )
)

_combined_54_44 = grouped_pulse_events(M54, [3, 2], bar_offset=0) + grouped_pulse_events(M44, [4], bar_offset=M54.bar_ticks)
PATTERNS.append(
    _pattern(
        "combined-odd-even-5-4-and-4-4",
        "Combined odd/even: 5/4 and 4/4",
        "Bar 1 in 5/4 (grouped 3+2), bar 2 in plain 4/4 - odd and even meters back to back.",
        "mixed-meter",
        [M54, M44],
        2,
        _combined_54_44,
        tags=["challenge", "mixed-meter", "odd-meter"],
    )
)

_combined_78_44 = grouped_pulse_events(M78, [2, 2, 3], bar_offset=0) + grouped_pulse_events(M44, [4], bar_offset=M78.bar_ticks)
PATTERNS.append(
    _pattern(
        "combined-odd-even-7-8-and-4-4",
        "Combined odd/even: 7/8 and 4/4",
        "Bar 1 in 7/8 (grouped 2+2+3), bar 2 in plain 4/4 - odd and even meters back to back.",
        "mixed-meter",
        [M78, M44],
        2,
        _combined_78_44,
        tags=["challenge", "mixed-meter", "odd-meter"],
    )
)

# --- G. Subdivisions and feel ------------------------------------------------------------

PATTERNS.append(
    _pattern(
        "straight-eighths",
        "Straight eighths",
        "A continuous, perfectly even eighth-note grid.",
        "subdivisions",
        [M44],
        4,
        repeat_events_across_bars(
            [EventSpec(i * EIGHTH, instr.HIHAT_CLOSED, instr.VEL_ACCENT if i % 2 == 0 else instr.VEL_LIGHT, instr.CLICK_DURATION_TICKS) for i in range(8)],
            M44.bar_ticks,
            4,
        ),
        tags=["easy", "subdivisions"],
    )
)

PATTERNS.append(
    _pattern(
        "straight-sixteenths",
        "Straight sixteenths",
        "A continuous, perfectly even sixteenth-note grid.",
        "subdivisions",
        [M44],
        4,
        repeat_events_across_bars(
            [
                EventSpec(i * SIXTEENTH, instr.HIHAT_CLOSED, instr.VEL_ACCENT if i % 4 == 0 else instr.VEL_GHOST, instr.CLICK_DURATION_TICKS)
                for i in range(16)
            ],
            M44.bar_ticks,
            4,
        ),
        tags=["easy", "subdivisions"],
    )
)

PATTERNS.append(
    _pattern(
        "eighth-note-triplets",
        "Eighth-note triplets",
        "Continuous eighth-note triplets across every beat.",
        "subdivisions",
        [M44],
        4,
        repeat_events_across_bars(
            [
                EventSpec(b * M44.unit_ticks + i * TRIPLET, instr.HIHAT_CLOSED, instr.VEL_ACCENT if i == 0 else instr.VEL_LIGHT, instr.CLICK_DURATION_TICKS)
                for b in range(4)
                for i in range(3)
            ],
            M44.bar_ticks,
            4,
        ),
        tags=["intermediate", "subdivisions"],
    )
)

PATTERNS.append(
    _pattern(
        "sixteenth-note-triplets",
        "Sixteenth-note triplets",
        "Continuous sixteenth-note triplets (6 per beat) across every beat.",
        "subdivisions",
        [M44],
        4,
        repeat_events_across_bars(
            [
                EventSpec(b * M44.unit_ticks + i * (TRIPLET // 2), instr.HIHAT_CLOSED, instr.VEL_ACCENT if i == 0 else instr.VEL_GHOST, instr.CLICK_DURATION_TICKS)
                for b in range(4)
                for i in range(6)
            ],
            M44.bar_ticks,
            4,
        ),
        tags=["challenge", "subdivisions"],
    )
)

PATTERNS.append(
    _pattern(
        "shuffle-basic",
        "Shuffle basic",
        "Each beat's triplet middle partial is dropped, leaving a long-short (2:1) shuffle pair.",
        "subdivisions",
        [M44],
        4,
        repeat_events_across_bars(
            grouped_pulse_events(M44, [4]) + fill_events([b * M44.unit_ticks + 2 * TRIPLET for b in range(4)], instr.HIHAT_CLOSED, instr.VEL_GHOST),
            M44.bar_ticks,
            4,
        ),
        tags=["easy", "subdivisions", "swing-shuffle"],
        swing_ratio="2:1",
    )
)

PATTERNS.append(
    _pattern(
        "shuffle-sparse",
        "Shuffle sparse",
        "Shuffle feel with the swung note only on beats 2 and 4 - a lighter, sparser shuffle reference.",
        "subdivisions",
        [M44],
        4,
        repeat_events_across_bars(
            grouped_pulse_events(M44, [4]) + fill_events([M44.unit_ticks + 2 * TRIPLET, 3 * M44.unit_ticks + 2 * TRIPLET], instr.HIHAT_CLOSED, instr.VEL_GHOST),
            M44.bar_ticks,
            4,
        ),
        tags=["intermediate", "subdivisions", "swing-shuffle"],
        swing_ratio="2:1",
    )
)

for _id, _name, _percent in (
    ("swing-light-55-percent", "Swing light 55%", 55),
    ("swing-medium-60-percent", "Swing medium 60%", 60),
    ("swing-heavy-66-percent", "Swing heavy 66%", 66),
):
    PATTERNS.append(
        _pattern(
            _id,
            _name,
            f"Unequal eighth pairs at a {_percent}:{100 - _percent} long:short ratio - an actual tick offset, not just a filename label.",
            "subdivisions",
            [M44],
            4,
            repeat_events_across_bars(grouped_pulse_events(M44, [4]) + swing_pair_events(M44, _percent), M44.bar_ticks, 4),
            tags=["intermediate", "subdivisions", "swing-shuffle"],
            swing_ratio=f"{_percent}:{100 - _percent}",
        )
    )

PATTERNS.append(
    _pattern(
        "swing-two-and-four",
        "Swing two and four",
        "60:40 swing feel, with the main click only on beats 2 and 4 (jazz backbeat plus swing).",
        "subdivisions",
        [M44],
        4,
        repeat_events_across_bars(
            [
                EventSpec(1 * M44.unit_ticks, instr.SNARE, instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS),
                EventSpec(3 * M44.unit_ticks, instr.SNARE, instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS),
            ]
            + swing_pair_events(M44, 60),
            M44.bar_ticks,
            4,
        ),
        tags=["intermediate", "subdivisions", "swing-shuffle", "jazz"],
        swing_ratio="60:40",
    )
)

PATTERNS.append(
    _pattern(
        "swung-offbeat-guide",
        "Swung offbeat guide",
        "Only the swung (late) eighth-note offbeats click, at 60:40, no downbeat reference.",
        "subdivisions",
        [M44],
        4,
        repeat_events_across_bars(swing_pair_events(M44, 60, velocity=instr.VEL_NORMAL), M44.bar_ticks, 4),
        tags=["challenge", "subdivisions", "swing-shuffle"],
        swing_ratio="60:40",
    )
)

# --- H. Internal time / challenge --------------------------------------------------------

for _id, _name, _click_bars, _silent_bars in (
    ("one-bar-click-one-bar-silent", "One bar click, one bar silent", 1, 1),
    ("two-bars-click-two-bars-silent", "Two bars click, two bars silent", 2, 2),
    ("four-bars-click-four-bars-silent", "Four bars click, four bars silent", 4, 4),
):
    _cycle_bars = _click_bars + _silent_bars
    _one_bar = grouped_pulse_events(M44, [4])
    _events = []
    for _bar in range(_cycle_bars):
        if _bar < _click_bars:
            _events += [EventSpec(_bar * M44.bar_ticks + e.tick, e.note, e.velocity, e.duration) for e in _one_bar]
    PATTERNS.append(
        _pattern(
            _id,
            _name,
            f"{_click_bars} bar(s) of normal quarter-note click, then {_silent_bars} bar(s) of complete silence, repeating. "
            "The click returns exactly on beat 1 of the next click bar - check for drift there.",
            "internal-time",
            [M44],
            _cycle_bars,
            _events,
            tags=["challenge", "internal-time"],
            recommended_use="Hold tempo through the silent bar(s); the click's return on beat 1 reveals any drift.",
        )
    )

PATTERNS.append(
    _pattern(
        "beat-one-only-four-bars",
        "Beat one only, four bars",
        "Only beat 1 of each of four bars clicks - three silent beats between every click.",
        "internal-time",
        [M44],
        4,
        repeat_events_across_bars([EventSpec(0, instr.SNARE, instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS)], M44.bar_ticks, 4),
        tags=["challenge", "internal-time"],
    )
)

PATTERNS.append(
    _pattern(
        "one-click-every-two-bars",
        "One click every two bars",
        "A single click at the start of every second bar - two full silent bars between clicks.",
        "internal-time",
        [M44],
        4,
        [EventSpec(0, instr.SNARE, instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS), EventSpec(2 * M44.bar_ticks, instr.SNARE, instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS)],
        tags=["challenge", "internal-time"],
        recommended_use="Hold two full bars of internal tempo between reference clicks.",
    )
)

PATTERNS.append(
    _pattern(
        "one-click-every-four-bars",
        "One click every four bars",
        "A single click at the very start of a four-bar cycle - three full silent bars follow.",
        "internal-time",
        [M44],
        4,
        [EventSpec(0, instr.SNARE, instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS)],
        tags=["advanced", "internal-time"],
        recommended_use="Hold three full silent bars of internal tempo before the next reference click.",
    )
)

for _id, _name, _missing_beat in (
    ("missing-beat-2", "Missing beat 2", 1),
    ("missing-beat-3", "Missing beat 3", 2),
    ("missing-beat-4", "Missing beat 4", 3),
):
    PATTERNS.append(
        _pattern(
            _id,
            _name,
            f"Normal quarter-note click with beat {_missing_beat + 1} silently omitted every bar.",
            "internal-time",
            [M44],
            4,
            repeat_events_across_bars(
                [
                    EventSpec(b * M44.unit_ticks, instr.SNARE, instr.VEL_ACCENT if b == 0 else instr.VEL_NORMAL, instr.CLICK_DURATION_TICKS)
                    for b in range(4)
                    if b != _missing_beat
                ],
                M44.bar_ticks,
                4,
            ),
            tags=["intermediate", "internal-time"],
            recommended_use=f"Supply beat {_missing_beat + 1} internally where the click goes silent.",
        )
    )

_rotating_missing_events = []
for _bar in range(4):
    _missing = _bar % 4
    _rotating_missing_events += [
        EventSpec(_bar * M44.bar_ticks + b * M44.unit_ticks, instr.SNARE, instr.VEL_ACCENT if b == 0 else instr.VEL_NORMAL, instr.CLICK_DURATION_TICKS)
        for b in range(4)
        if b != _missing
    ]
PATTERNS.append(
    _pattern(
        "rotating-missing-beat",
        "Rotating missing beat",
        "The silently-omitted beat rotates by one position each bar (bar 1 omits beat 1, bar 2 omits beat 2, ...).",
        "internal-time",
        [M44],
        4,
        _rotating_missing_events,
        tags=["challenge", "internal-time"],
        recommended_use="Track which beat is missing as it rotates bar to bar - a harder internal-time drill.",
    )
)

PATTERNS.append(
    _pattern(
        "barline-only",
        "Barline only",
        "A click only at the very first tick of each bar, nothing else - the sparsest possible bar reference.",
        "internal-time",
        [M44],
        4,
        repeat_events_across_bars([EventSpec(0, instr.SNARE, instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS)], M44.bar_ticks, 4),
        tags=["challenge", "internal-time"],
    )
)

PATTERNS.append(
    _pattern(
        "offbeat-only",
        "Offbeat only",
        "Click only on eighth-note offbeats, never on a quarter-note beat.",
        "internal-time",
        [M44],
        4,
        repeat_events_across_bars(fill_events([b * M44.unit_ticks + EIGHTH for b in range(4)], instr.HIHAT_CLOSED, instr.VEL_NORMAL), M44.bar_ticks, 4),
        tags=["intermediate", "internal-time", "syncopation"],
    )
)


def _deterministic_sparse(meter: Meter, bar_count: int, seed: int, density: float) -> list[EventSpec]:
    rng = random.Random(seed)
    events: list[EventSpec] = []
    for bar in range(bar_count):
        offset = bar * meter.bar_ticks
        events.append(EventSpec(offset, instr.SNARE, instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS))
        units = meter.numerator if meter.denominator in (4,) else meter.numerator
        for unit in range(1, units):
            if rng.random() < density:
                events.append(EventSpec(offset + unit * meter.unit_ticks, instr.SNARE, instr.VEL_NORMAL, instr.CLICK_DURATION_TICKS))
    return events


PATTERNS.append(
    _pattern(
        "random-but-deterministic-sparse-4-4",
        "Random but deterministic sparse 4/4",
        "A fixed-seed pseudo-random sparse click pattern in 4/4 - always the same pattern, unpredictable to the ear.",
        "internal-time",
        [M44],
        4,
        _deterministic_sparse(M44, 4, seed=44, density=0.35),
        tags=["challenge", "internal-time"],
        recommended_use="Track tempo through irregular, unpredictable (but reproducible) click placement.",
    )
)

PATTERNS.append(
    _pattern(
        "random-but-deterministic-sparse-7-8",
        "Random but deterministic sparse 7/8",
        "A fixed-seed pseudo-random sparse click pattern in 7/8 - always the same pattern, unpredictable to the ear.",
        "internal-time",
        [M78],
        4,
        _deterministic_sparse(M78, 4, seed=78, density=0.35),
        tags=["advanced", "internal-time", "odd-meter"],
        recommended_use="Track an odd meter through irregular, unpredictable (but reproducible) click placement.",
    )
)


EDUCATIONAL_PATTERNS: list[PatternSpec] = PATTERNS
