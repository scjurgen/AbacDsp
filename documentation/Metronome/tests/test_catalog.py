from __future__ import annotations


def test_no_duplicate_pattern_ids(patterns):
    ids = [p.id for p in patterns]
    assert len(ids) == len(set(ids))


def test_no_duplicate_output_paths(patterns):
    paths = [p.relative_path for p in patterns]
    assert len(paths) == len(set(paths))


def test_all_patterns_have_description_tags_and_recommended_use(patterns):
    for pattern in patterns:
        assert pattern.description
        assert pattern.tags
        assert pattern.recommended_use


def test_cultural_patterns_have_source_context(patterns):
    cultural = [p for p in patterns if "cultural" in p.tags]
    assert cultural
    for pattern in cultural:
        assert pattern.source_context


def test_pattern_count_is_substantial(patterns):
    # PLAN.md's original "roughly 75-110" estimate was superseded by the explicit "generate at
    # least these" catalog once every listed family/pattern was implemented in full (see
    # PLAN.md, "Delivery"); this just guards against the catalog silently shrinking.
    assert len(patterns) >= 150


def test_every_area_and_family_has_at_least_one_file(patterns):
    areas = {p.area for p in patterns}
    assert areas == {"educational", "looper", "performance"}
    families_by_area: dict[str, set[str]] = {}
    for pattern in patterns:
        families_by_area.setdefault(pattern.area, set()).add(pattern.family)
    for area, families in families_by_area.items():
        assert families, f"{area} has no families"
