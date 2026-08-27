"""Afro-Cuban, Afro-diasporic/Brazilian, and Middle Eastern rhythmic-guide patterns.

These are introductory pulse/clave/dum-tak guides, not substitutes for learning the traditions
and performance practice - see PLAN.md and every pattern's `source_context`/`simplification_note`.
This project has no dedicated claves voice (see rhythm_library/instruments.py), so clave-family
patterns use Woodblock/Rimshot/Sidestick as the closest available timbres.
"""

from __future__ import annotations

from rhythm_library import instruments as instr
from rhythm_library.model import EventSpec, Meter, PatternSpec, total_ticks
from rhythm_library.patterns.helpers import (
    EIGHTH,
    SIXTEENTH,
    bar_markers,
    clave_events,
    fill_events,
    grouped_pulse_events,
    repeat_events_across_bars,
)

M44 = Meter(4, 4)
M68 = Meter(6, 8)
M98 = Meter(9, 8)
M128 = Meter(12, 8)
M24 = Meter(2, 4)
M108 = Meter(10, 8)

BPM_DEFAULT = 120.0

SON_RUMBA_NOTE = "Son and rumba clave differ only in the 3-side's final note: son lands it on beat 4, rumba delays it to the 'e' of beat 4."
DUM_TAK_DISCLAIMER = (
    "Grouping and basic dum/tak placement here follow commonly cited introductory descriptions of this "
    "rhythmic cycle; exact stroke pattern, ornamentation and regional practice vary - treat this as a "
    "schematic starting point, not a transcription of a specific performance tradition."
)
AFRO_CUBAN_CONTEXT = (
    "Introductory Afro-Cuban pulse/clave guide, not a substitute for learning the tradition and its "
    "performance practice from qualified sources."
)
AFRO_DIASPORIC_CONTEXT = (
    "Introductory Afro-diasporic/Brazilian guide - a simplified pulse/bell reference, not a claim to "
    "represent an entire genre's rhythmic vocabulary."
)


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
    grid_ticks: int = EIGHTH,
    tags: list[str] = (),
    recommended_use: str = "",
    grouping: list[int] | None = None,
    source_context: str = "",
    simplification_note: str = "",
    click_language: str = "",
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
        tags=list(tags) + ["cultural"],
        sections=bar_markers(bar_count, meters[0].bar_ticks),
        recommended_use=recommended_use,
        grouping=grouping,
        source_context=source_context,
        simplification_note=simplification_note,
        click_language=click_language,
    )


PATTERNS: list[PatternSpec] = []

# --- A. Afro-Cuban -----------------------------------------------------------------------

for _id, _name, _variant, _direction in (
    ("son-clave-3-2", "Son clave 3-2", "son", "3-2"),
    ("son-clave-2-3", "Son clave 2-3", "son", "2-3"),
    ("rumba-clave-3-2", "Rumba clave 3-2", "rumba", "3-2"),
    ("rumba-clave-2-3", "Rumba clave 2-3", "rumba", "2-3"),
):
    PATTERNS.append(
        _pattern(
            _id,
            _name,
            f"Two-bar {_variant} clave, {_direction} direction. {SON_RUMBA_NOTE}",
            "afro-cuban",
            [M44],
            2,
            clave_events(_variant, _direction),
            tags=["intermediate", "afro-cuban", "clave", f"clave-{_direction}"],
            source_context=AFRO_CUBAN_CONTEXT,
            simplification_note="Played here on a single Woodblock voice, standing in for a real claves pair.",
            recommended_use="Internalise the clave's asymmetric 3-side/2-side shape before playing along with a real groove.",
        )
    )

_cascara_events = []
for _bar in range(2):
    _offset = _bar * M44.bar_ticks
    _cascara_events += fill_events([i * EIGHTH for i in range(8)], instr.SIDESTICK, instr.VEL_LIGHT, bar_offset=_offset)
_cascara_accent_ticks = [t.tick for t in clave_events("son", "3-2")]
_cascara_events = [
    EventSpec(e.tick, e.note, instr.VEL_ACCENT if e.tick in _cascara_accent_ticks else e.velocity, e.duration)
    for e in _cascara_events
]
PATTERNS.append(
    _pattern(
        "cascara-basic",
        "Cascara basic",
        "A continuous eighth-note shell pattern (Sidestick) with the son clave 3-2 accents emphasised on top of it.",
        "afro-cuban",
        [M44],
        2,
        _cascara_events,
        tags=["intermediate", "afro-cuban"],
        source_context=AFRO_CUBAN_CONTEXT,
        simplification_note="A simplified single-voice cascara; a real cascara often interacts with clave, bell and kick.",
        recommended_use="Practice the continuous cascara pulse while feeling the clave accents inside it.",
    )
)

PATTERNS.append(
    _pattern(
        "bossa-nova-clave-guide",
        "Bossa nova clave guide",
        "The bossa nova's rhythmic skeleton follows the same son-clave shape (3-2), one of Afro-Cuban music's "
        "clearest exports into Brazilian popular music.",
        "afro-cuban",
        [M44],
        2,
        clave_events("son", "3-2"),
        tags=["intermediate", "afro-cuban", "clave"],
        source_context=AFRO_CUBAN_CONTEXT,
        simplification_note="Presented here as the same clave shape as son-clave-3-2; bossa performance practice varies its feel and instrumentation considerably.",
    )
)

_songo_events = [
    EventSpec(0, instr.KICK, instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS),
    EventSpec(M44.unit_ticks + EIGHTH, instr.KICK, instr.VEL_NORMAL, instr.CLICK_DURATION_TICKS),
    EventSpec(2 * M44.unit_ticks, instr.RIMSHOT, instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS),
    EventSpec(3 * M44.unit_ticks + EIGHTH, instr.RIMSHOT, instr.VEL_NORMAL, instr.CLICK_DURATION_TICKS),
]
PATTERNS.append(
    _pattern(
        "songo-style-pulse-guide",
        "Songo-style pulse guide",
        "A sparse two-voice (kick + rimshot) pulse guide referencing songo's characteristic off-the-beat kick "
        "anchors, not a full songo drum part.",
        "afro-cuban",
        [M44],
        1,
        _songo_events,
        tags=["challenge", "afro-cuban"],
        source_context=AFRO_CUBAN_CONTEXT,
        simplification_note="A minimal two-voice pulse guide; real songo interlocks kick, snare, bell, hi-hat and toms.",
    )
)

_mozambique_events = fill_events([0, 3 * EIGHTH, 5 * EIGHTH], instr.WOODBLOCK, instr.VEL_ACCENT) + fill_events(
    [EIGHTH, 2 * EIGHTH, 6 * EIGHTH, 7 * EIGHTH], instr.WOODBLOCK, instr.VEL_LIGHT
)
PATTERNS.append(
    _pattern(
        "mozambique-style-pulse-guide",
        "Mozambique-style pulse guide",
        "A single-voice bell-style pulse guide referencing the mozambique's characteristic accent placement, "
        "not a full mozambique ensemble part.",
        "afro-cuban",
        [M44],
        1,
        _mozambique_events,
        tags=["challenge", "afro-cuban"],
        source_context=AFRO_CUBAN_CONTEXT,
        simplification_note="A minimal single-voice pulse guide; real mozambique interlocks multiple bells, congas and bass drum.",
    )
)

# --- B. Afro-diasporic / Brazilian ---------------------------------------------------------

PATTERNS.append(
    _pattern(
        "bossa-nova-two-bar-guide",
        "Bossa nova two-bar guide",
        "The same two-bar son-clave shape presented as a bossa nova rhythm guide (see bossa-nova-clave-guide).",
        "afro-diasporic",
        [M44],
        2,
        clave_events("son", "3-2", note=instr.SIDESTICK),
        tags=["intermediate", "afro-diasporic"],
        source_context=AFRO_DIASPORIC_CONTEXT,
        simplification_note="A pulse guide, not a transcription of a specific bossa nova recording or arranger's part.",
    )
)

_surdo_events = repeat_events_across_bars(
    [
        EventSpec(0, instr.KICK, instr.VEL_LIGHT, instr.CLICK_DURATION_TICKS),
        EventSpec(2 * M44.unit_ticks, instr.KICK, instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS),
    ],
    M44.bar_ticks,
    2,
)
PATTERNS.append(
    _pattern(
        "samba-surdo-pulse-guide",
        "Samba surdo pulse guide",
        "A bass-drum surdo guide: a low accent on beat 3 (the samba surdo's characteristic strong beat) with a "
        "light pulse on beat 1.",
        "afro-diasporic",
        [M44],
        2,
        _surdo_events,
        tags=["intermediate", "afro-diasporic"],
        source_context=AFRO_DIASPORIC_CONTEXT,
        simplification_note="A single-voice surdo guide; a real bateria layers multiple surdo pitches plus caixa, tamborim and agogo.",
    )
)

PATTERNS.append(
    _pattern(
        "samba-clave-like-guide",
        "Samba clave-like guide",
        "A clave-shaped reference pattern used as a learning aid for samba phrasing - carefully NOT labelled "
        "\"the samba clave\", since samba is not organised around a single fixed clave the way Afro-Cuban music is.",
        "afro-diasporic",
        [M44],
        2,
        clave_events("son", "2-3", note=instr.WOODBLOCK),
        tags=["challenge", "afro-diasporic"],
        source_context=AFRO_DIASPORIC_CONTEXT,
        simplification_note="A learning-guide pattern only; samba's rhythmic identity comes from the interlocking bateria, not a single clave line.",
    )
)

_bell_12_8 = fill_events([i * EIGHTH for i in (0, 3, 5, 6, 8, 10)], instr.WOODBLOCK, instr.VEL_ACCENT)
PATTERNS.append(
    _pattern(
        "12-8-afro-bell-basic",
        "12/8 Afro bell basic",
        "The widely documented 12/8 \"standard pattern\" bell rhythm found across West African and "
        "Afro-diasporic musics.",
        "afro-diasporic",
        [M128],
        2,
        repeat_events_across_bars(_bell_12_8, M128.bar_ticks, 2),
        tags=["intermediate", "afro-diasporic"],
        source_context=AFRO_DIASPORIC_CONTEXT,
        recommended_use="Learn the 'standard pattern' bell shape that underlies many West African and Afro-diasporic grooves.",
    )
)

_bell_6_8 = fill_events([0, 2 * EIGHTH, 3 * EIGHTH, 5 * EIGHTH], instr.WOODBLOCK, instr.VEL_ACCENT)
PATTERNS.append(
    _pattern(
        "6-8-afro-bell-basic",
        "6/8 Afro bell basic",
        "A commonly cited 6/8 excerpt of the standard-pattern bell rhythm.",
        "afro-diasporic",
        [M68],
        2,
        repeat_events_across_bars(_bell_6_8, M68.bar_ticks, 2),
        tags=["intermediate", "afro-diasporic"],
        source_context=AFRO_DIASPORIC_CONTEXT,
    )
)

_tresillo_events = fill_events([0, 3 * EIGHTH, 6 * EIGHTH], instr.WOODBLOCK, instr.VEL_ACCENT)
PATTERNS.append(
    _pattern(
        "tresillo-3-3-2",
        "Tresillo 3+3+2",
        "The tresillo: three onsets grouped 3+3+2 in eighth notes over one 4/4 bar - the clave's 3-side on its own.",
        "afro-diasporic",
        [M44],
        2,
        repeat_events_across_bars(_tresillo_events, M44.bar_ticks, 2),
        tags=["easy", "afro-diasporic"],
        grouping=[3, 3, 2],
        source_context=AFRO_DIASPORIC_CONTEXT,
    )
)

_cinquillo_events = fill_events([0, 2 * EIGHTH, 3 * EIGHTH, 5 * EIGHTH, 6 * EIGHTH], instr.WOODBLOCK, instr.VEL_ACCENT)
PATTERNS.append(
    _pattern(
        "cinquillo-guide",
        "Cinquillo guide",
        "The cinquillo: a five-onset pattern over one bar, the tresillo's three hits filled in with two more.",
        "afro-diasporic",
        [M44],
        2,
        repeat_events_across_bars(_cinquillo_events, M44.bar_ticks, 2),
        tags=["intermediate", "afro-diasporic"],
        source_context=AFRO_DIASPORIC_CONTEXT,
    )
)

_habanera_events = fill_events([0, 3 * SIXTEENTH, 4 * SIXTEENTH, 6 * SIXTEENTH], instr.WOODBLOCK, instr.VEL_ACCENT)
PATTERNS.append(
    _pattern(
        "habanera-guide",
        "Habanera guide",
        "The habanera figure: dotted-eighth, sixteenth, then two even eighths, repeated each half-bar.",
        "afro-diasporic",
        [M44],
        2,
        repeat_events_across_bars(_habanera_events, M44.bar_ticks, 2),
        tags=["intermediate", "afro-diasporic"],
        source_context=AFRO_DIASPORIC_CONTEXT,
    )
)

# --- C. Middle Eastern / Arabic rhythmic cycles --------------------------------------------

_maqsum = [
    EventSpec(tick, instr.DUM if tick in (0, 4 * EIGHTH) else instr.TAK, instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS)
    for tick in (0, EIGHTH, 3 * EIGHTH, 4 * EIGHTH, 6 * EIGHTH)
]
PATTERNS.append(
    _pattern(
        "maqsum-4-4-guide",
        "Maqsum 4/4 guide",
        "Dum-Tak-rest-Tak-Dum-rest-Tak-rest: a commonly taught basic maqsum pattern.",
        "middle-eastern",
        [M44],
        2,
        repeat_events_across_bars(_maqsum, M44.bar_ticks, 2),
        tags=["intermediate", "middle-eastern"],
        click_language="D T - T D - T -",
        source_context=DUM_TAK_DISCLAIMER,
    )
)

_baladi = [
    EventSpec(0, instr.DUM, instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS),
    EventSpec(EIGHTH, instr.TAK, instr.VEL_NORMAL, instr.CLICK_DURATION_TICKS),
    EventSpec(3 * EIGHTH, instr.TAK, instr.VEL_NORMAL, instr.CLICK_DURATION_TICKS),
    EventSpec(4 * EIGHTH, instr.DUM, instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS),
    EventSpec(5 * EIGHTH, instr.DUM, instr.VEL_SECONDARY, instr.CLICK_DURATION_TICKS),
    EventSpec(6 * EIGHTH, instr.TAK, instr.VEL_NORMAL, instr.CLICK_DURATION_TICKS),
]
PATTERNS.append(
    _pattern(
        "baladi-4-4-guide",
        "Baladi 4/4 guide",
        "A maqsum-like cycle with an extra dum, commonly cited as characteristic of baladi's heavier feel.",
        "middle-eastern",
        [M44],
        2,
        repeat_events_across_bars(_baladi, M44.bar_ticks, 2),
        tags=["intermediate", "middle-eastern"],
        click_language="D T - T D D T -",
        source_context=DUM_TAK_DISCLAIMER,
    )
)

_saidi = [
    EventSpec(0, instr.DUM, instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS),
    EventSpec(EIGHTH, instr.DUM, instr.VEL_SECONDARY, instr.CLICK_DURATION_TICKS),
    EventSpec(2 * EIGHTH, instr.TAK, instr.VEL_NORMAL, instr.CLICK_DURATION_TICKS),
    EventSpec(4 * EIGHTH, instr.DUM, instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS),
    EventSpec(5 * EIGHTH, instr.TAK, instr.VEL_NORMAL, instr.CLICK_DURATION_TICKS),
    EventSpec(6 * EIGHTH, instr.TAK, instr.VEL_NORMAL, instr.CLICK_DURATION_TICKS),
]
PATTERNS.append(
    _pattern(
        "saidi-4-4-guide",
        "Saidi 4/4 guide",
        "Distinguished from maqsum/baladi by two dums placed close together near the top of the cycle, "
        "commonly cited as saidi's characteristic feature.",
        "middle-eastern",
        [M44],
        2,
        repeat_events_across_bars(_saidi, M44.bar_ticks, 2),
        tags=["intermediate", "middle-eastern"],
        click_language="D D T - D T T -",
        source_context=DUM_TAK_DISCLAIMER,
    )
)

_malfuf = [
    EventSpec(0, instr.DUM, instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS),
    EventSpec(2 * EIGHTH, instr.TAK, instr.VEL_NORMAL, instr.CLICK_DURATION_TICKS),
]
PATTERNS.append(
    _pattern(
        "malfuf-2-4-guide",
        "Malfuf 2/4 guide",
        "A fast 2/4 dum-tak alternation, characteristic of malfuf's quick duple feel.",
        "middle-eastern",
        [M24],
        4,
        repeat_events_across_bars(_malfuf, M24.bar_ticks, 4),
        bpm=140.0,
        tags=["intermediate", "middle-eastern"],
        click_language="D - T -",
        source_context=DUM_TAK_DISCLAIMER,
    )
)

_wahda = [
    EventSpec(0, instr.DUM, instr.VEL_ACCENT, instr.CLICK_DURATION_TICKS),
    EventSpec(3 * EIGHTH, instr.TAK, instr.VEL_NORMAL, instr.CLICK_DURATION_TICKS),
    EventSpec(6 * EIGHTH, instr.TAK, instr.VEL_NORMAL, instr.CLICK_DURATION_TICKS),
]
PATTERNS.append(
    _pattern(
        "wahda-4-4-slow-guide",
        "Wahda 4/4 slow guide",
        "A slow, spacious dum-tak cycle - \"wahda\" (\"one\") reflects its simple, unhurried feel.",
        "middle-eastern",
        [M44],
        2,
        repeat_events_across_bars(_wahda, M44.bar_ticks, 2),
        bpm=70.0,
        tags=["easy", "middle-eastern"],
        click_language="D - - T - - T -",
        source_context=DUM_TAK_DISCLAIMER,
    )
)

PATTERNS.append(
    _pattern(
        "samaai-thaqil-10-8-guide",
        "Samaai thaqil 10/8 guide",
        "A 10/8 cycle grouped 3+2+2+3, with dum on each group's first unit and tak elsewhere - a schematic "
        "reference, not a transcription of a specific performance.",
        "middle-eastern",
        [M108],
        2,
        repeat_events_across_bars(
            grouped_pulse_events(M108, [3, 2, 2, 3], accent_note=instr.DUM, normal_note=instr.TAK), M108.bar_ticks, 2
        ),
        bpm=80.0,
        tags=["advanced", "middle-eastern", "odd-meter"],
        grouping=[3, 2, 2, 3],
        source_context=DUM_TAK_DISCLAIMER,
    )
)

PATTERNS.append(
    _pattern(
        "karsilama-9-8-two-two-two-three",
        "Karsilama 9/8: 2+2+2+3",
        "9/8 grouped 2+2+2+3, with dum on each group's first unit and tak elsewhere.",
        "middle-eastern",
        [M98],
        4,
        repeat_events_across_bars(
            grouped_pulse_events(M98, [2, 2, 2, 3], accent_note=instr.DUM, normal_note=instr.TAK), M98.bar_ticks, 4
        ),
        tags=["advanced", "middle-eastern", "odd-meter"],
        grouping=[2, 2, 2, 3],
        source_context=DUM_TAK_DISCLAIMER,
    )
)

PATTERNS.append(
    _pattern(
        "aksak-9-8-two-two-two-three",
        "Aksak 9/8: 2+2+2+3",
        "The Turkish/Balkan \"aksak\" (limping) additive rhythm, 9/8 grouped 2+2+2+3.",
        "middle-eastern",
        [M98],
        4,
        repeat_events_across_bars(
            grouped_pulse_events(M98, [2, 2, 2, 3], accent_note=instr.DUM, normal_note=instr.TAK), M98.bar_ticks, 4
        ),
        tags=["advanced", "middle-eastern", "odd-meter"],
        grouping=[2, 2, 2, 3],
        source_context=DUM_TAK_DISCLAIMER,
    )
)


CULTURAL_PATTERNS: list[PatternSpec] = PATTERNS
