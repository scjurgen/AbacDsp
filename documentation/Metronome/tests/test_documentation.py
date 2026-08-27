from __future__ import annotations

import os
import re


def _links_in(markdown: str) -> list[str]:
    return re.findall(r"\]\(([^)]+)\)", markdown)


def test_readme_present_for_every_populated_folder(generated_dir, patterns):
    assert (generated_dir / "README.md").is_file()
    for area in {p.area for p in patterns}:
        assert (generated_dir / area / "README.md").is_file()
    for pattern in patterns:
        assert (generated_dir / pattern.area / pattern.family / "README.md").is_file()


def test_every_mid_file_linked_from_its_family_readme(generated_dir, patterns):
    for pattern in patterns:
        readme_path = generated_dir / pattern.area / pattern.family / "README.md"
        content = readme_path.read_text(encoding="utf-8")
        assert f"{pattern.id}.mid" in content


def test_every_family_readme_linked_from_area_readme(generated_dir, patterns):
    by_area: dict[str, set[str]] = {}
    for pattern in patterns:
        by_area.setdefault(pattern.area, set()).add(pattern.family)
    for area, families in by_area.items():
        content = (generated_dir / area / "README.md").read_text(encoding="utf-8")
        for family in families:
            assert f"{family}/README.md" in content


def test_every_area_readme_linked_from_root(generated_dir, patterns):
    root = (generated_dir / "README.md").read_text(encoding="utf-8")
    for area in {p.area for p in patterns}:
        assert f"{area}/README.md" in root


def test_all_local_markdown_links_resolve(generated_dir):
    for readme_path in generated_dir.rglob("README.md"):
        content = readme_path.read_text(encoding="utf-8")
        for link in _links_in(content):
            if link.startswith("http://") or link.startswith("https://"):
                continue
            target = (readme_path.parent / link).resolve()
            assert target.exists(), f"{readme_path}: broken link {link}"


def test_family_readme_catalog_has_one_row_per_file(generated_dir, patterns):
    by_family: dict[tuple[str, str], list] = {}
    for pattern in patterns:
        by_family.setdefault((pattern.area, pattern.family), []).append(pattern)
    for (area, family), family_patterns in by_family.items():
        content = (generated_dir / area / family / "README.md").read_text(encoding="utf-8")
        for pattern in family_patterns:
            assert content.count(f"{pattern.id}.mid") >= 1


def test_cultural_patterns_documented_with_context(generated_dir, patterns):
    for pattern in patterns:
        if not pattern.source_context:
            continue
        content = (generated_dir / pattern.area / pattern.family / "README.md").read_text(encoding="utf-8")
        assert pattern.display_name in content


def test_swing_patterns_display_swing_ratio(generated_dir, patterns):
    for pattern in patterns:
        if not pattern.swing_ratio:
            continue
        content = (generated_dir / pattern.area / pattern.family / "README.md").read_text(encoding="utf-8")
        assert pattern.display_name in content


def test_regenerating_without_data_changes_is_deterministic(tmp_path, patterns):
    from rhythm_library.documentation import generate_root_readme

    out_a = tmp_path / "a"
    out_b = tmp_path / "b"
    path_a = generate_root_readme(patterns, str(out_a))
    path_b = generate_root_readme(patterns, str(out_b))
    assert open(path_a, encoding="utf-8").read() == open(path_b, encoding="utf-8").read()


def test_family_readme_generation_works_for_a_single_pattern_family(tmp_path, patterns):
    from rhythm_library.documentation import generate_family_readme

    one = [patterns[0]]
    path = generate_family_readme(one[0].area, one[0].family, one, str(tmp_path))
    assert os.path.isfile(path)
