"""Serializes a PatternSpec (authored entirely in absolute ticks) to a Standard MIDI File.
This is the only place that computes MIDI delta times - callers never think in deltas.
"""

from __future__ import annotations

import os

import mido

from rhythm_library.model import PatternSpec, bar_start_ticks

CHANNEL = 9  # zero-based channel 9 (GM percussion)

# Category rank breaks ties when several messages share a tick, matching PLAN.md's required
# tick-0 order (track_name, text, time_signature, set_tempo, markers) and putting a note-off
# before any note-on sharing its tick, so a same-tick retrigger never gets swallowed.
_RANK_TRACK_NAME = 0
_RANK_TEXT = 1
_RANK_TIME_SIGNATURE = 2
_RANK_TEMPO = 3
_RANK_MARKER = 4
_RANK_NOTE_OFF = 5
_RANK_NOTE_ON = 6


def _time_signature_events(pattern: PatternSpec) -> list[tuple[int, mido.MetaMessage]]:
    events: list[tuple[int, mido.MetaMessage]] = []
    starts = bar_start_ticks(pattern.meters, pattern.bar_count)
    last_meter = None
    for bar, start in enumerate(starts):
        meter = pattern.meters[bar % len(pattern.meters)]
        if meter != last_meter:
            events.append((start, mido.MetaMessage("time_signature", numerator=meter.numerator, denominator=meter.denominator, time=0)))
            last_meter = meter
    return events


def write_pattern(pattern: PatternSpec, out_dir: str) -> str:
    """Writes pattern to `<out_dir>/<pattern.relative_path>`, creating directories as needed.
    Returns the absolute path written. Reloads and sanity-checks the file before returning."""
    midi_file = mido.MidiFile(type=0, ticks_per_beat=480)
    track = mido.MidiTrack()
    midi_file.tracks.append(track)

    all_events: list[tuple[int, int, object]] = [
        (0, _RANK_TRACK_NAME, mido.MetaMessage("track_name", name=pattern.display_name, time=0)),
        (0, _RANK_TEXT, mido.MetaMessage("text", text=pattern.description, time=0)),
        (0, _RANK_TEMPO, mido.MetaMessage("set_tempo", tempo=mido.bpm2tempo(pattern.bpm), time=0)),
    ]
    all_events += [(tick, _RANK_TIME_SIGNATURE, msg) for tick, msg in _time_signature_events(pattern)]
    all_events += [(section.tick, _RANK_MARKER, mido.MetaMessage("marker", text=section.label, time=0)) for section in pattern.sections]
    for event in pattern.events:
        all_events.append((event.tick, _RANK_NOTE_ON, ("note_on", event.note, event.velocity)))
        all_events.append((event.tick + event.duration, _RANK_NOTE_OFF, ("note_off", event.note, 0)))

    all_events.sort(key=lambda e: (e[0], e[1]))

    last_tick = 0
    for tick, _rank, payload in all_events:
        delta = tick - last_tick
        if isinstance(payload, mido.MetaMessage):
            track.append(payload.copy(time=delta))
        else:
            message_type, note, velocity = payload
            track.append(mido.Message(message_type, note=note, velocity=velocity, time=delta, channel=CHANNEL))
        last_tick = tick

    assert last_tick <= pattern.loop_length_ticks
    track.append(mido.MetaMessage("end_of_track", time=pattern.loop_length_ticks - last_tick))

    out_path = os.path.join(out_dir, pattern.relative_path)
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    midi_file.save(out_path)

    reloaded = mido.MidiFile(out_path)
    assert reloaded.type == 0
    assert len(reloaded.tracks) == 1
    return out_path
