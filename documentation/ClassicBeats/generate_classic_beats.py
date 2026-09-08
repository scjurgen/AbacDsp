#!/usr/bin/env python3
"""Generates original, copyright-free full-drumset MIDI grooves for classic genres.

Every groove here is an original composition, informed only by the aggregate numbers in
genre_profiles.md (typical backbeat placement, hihat density, ghost-note rate, swing/shuffle
prevalence, fill density) - never a transcription of any reference recording or pack. See
.claude/classic-beats-full-kit.md for the full design rationale.

Output is pure Python stdlib, no dependencies (hand-written Standard MIDI File writer, format 0,
channel 9/GM percussion), matching documentation/DrumKit808's zero-dependency convention.

Usage:
    generate_classic_beats.py [--out DIR]

Writes <out>/<Genre>/<slug>_v1.mid + _v2.mid (plus .json sidecars) for every GROOVES entry.
Output stays local to this folder (generated/, gitignored) - copying reviewed grooves into
MidiDrums/<Genre>/ is a separate, later step, not done by this script.
"""
from __future__ import annotations

import argparse
import json
import random
import struct
from dataclasses import dataclass, field
from functools import partial
from pathlib import Path

# --- Note palette (MidiDrums/README.md / src/includes/Sampler/GrooveNoteMap.h) --------------

KICK = 36
SNARE = 38
RIMSHOT = 37
SIDESTICK = 71
HIHAT_CLOSED = 42
HIHAT_OPEN = 49
CRASH = 27
RIDE = 51
RIDE_BELL = 53
TOM1 = 43
TOM2 = 45
TOM3 = 47
TOM_LOW = 41
WOODBLOCK = 56
TIMBALE1 = 66
TIMBALE2 = 67
TIMBALE3 = 68
TIMBALE4 = 69

TICKS_PER_QUARTER = 480
SIXTEENTH = TICKS_PER_QUARTER // 4
TRIPLET_8TH = TICKS_PER_QUARTER // 3
BAR_TICKS_44 = TICKS_PER_QUARTER * 4

VELOCITY = {"X": 112, "x": 88, "g": 42, "o": 88, "O": 112}
DEFAULT_DURATION = 40
SUSTAIN_DURATION = 220  # crash, ride, open hihat


def beat16(beat: int, sixteenth: int = 0) -> int:
    """Step index (0-15) for a 16th-note grid: beat is 0-based (0=beat 1)."""
    return beat * 4 + sixteenth


def beat_triplet(beat: int, third: int = 0) -> int:
    """Step index (0-11) for a triplet-8th grid (3 subdivisions per beat)."""
    return beat * 3 + third


# --- Bar assembly ------------------------------------------------------------------------

Hit = tuple[int, str]  # (step index, symbol: X/x/g/o/O)
NoteEvent = tuple[int, int, int, int]  # tick, note, velocity, duration


def bar_from_hits(hits: dict[int, list[Hit]], *, step_ticks: int = SIXTEENTH) -> list[NoteEvent]:
    events: list[NoteEvent] = []
    for note, note_hits in hits.items():
        for step, symbol in note_hits:
            tick = step * step_ticks
            duration = SUSTAIN_DURATION if symbol in ("o", "O") or note in (CRASH, RIDE, RIDE_BELL) else DEFAULT_DURATION
            events.append((tick, note, VELOCITY[symbol], duration))
    return events


def crash_hit(*, note: int = CRASH) -> list[NoteEvent]:
    return [(0, note, VELOCITY["X"], SUSTAIN_DURATION)]


# --- Fill devices ----------------------------------------------------------------------------
# Real drummers reach for a different device depending on genre and mood - see
# .claude/classic-beats-full-kit.md for the sourcing behind each choice. Every device is
# parameterized (cutoff_tick, fill_beats) rather than fixed content, so nothing here is a
# transcription of anything: it is an original phrase built from a named, well-documented
# technique (tom cascade, ghost-note jabs, triplet hihat, snare comping, ...).


def tom_cascade_ending(cutoff_tick: int, fill_beats: int, *, toms: tuple[int, ...] = (TOM1, TOM2, TOM3, TOM_LOW)) -> list[NoteEvent]:
    """High-to-low tom run, one hit per 16th, accenting the last hit - the classic
    "cascading" fill that lands on the next downbeat's crash."""
    count = fill_beats * 4
    events: list[NoteEvent] = []
    for i in range(count):
        symbol = "X" if i == count - 1 else "x"
        events.append(((cutoff_tick + i * SIXTEENTH), toms[i % len(toms)], VELOCITY[symbol], DEFAULT_DURATION))
    return events


def ghost_snare_ending(cutoff_tick: int, fill_beats: int) -> list[NoteEvent]:
    """Syncopated ghost/accent snare jabs (funk/soul's signature device) rather than a tom run."""
    count = fill_beats * 4
    events: list[NoteEvent] = []
    for i in range(count):
        symbol = "X" if i == count - 1 else ("g" if i % 2 == 0 else "x")
        events.append(((cutoff_tick + i * SIXTEENTH), SNARE, VELOCITY[symbol], DEFAULT_DURATION))
    return events


def hihat_triplet_ending(cutoff_tick: int, fill_beats: int) -> list[NoteEvent]:
    """Laid-back triplet hihat with a sidestick accent on the last hit, matching how a one-drop
    reggae fill is actually built (hihat + incidental sidestick, not toms)."""
    count = fill_beats * 3
    events: list[NoteEvent] = []
    for i in range(count):
        tick = cutoff_tick + i * TRIPLET_8TH
        if i == count - 1:
            events.append((tick, SIDESTICK, VELOCITY["X"], DEFAULT_DURATION))
        else:
            events.append((tick, HIHAT_CLOSED, VELOCITY["x"], DEFAULT_DURATION))
    return events


def snare_comping_ending(cutoff_tick: int, fill_beats: int) -> list[NoteEvent]:
    """Light syncopated snare comping jabs on the triplet grid - a jazz-comping gesture, not a
    dense fill; the ride carries the actual landing, so this stays understated."""
    count = fill_beats * 3
    events: list[NoteEvent] = []
    for i in range(count):
        symbol = "x" if i == count - 1 else "g"
        events.append((cutoff_tick + i * TRIPLET_8TH, SNARE, VELOCITY[symbol], DEFAULT_DURATION))
    return events


def tom_kick_punch_ending(cutoff_tick: int, fill_beats: int) -> list[NoteEvent]:
    """A single low-tom-then-kick punch - boom-bap's "kick sneaking in when you don't expect it"
    trademark, not a melodic run."""
    return [(cutoff_tick, TOM_LOW, VELOCITY["X"], DEFAULT_DURATION), (cutoff_tick + 2 * SIXTEENTH, KICK, VELOCITY["X"], DEFAULT_DURATION)]


def snare_roll_flourish_ending(cutoff_tick: int, fill_beats: int) -> list[NoteEvent]:
    """A short snare roll with velocity ramping up across the window - the classic crescendo-roll
    buildup, for a groove whose normal bar is already a dense 16th-note hihat roll."""
    count = fill_beats * 4
    events: list[NoteEvent] = []
    for i in range(count):
        velocity = round(VELOCITY["g"] + (VELOCITY["X"] - VELOCITY["g"]) * i / max(count - 1, 1))
        events.append((cutoff_tick + i * SIXTEENTH, SNARE, velocity, DEFAULT_DURATION))
    return events


def make_fill_bar(normal_bar: list[NoteEvent], *, fill_beats: int, ending) -> list[NoteEvent]:
    """The groove's own normal bar, kept as-is up to the last `fill_beats` beat(s), then closed
    by `ending` - so the fill grows out of the groove instead of replacing it wholesale."""
    cutoff_tick = BAR_TICKS_44 - fill_beats * TICKS_PER_QUARTER
    kept = [event for event in normal_bar if event[0] < cutoff_tick]
    return kept + ending(cutoff_tick, fill_beats)


def assemble(bars: list[list[NoteEvent]], *, bar_ticks: int = BAR_TICKS_44) -> list[NoteEvent]:
    events: list[NoteEvent] = []
    for bar_index, bar_events in enumerate(bars):
        offset = bar_index * bar_ticks
        events.extend((offset + tick, note, vel, dur) for tick, note, vel, dur in bar_events)
    return sorted(events, key=lambda e: e[0])


def humanize(events: list[NoteEvent], seed: str, *, timing_jitter: int = 6, velocity_jitter: int = 6) -> list[NoteEvent]:
    rng = random.Random(seed)
    humanized = []
    for tick, note, vel, dur in events:
        jittered_tick = max(0, tick + rng.randint(-timing_jitter, timing_jitter))
        jittered_vel = max(1, min(127, vel + rng.randint(-velocity_jitter, velocity_jitter)))
        humanized.append((jittered_tick, note, jittered_vel, dur))
    return sorted(humanized, key=lambda e: e[0])


# --- Standard MIDI File writer (format 0, channel 9, hand-written - no mido dependency) -----

CHANNEL = 9


def _vlq(value: int) -> bytes:
    chunks = [value & 0x7F]
    value >>= 7
    while value:
        chunks.append((value & 0x7F) | 0x80)
        value >>= 7
    return bytes(reversed(chunks))


def _track_events(bpm: float, numerator: int, denominator: int, notes: list[NoteEvent], loop_length_ticks: int) -> list[tuple[int, int, bytes]]:
    """Returns (tick, rank, bytes) triples; rank breaks same-tick ties (meta before note-off before note-on).
    The end-of-track meta event's tick is pinned to loop_length_ticks (not the last note's own tick) so a
    consumer that derives loop length from the track's final event gets the full bar count, matching
    documentation/Metronome/rhythm_library/midi_writer.py's own end_of_track placement.
    """
    microseconds_per_quarter = round(60_000_000 / bpm)
    denominator_exponent = denominator.bit_length() - 1
    triples: list[tuple[int, int, bytes]] = [
        (0, 0, b"\xff\x58\x04" + bytes([numerator, denominator_exponent, 24, 8])),
        (0, 1, b"\xff\x51\x03" + microseconds_per_quarter.to_bytes(3, "big")),
    ]
    last_tick = 0
    for tick, note, velocity, duration in notes:
        triples.append((tick, 2, bytes([0x90 | CHANNEL, note, velocity])))
        triples.append((tick + duration, 3, bytes([0x80 | CHANNEL, note, 0])))
        last_tick = max(last_tick, tick + duration)
    assert last_tick <= loop_length_ticks, f"last note-off at {last_tick} overruns loop length {loop_length_ticks}"
    triples.append((loop_length_ticks, 4, b"\xff\x2f\x00"))
    return triples


def write_smf(path: Path, *, bpm: float, numerator: int, denominator: int, notes: list[NoteEvent], loop_length_ticks: int) -> None:
    triples = sorted(_track_events(bpm, numerator, denominator, notes, loop_length_ticks), key=lambda t: (t[0], t[1]))
    track_data = bytearray()
    previous_tick = 0
    for tick, _rank, event_bytes in triples:
        track_data += _vlq(tick - previous_tick)
        track_data += event_bytes
        previous_tick = tick

    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("wb") as f:
        f.write(b"MThd" + struct.pack(">IHHH", 6, 0, 1, TICKS_PER_QUARTER))
        f.write(b"MTrk" + struct.pack(">I", len(track_data)) + bytes(track_data))


# --- Groove definitions ---------------------------------------------------------------------


@dataclass
class Groove:
    genre: str
    slug: str
    display_name: str
    description: str
    bpm: float
    feel: str  # "even" | "swing" | "shuffle" (MidiDrums/README.md JSON convention)
    time_signature: str = "4/4"
    dominant_sounds: list[str] = field(default_factory=list)
    varies_by: str = "hihat"
    section: str = "verse"
    bars: list[list[NoteEvent]] = field(default_factory=list)


GROOVES: list[Groove] = []


def _add(genre, slug, display_name, description, bpm, feel, normal_bar, *, fill_beats=1, fill_ending=tom_cascade_ending, landing_note=CRASH, bar_count=4, dominant_sounds=("snare", "hihat"), varies_by="hihat", time_signature="4/4") -> None:
    first_bar = normal_bar + (crash_hit(note=landing_note) if landing_note is not None else [])
    fill_bar = make_fill_bar(normal_bar, fill_beats=fill_beats, ending=fill_ending)
    bars = [first_bar] + [normal_bar] * (bar_count - 2) + [fill_bar]
    GROOVES.append(
        Groove(
            genre=genre,
            slug=slug,
            display_name=display_name,
            description=description,
            bpm=bpm,
            feel=feel,
            time_signature=time_signature,
            dominant_sounds=list(dominant_sounds),
            varies_by=varies_by,
            bars=bars,
        )
    )


# --- Rock: straight backbeat + shuffle backbeat (profile: straight 83%, swing 17%) ----------

_rock_straight_bar = bar_from_hits(
    {
        KICK: [(beat16(0), "X"), (beat16(1, 2), "x"), (beat16(2), "x")],
        SNARE: [(beat16(1), "X"), (beat16(3), "X")],
        HIHAT_CLOSED: [(beat16(b, s), "x" if s == 0 else "x") for b in range(4) for s in (0, 2)],
    }
)
_add("Rock", "driving-straight-backbeat", "Driving straight backbeat", "Classic straight-8th rock backbeat with a syncopated kick, closed by a tom cascade into the next downbeat's crash.", 128, "even", _rock_straight_bar)

_rock_shuffle_bar = bar_from_hits(
    {
        KICK: [(beat_triplet(0), "X"), (beat_triplet(2, 2), "x")],
        SNARE: [(beat_triplet(1), "X"), (beat_triplet(3), "X")],
        HIHAT_CLOSED: [(beat_triplet(b, t), "x") for b in range(4) for t in (0, 2)],
    },
    step_ticks=TRIPLET_8TH,
)
_add("Rock", "shuffle-backbeat", "Shuffle backbeat", "Triplet-swung rock shuffle groove, a secondary feel this genre's reference profile shows alongside the dominant straight feel, closed by a tom cascade into the next downbeat's crash.", 116, "shuffle", _rock_shuffle_bar)

# --- Blues: 12/8 shuffle + straight blues-rock (no feel signal in profile; genre-defining shuffle used on general knowledge) ---

_blues_shuffle_bar = bar_from_hits(
    {
        KICK: [(0, "X"), (6, "x")],
        SNARE: [(3, "X"), (9, "X")],
        HIHAT_CLOSED: [(i, "x") for i in range(12)],
    },
    step_ticks=TRIPLET_8TH,
)
_add("Blues", "shuffle-blues", "Shuffle blues", "The classic triplet-swung blues shuffle (the 4/4 notation of what's often written as 12/8): kick-and-snare pulse under a full triplet hihat, with a two-beat tom-cascade turnaround - this genre's reference profile shows notably more last-bar activity than the others.", 84, "shuffle", _blues_shuffle_bar, fill_beats=2)

_blues_straight_bar = bar_from_hits(
    {
        KICK: [(beat16(0), "X"), (beat16(2, 2), "x")],
        SNARE: [(beat16(1), "X"), (beat16(3), "X", ), (beat16(3, 2), "g")],
        HIHAT_CLOSED: [(beat16(b, s), "x") for b in range(4) for s in (0, 2)],
    }
)
_add("Blues", "straight-blues-rock", "Straight blues-rock backbeat", "A straighter blues-rock backbeat with a snare ghost note and a two-beat tom-cascade turnaround, for verses that don't want the full triplet shuffle.", 100, "even", _blues_straight_bar, fill_beats=2)

# --- Jazz Swing: ride-driven swing groove + lighter comping variant (genre identity: keep both grooves in swing feel) ---

_jazz_ride_bar = bar_from_hits(
    {
        RIDE: [(beat_triplet(b), "x" if b % 2 else "X") for b in range(4)] + [(beat_triplet(b, 2), "x") for b in range(4)],
        HIHAT_CLOSED: [(beat_triplet(1), "x"), (beat_triplet(3), "x")],
        SNARE: [(beat_triplet(1), "g"), (beat_triplet(3, 1), "g")],
        KICK: [(beat_triplet(0), "x"), (beat_triplet(2), "x")],
    },
    step_ticks=TRIPLET_8TH,
)
_add("Jazz Swing", "ride-spang-a-lang", "Ride spang-a-lang groove", "The classic ride-cymbal swing pattern with hihat on 2 and 4 and light kick/snare comping, closed by a syncopated snare comp - the ride itself, already ringing through the bar, carries the landing (jazz rarely resolves onto a crash).", 132, "swing", _jazz_ride_bar, fill_ending=snare_comping_ending, landing_note=None)

_jazz_comp_bar = bar_from_hits(
    {
        RIDE: [(beat_triplet(b), "x" if b % 2 else "X") for b in range(4)] + [(beat_triplet(b, 2), "x") for b in range(4)],
        SIDESTICK: [(beat_triplet(1), "x"), (beat_triplet(3, 1), "g")],
        KICK: [(beat_triplet(2, 1), "x")],
    },
    step_ticks=TRIPLET_8TH,
)
_add("Jazz Swing", "brush-style-comping", "Brush-style comping groove", "A lighter swing groove using sidestick instead of a full backbeat, for a brushed-comping feel, closed by a light syncopated snare comp under the ride.", 96, "swing", _jazz_comp_bar, fill_ending=snare_comping_ending, landing_note=None)

# --- Latin: bossa nova + songo-style (profile: straight-dominant, no dedicated claves/cowbell voice) ---

_SON_CLAVE_3_2 = [(0, "X"), (3, "X"), (6, "X"), (10, "X"), (12, "X")]

_bossa_bar = bar_from_hits(
    {
        SIDESTICK: _SON_CLAVE_3_2,
        KICK: [(beat16(0), "x"), (beat16(1, 2), "x"), (beat16(3), "x")],
        HIHAT_CLOSED: [(beat16(b, s), "x") for b in range(4) for s in (0, 2)],
    }
)
_add("Latin", "bossa-nova", "Bossa nova groove", "Son-clave sidestick pattern over a light bossa kick and closed-hihat pulse, closed by a laid-back triplet-hihat turn into a sidestick accent.", 118, "even", _bossa_bar, fill_ending=hihat_triplet_ending)

_songo_bar = bar_from_hits(
    {
        TIMBALE1: [(beat16(0), "X"), (beat16(2), "x"), (beat16(2, 2), "x")],
        SNARE: [(beat16(1), "x", ), (beat16(3), "X")],
        KICK: [(beat16(0), "X"), (beat16(2, 3), "x")],
        HIHAT_CLOSED: [(beat16(b, s), "x") for b in range(4) for s in (0, 2)],
        TIMBALE2: [(beat16(1, 2), "x")],
    }
)
_add("Latin", "songo-style", "Songo-style groove", "A timbale-and-kick-driven songo-inspired groove, using this project's timbale voices for Latin percussion colour, closed by a timbale/tom cascade into the next downbeat's crash.", 104, "even", _songo_bar, fill_ending=partial(tom_cascade_ending, toms=(TIMBALE3, TIMBALE4, TOM1)))

# --- Funk: straight 16th-note funk + swung/New-Orleans-style funk (profile: straight 76%, swing 24%) ---

_funk_straight_bar = bar_from_hits(
    {
        KICK: [(beat16(0), "X"), (beat16(1, 3), "x"), (beat16(2, 2), "x")],
        SNARE: [(beat16(1), "X"), (beat16(2, 3), "g"), (beat16(3), "X"), (beat16(3, 3), "g")],
        HIHAT_CLOSED: [(i, "x") for i in range(16)],
    }
)
_add("Funk", "straight-sixteenth-funk", "Straight 16th-note funk", "Dense 16th-note hihat funk groove with syncopated kick and snare ghost notes, closed by funk's own signature device - syncopated ghost/accent snare jabs - into the next downbeat's crash.", 104, "even", _funk_straight_bar, fill_ending=ghost_snare_ending)

_funk_swing_bar = bar_from_hits(
    {
        KICK: [(beat_triplet(0), "X"), (beat_triplet(1, 2), "x"), (beat_triplet(2, 1), "x")],
        SNARE: [(beat_triplet(1), "X"), (beat_triplet(2, 2), "g"), (beat_triplet(3), "X")],
        HIHAT_CLOSED: [(i, "x") for i in range(12)],
    },
    step_ticks=TRIPLET_8TH,
)
_add("Funk", "swung-funk", "Swung funk (New-Orleans style)", "A triplet-swung funk pocket in the New-Orleans second-line tradition, closed by syncopated ghost/accent snare jabs into the next downbeat's crash.", 92, "swing", _funk_swing_bar, fill_ending=ghost_snare_ending)

# --- Fusion: straight syncopated + swung fusion (profile: straight 77%, swing 23%, high ghost rate) ---

_fusion_straight_bar = bar_from_hits(
    {
        KICK: [(beat16(0), "X"), (beat16(1, 2), "x"), (beat16(2, 3), "x"), (beat16(3, 1), "x")],
        SNARE: [(beat16(1), "X"), (beat16(1, 2), "g"), (beat16(3), "X"), (beat16(3, 3), "g")],
        RIDE: [(i, "x") for i in range(0, 16, 2)],
    }
)
_add("Fusion", "syncopated-straight-fusion", "Syncopated straight fusion", "A ride-driven fusion groove with a syncopated kick and frequent snare ghost notes, matching this genre's high ghost-note rate, closed by a short tom turn on the last beat.", 112, "even", _fusion_straight_bar)

_fusion_swing_bar = bar_from_hits(
    {
        KICK: [(beat_triplet(0), "X"), (beat_triplet(1, 1), "x"), (beat_triplet(2, 2), "x")],
        SNARE: [(beat_triplet(1), "X"), (beat_triplet(2, 1), "g"), (beat_triplet(3), "X")],
        RIDE: [(beat_triplet(b), "x") for b in range(4)] + [(beat_triplet(b, 2), "x") for b in range(4)],
    },
    step_ticks=TRIPLET_8TH,
)
_add("Fusion", "swung-fusion", "Swung fusion", "A triplet-swung fusion groove over a ride-cymbal pulse, the secondary feel this genre's profile shows.", 100, "swing", _fusion_swing_bar)

# --- Hip-Hop: boom-bap (laid-back) + busy 16th-roll hihat variant (no feel signal; genre knowledge used) ---

_hiphop_boombap_bar = bar_from_hits(
    {
        KICK: [(beat16(0), "X"), (beat16(2, 2), "x"), (beat16(3, 3), "x")],
        SNARE: [(beat16(1), "X"), (beat16(3), "X")],
        HIHAT_CLOSED: [(beat16(b, s), "x") for b in range(4) for s in (0, 2)],
    }
)
_add("Hip-Hop", "boom-bap", "Boom-bap groove", "A laid-back boom-bap groove: sparse syncopated kick, hard backbeat, 8th-note hihat, closed by boom-bap's own trademark - a low tom-then-kick punch, not a melodic run - and no crash, since these loops are meant to cut cleanly.", 90, "even", _hiphop_boombap_bar, fill_ending=tom_kick_punch_ending, landing_note=None)

_hiphop_trap_bar = bar_from_hits(
    {
        KICK: [(beat16(0), "X"), (beat16(1, 2), "x"), (beat16(2, 3), "x")],
        SNARE: [(beat16(1), "X"), (beat16(3), "X")],
        HIHAT_CLOSED: [(i, "x") for i in range(16)],
        HIHAT_OPEN: [(beat16(3, 2), "o")],
    }
)
_add("Hip-Hop", "busy-hihat-roll", "Busy hihat-roll groove", "A denser modern hip-hop groove with a full 16th-note hihat roll and one open-hihat accent, closed by a short snare-roll crescendo and no crash, since these loops are meant to cut cleanly.", 140, "even", _hiphop_trap_bar, fill_ending=snare_roll_flourish_ending, landing_note=None)

# --- Progressive: straight odd-accented + swung progressive (profile: straight 72%, swing 16%) ---

_prog_straight_bar = bar_from_hits(
    {
        KICK: [(beat16(0), "X"), (beat16(1, 1), "x"), (beat16(2, 2), "x"), (beat16(3, 1), "x")],
        SNARE: [(beat16(1), "X"), (beat16(3), "X")],
        RIDE: [(i, "x") for i in range(0, 16, 2)],
        RIDE_BELL: [(beat16(0), "X")],
    }
)
_add("Progressive", "straight-odd-accented", "Straight odd-accented groove", "A straight-8th progressive groove with an asymmetric kick pattern under a ride-bell-accented cymbal pulse, closed by a short tom turn on the last beat.", 118, "even", _prog_straight_bar)

_prog_swing_bar = bar_from_hits(
    {
        KICK: [(beat_triplet(0), "X"), (beat_triplet(1, 2), "x"), (beat_triplet(3, 1), "x")],
        SNARE: [(beat_triplet(1), "X"), (beat_triplet(3), "X")],
        RIDE: [(beat_triplet(b), "x") for b in range(4)],
    },
    step_ticks=TRIPLET_8TH,
)
_add("Progressive", "swung-progressive", "Swung progressive groove", "A triplet-swung progressive groove, the secondary feel this genre's profile shows alongside the dominant straight feel.", 100, "swing", _prog_swing_bar)

# --- Reggae is not built here - see "Expert-driven grooves" below (reggae_expert.json) -------

# --- Pop: straight backbeat + swung pop shuffle (profile: straight 77%, swing 23%) ---

_pop_straight_bar = bar_from_hits(
    {
        KICK: [(beat16(0), "X"), (beat16(2, 2), "x")],
        SNARE: [(beat16(1), "X"), (beat16(3), "X")],
        HIHAT_CLOSED: [(beat16(b, s), "x") for b in range(4) for s in (0, 2)],
    }
)
_add("Pop", "straight-backbeat", "Straight pop backbeat", "A clean, simple straight-8th pop backbeat - the dominant feel in this genre's reference profile - closed by a tom cascade into the next downbeat's crash.", 118, "even", _pop_straight_bar)

_pop_swing_bar = bar_from_hits(
    {
        KICK: [(beat_triplet(0), "X"), (beat_triplet(2, 2), "x")],
        SNARE: [(beat_triplet(1), "X"), (beat_triplet(3), "X")],
        HIHAT_CLOSED: [(beat_triplet(b), "x") for b in range(4)] + [(beat_triplet(b, 2), "x") for b in range(4)],
    },
    step_ticks=TRIPLET_8TH,
)
_add("Pop", "swung-pop-shuffle", "Swung pop shuffle", "A lightly swung pop shuffle, the secondary feel this genre's reference profile shows.", 100, "swing", _pop_swing_bar)

# --- Soul: straight deep pocket + swung soul (profile: straight 60%, swing 40%, moderate ghost rate) ---

_soul_straight_bar = bar_from_hits(
    {
        KICK: [(beat16(0), "X"), (beat16(1, 3), "x"), (beat16(2, 2), "x")],
        SNARE: [(beat16(1), "X"), (beat16(2, 3), "g"), (beat16(3), "X")],
        HIHAT_CLOSED: [(beat16(b, s), "x") for b in range(4) for s in (0, 2)],
    }
)
_add("Soul", "deep-pocket-straight", "Deep-pocket straight soul", "A laid-back straight soul groove with a syncopated kick and a snare ghost note, closed by syncopated ghost/accent snare jabs (soul's subtler cousin of funk's fill device) into the next downbeat's crash.", 92, "even", _soul_straight_bar, fill_ending=ghost_snare_ending)

_soul_swing_bar = bar_from_hits(
    {
        KICK: [(beat_triplet(0), "X"), (beat_triplet(1, 2), "x"), (beat_triplet(2, 1), "x")],
        SNARE: [(beat_triplet(1), "X"), (beat_triplet(2, 2), "g"), (beat_triplet(3), "X")],
        HIHAT_CLOSED: [(beat_triplet(b), "x") for b in range(4)] + [(beat_triplet(b, 2), "x") for b in range(4)],
    },
    step_ticks=TRIPLET_8TH,
)
_add("Soul", "swung-soul", "Swung soul groove", "A triplet-swung soul groove, closed by syncopated ghost/accent snare jabs into the next downbeat's crash.", 84, "swing", _soul_swing_bar, fill_ending=ghost_snare_ending)


# --- Expert-driven grooves (pilot: Reggae) ----------------------------------------------------
# Rock/Blues/.../Soul above are one hand-written Python bar per groove, with a jitter-only
# second take. That produced two problems for Reggae specifically: the jitter isn't how a
# drummer actually varies a take (a real second take swaps an articulation or adds an accent,
# per its own named device - see reggae_expert.json), and the hand-written pattern itself had
# real idiomatic mistakes (a full snare where the style's defining sound is a rimshot/cross-
# stick click). This interpreter reads reggae_expert.json - an explicit, citable record of how
# the style is actually played and varied - instead of more hand-written Python literals, and
# each named variation becomes its own take rather than a humanized clone of another one.

ARTICULATION_NOTE = {"rimshot": RIMSHOT, "sidestick": SIDESTICK, "full_snare": SNARE}
NOTE_BY_NAME = {
    "KICK": KICK, "SNARE": SNARE, "RIMSHOT": RIMSHOT, "SIDESTICK": SIDESTICK,
    "HIHAT_CLOSED": HIHAT_CLOSED, "HIHAT_OPEN": HIHAT_OPEN, "CRASH": CRASH, "RIDE": RIDE,
    "TOM1": TOM1, "TOM2": TOM2, "TOM3": TOM3, "TOM_LOW": TOM_LOW,
}
FILL_DEVICE_BY_NAME = {
    "tom_cascade": tom_cascade_ending,
    "ghost_snare": ghost_snare_ending,
    "hihat_triplet": hihat_triplet_ending,
    "snare_comping": snare_comping_ending,
}


def build_expert_bar(style: dict, variation: dict) -> list[NoteEvent]:
    hits: dict[int, list[Hit]] = {}
    hits.setdefault(HIHAT_CLOSED, []).extend((step, "x") for step in style["hihat_steps"])
    hits.setdefault(KICK, []).extend((step, "X") for step in style["kick_steps"])
    snare_note = ARTICULATION_NOTE[variation["snare_articulation"]]
    hits.setdefault(snare_note, []).append((style["snare_accent_step"], "X"))
    for extra in variation.get("extra_accents", []):
        hits.setdefault(NOTE_BY_NAME[extra["note"]], []).append((extra["step"], extra["symbol"]))
    return bar_from_hits(hits)


def write_expert_groove(config_path: Path, genre: str, out_dir: Path) -> int:
    styles = json.loads(config_path.read_text(encoding="utf-8"))["styles"]
    genre_dir = out_dir / genre
    written = 0
    for style in styles:
        fill_ending = FILL_DEVICE_BY_NAME[style["fill_device"]]
        for variation_index, variation in enumerate(style["variations"], start=1):
            normal_bar = build_expert_bar(style, variation)
            first_bar = normal_bar + crash_hit()
            fill_bar = make_fill_bar(normal_bar, fill_beats=style["fill_beats"], ending=fill_ending)
            bars = [first_bar, normal_bar, normal_bar, fill_bar]
            loop_length_ticks = len(bars) * BAR_TICKS_44
            take_events = humanize(assemble(bars), seed=f"{genre}/{style['id']}/{variation['name']}")

            stem = f"{style['id']}_v{variation_index}"
            write_smf(genre_dir / f"{stem}.mid", bpm=style["bpm"], numerator=4, denominator=4, notes=take_events, loop_length_ticks=loop_length_ticks)
            sidecar = {
                "idealBpm": style["bpm"],
                "rhythm": {"feel": "even", "timeSignature": "4/4"},
                "variation": variation_index,
                "section": variation["name"],
                "dominantSounds": ["snare", "hihat"],
                "variesBy": "snare",
                "displayName": style["display_name"],
                "description": style["expert_notes"],
                "variationNotes": variation.get("expert_notes", ""),
            }
            (genre_dir / f"{stem}.json").write_text(json.dumps(sidecar, indent=2) + "\n", encoding="utf-8")
            written += 1
    return written


# --- Output ------------------------------------------------------------------------------


def write_groove(groove: Groove, out_dir: Path) -> None:
    events = assemble(groove.bars)  # every bar is a full 4/4 bar in ticks, whatever subdivision grid built it
    loop_length_ticks = len(groove.bars) * BAR_TICKS_44
    numerator, denominator = (int(x) for x in groove.time_signature.split("/"))

    genre_dir = out_dir / groove.genre
    for variation, take_events in ((1, events), (2, humanize(events, seed=f"{groove.genre}/{groove.slug}"))):
        stem = f"{groove.slug}_v{variation}"
        write_smf(genre_dir / f"{stem}.mid", bpm=groove.bpm, numerator=numerator, denominator=denominator, notes=take_events, loop_length_ticks=loop_length_ticks)
        sidecar = {
            "idealBpm": groove.bpm,
            "rhythm": {"feel": groove.feel, "timeSignature": groove.time_signature},
            "variation": variation,
            "section": groove.section,
            "dominantSounds": groove.dominant_sounds,
            "variesBy": groove.varies_by,
            "displayName": groove.display_name,
            "description": groove.description,
        }
        (genre_dir / f"{stem}.json").write_text(json.dumps(sidecar, indent=2) + "\n", encoding="utf-8")


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out", type=Path, default=Path(__file__).resolve().parent / "generated")
    args = ap.parse_args()

    for groove in GROOVES:
        write_groove(groove, args.out)

    reggae_config = Path(__file__).resolve().parent / "reggae_expert.json"
    expert_file_count = write_expert_groove(reggae_config, "Reggae", args.out)

    genres = sorted({g.genre for g in GROOVES} | {"Reggae"})
    print(f"wrote {len(GROOVES)} grooves x 2 variations + {expert_file_count} expert-driven Reggae files across {len(genres)} genres to {args.out}")


if __name__ == "__main__":
    main()
