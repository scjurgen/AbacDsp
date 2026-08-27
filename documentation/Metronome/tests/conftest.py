from __future__ import annotations

import json

import pytest

from rhythm_library.documentation import generate_all_documentation
from rhythm_library.manifest import write_manifest
from rhythm_library.midi_writer import write_pattern
from rhythm_library.patterns import all_patterns


@pytest.fixture(scope="session")
def patterns():
    return all_patterns()


@pytest.fixture(scope="session")
def generated_dir(tmp_path_factory, patterns):
    out_dir = tmp_path_factory.mktemp("generated")
    for pattern in patterns:
        write_pattern(pattern, str(out_dir))
    write_manifest(patterns, str(out_dir))
    generate_all_documentation(patterns, str(out_dir))
    return out_dir


@pytest.fixture(scope="session")
def manifest(generated_dir):
    with open(generated_dir / "manifest.json", encoding="utf-8") as manifest_file:
        return json.load(manifest_file)


@pytest.fixture
def pattern_by_id(patterns):
    return {p.id: p for p in patterns}
