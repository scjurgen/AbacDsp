from __future__ import annotations

import mido
import pytest


def _load(generated_dir, pattern):
    return mido.MidiFile(str(generated_dir / pattern.relative_path))


def test_every_file_opens_in_mido(generated_dir, patterns):
    for pattern in patterns:
        _load(generated_dir, pattern)


def test_every_file_is_type_0_single_track(generated_dir, patterns):
    for pattern in patterns:
        midi_file = _load(generated_dir, pattern)
        assert midi_file.type == 0
        assert len(midi_file.tracks) == 1


def test_every_file_ends_with_end_of_track(generated_dir, patterns):
    for pattern in patterns:
        midi_file = _load(generated_dir, pattern)
        assert midi_file.tracks[0][-1].type == "end_of_track"


def test_tick_zero_has_time_signature_and_tempo(generated_dir, patterns):
    for pattern in patterns:
        midi_file = _load(generated_dir, pattern)
        tick = 0
        seen = set()
        for message in midi_file.tracks[0]:
            if tick == 0:
                seen.add(message.type)
            tick += message.time
            if tick > 0:
                break
        assert "time_signature" in seen
        assert "set_tempo" in seen


def test_every_filename_appears_in_manifest(patterns, manifest):
    manifest_paths = {entry["relative_path"] for entry in manifest}
    for pattern in patterns:
        assert pattern.relative_path in manifest_paths


def test_every_manifest_item_refers_to_existing_file(generated_dir, manifest):
    for entry in manifest:
        assert (generated_dir / entry["relative_path"]).is_file()
