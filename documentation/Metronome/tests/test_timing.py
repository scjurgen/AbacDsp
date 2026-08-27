from __future__ import annotations

import mido

from rhythm_library.model import bar_start_ticks, total_ticks


def _load(generated_dir, pattern):
    return mido.MidiFile(str(generated_dir / pattern.relative_path))


def test_all_deltas_are_non_negative(generated_dir, patterns):
    for pattern in patterns:
        midi_file = _load(generated_dir, pattern)
        for message in midi_file.tracks[0]:
            assert message.time >= 0


def test_sum_of_deltas_equals_expected_duration(generated_dir, patterns):
    for pattern in patterns:
        midi_file = _load(generated_dir, pattern)
        total = sum(message.time for message in midi_file.tracks[0])
        assert total == pattern.loop_length_ticks, pattern.id


def test_every_note_off_after_its_note_on(generated_dir, patterns):
    for pattern in patterns:
        midi_file = _load(generated_dir, pattern)
        open_notes: dict[int, int] = {}
        tick = 0
        for message in midi_file.tracks[0]:
            tick += message.time
            if message.type == "note_on":
                open_notes[message.note] = tick
            elif message.type == "note_off":
                assert message.note in open_notes, f"{pattern.id}: note_off with no matching note_on at tick {tick}"
                assert tick >= open_notes[message.note]
                del open_notes[message.note]


def test_marker_ticks_within_duration(patterns):
    for pattern in patterns:
        for section in pattern.sections:
            assert 0 <= section.tick <= pattern.loop_length_ticks


def test_loopable_pattern_duration_matches_declared_bar_length(patterns):
    for pattern in patterns:
        if "looper" not in {pattern.area} and pattern.area != "performance":
            continue
        assert pattern.loop_length_ticks == total_ticks(pattern.meters, pattern.bar_count)


def test_mixed_meter_bar_lengths_calculated_correctly(patterns):
    mixed = [p for p in patterns if len(p.meters) > 1]
    assert mixed, "expected at least one mixed-meter pattern in the catalog"
    for pattern in mixed:
        starts = bar_start_ticks(pattern.meters, pattern.bar_count)
        expected = 0
        for bar, start in enumerate(starts):
            assert start == expected
            expected += pattern.meters[bar % len(pattern.meters)].bar_ticks
        assert expected == pattern.loop_length_ticks
