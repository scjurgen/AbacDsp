from __future__ import annotations

import json
import os

import pytest

from rhythm_library.deploy import deploy


@pytest.fixture(scope="session")
def deployed_dir(tmp_path_factory, generated_dir):
    out_dir = tmp_path_factory.mktemp("midi_drums")
    deploy(str(generated_dir), str(out_dir))
    return out_dir


def _sidecar(deployed_dir, pattern):
    stem = pattern.relative_path[: -len(".mid")]
    with open(deployed_dir / f"{stem}_v1.json", encoding="utf-8") as sidecar_file:
        return json.load(sidecar_file)


def test_deploy_writes_a_sidecar_for_every_pattern(deployed_dir, patterns):
    for pattern in patterns:
        stem = pattern.relative_path[: -len(".mid")]
        assert os.path.isfile(deployed_dir / f"{stem}_v1.json")


def test_sidecar_fields_for_a_single_meter_pattern(deployed_dir, pattern_by_id):
    sidecar = _sidecar(deployed_dir, pattern_by_id["backbeat"])
    assert sidecar["idealBpm"] == 120.0
    assert sidecar["rhythm"] == {"feel": "even", "timeSignature": "4/4"}
    assert sidecar["bars"] == 4
    assert sidecar["dominantSounds"] == ["snare"]


def test_sidecar_time_signature_is_mixed_for_a_multi_meter_pattern(deployed_dir, pattern_by_id):
    pattern = pattern_by_id["alternating-3-4-and-4-4"]
    assert len(pattern.meters) > 1
    sidecar = _sidecar(deployed_dir, pattern)
    assert sidecar["rhythm"]["timeSignature"] == "mixed"


def test_sidecar_feel_shuffle_for_hard_triplet_swing_ratio(deployed_dir, pattern_by_id):
    pattern = pattern_by_id["shuffle-basic"]
    assert pattern.swing_ratio == "2:1"
    sidecar = _sidecar(deployed_dir, pattern)
    assert sidecar["rhythm"]["feel"] == "shuffle"


def test_sidecar_feel_swing_for_other_swing_ratios(deployed_dir, pattern_by_id):
    pattern = pattern_by_id["swing-medium-60-percent"]
    assert pattern.swing_ratio not in (None, "2:1")
    sidecar = _sidecar(deployed_dir, pattern)
    assert sidecar["rhythm"]["feel"] == "swing"
