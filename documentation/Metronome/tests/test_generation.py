from __future__ import annotations

import os


def test_generates_every_pattern_file(generated_dir, patterns):
    for pattern in patterns:
        assert os.path.isfile(generated_dir / pattern.relative_path)


def test_generates_manifest(generated_dir):
    assert os.path.isfile(generated_dir / "manifest.json")


def test_generates_root_readme(generated_dir):
    assert os.path.isfile(generated_dir / "README.md")


def test_pattern_count_matches_catalog(patterns, manifest):
    assert len(manifest) == len(patterns)
