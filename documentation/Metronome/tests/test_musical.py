"""Pattern-specific musical correctness tests (PLAN.md, Testing and validation, section 3)."""

from __future__ import annotations

from rhythm_library import instruments as instr


def test_conventional_bar_marker_accents_beat_one(pattern_by_id):
    pattern = pattern_by_id["conventional-bar-marker-4-4"]
    bar_ticks = pattern.meters[0].bar_ticks
    first_bar = [e for e in pattern.events if e.tick < bar_ticks]
    beat_one = [e for e in first_bar if e.tick == 0]
    other_beats = [e for e in first_bar if e.tick != 0]
    assert beat_one and all(e.velocity == instr.VEL_ACCENT for e in beat_one)
    assert other_beats and all(e.velocity < instr.VEL_ACCENT for e in other_beats)


def test_backbeat_stronger_on_two_and_four(pattern_by_id):
    pattern = pattern_by_id["backbeat"]
    bar_ticks = pattern.meters[0].bar_ticks
    unit = pattern.meters[0].unit_ticks
    first_bar = [e for e in pattern.events if e.tick < bar_ticks]
    by_tick = {e.tick: e.velocity for e in first_bar}
    assert by_tick[unit] > by_tick[0]
    assert by_tick[3 * unit] > by_tick[2 * unit]


def test_jazz_two_and_four_clicks_only_on_two_and_four(pattern_by_id):
    pattern = pattern_by_id["jazz-two-and-four"]
    bar_ticks = pattern.meters[0].bar_ticks
    unit = pattern.meters[0].unit_ticks
    first_bar_ticks = {e.tick for e in pattern.events if e.tick < bar_ticks}
    assert first_bar_ticks == {unit, 3 * unit}


def test_six_eight_pulses_on_dotted_quarter_positions(pattern_by_id):
    pattern = pattern_by_id["six-eight-two-dotted-quarter-pulses"]
    bar_ticks = pattern.meters[0].bar_ticks
    first_bar_ticks = sorted(e.tick for e in pattern.events if e.tick < bar_ticks)
    assert first_bar_ticks == [0, 720]


def test_five_four_grouping_matches_accents(pattern_by_id):
    for pattern_id, grouping in (
        ("five-four-three-plus-two", [3, 2]),
        ("five-four-two-plus-three", [2, 3]),
    ):
        pattern = pattern_by_id[pattern_id]
        bar_ticks = pattern.meters[0].bar_ticks
        unit = pattern.meters[0].unit_ticks
        first_bar = sorted((e.tick for e in pattern.events if e.tick < bar_ticks and e.velocity == instr.VEL_ACCENT))
        expected_starts = []
        running = 0
        for size in grouping:
            expected_starts.append(running * unit)
            running += size
        assert first_bar == expected_starts


def test_seven_eight_grouping_matches_accents(pattern_by_id):
    pattern = pattern_by_id["seven-eight-two-plus-two-plus-three"]
    unit = pattern.meters[0].unit_ticks
    bar_ticks = pattern.meters[0].bar_ticks
    accented = sorted(e.tick for e in pattern.events if e.tick < bar_ticks and e.velocity == instr.VEL_ACCENT)
    assert accented == [0, 2 * unit, 4 * unit]


def test_offbeat_only_has_no_click_on_quarter_beats(pattern_by_id):
    pattern = pattern_by_id["offbeat-only"]
    unit = pattern.meters[0].unit_ticks
    for event in pattern.events:
        assert event.tick % unit != 0


def test_gap_patterns_contain_actual_silent_bars(pattern_by_id):
    pattern = pattern_by_id["one-bar-click-one-bar-silent"]
    bar_ticks = pattern.meters[0].bar_ticks
    silent_bar_events = [e for e in pattern.events if bar_ticks <= e.tick < 2 * bar_ticks]
    assert silent_bar_events == []


def test_swing_files_have_unequal_eighth_positions(pattern_by_id):
    pattern = pattern_by_id["swing-light-55-percent"]
    unit = pattern.meters[0].unit_ticks
    onbeat = 0
    offbeat = min(e.tick for e in pattern.events if 0 < e.tick < unit)
    assert offbeat != unit // 2  # not a straight (even) eighth
    expected_offset = round(unit * 55 / 100)
    assert offbeat == expected_offset


def test_son_vs_rumba_clave_differ_at_3_side_final_note(pattern_by_id):
    son = pattern_by_id["son-clave-3-2"]
    rumba = pattern_by_id["rumba-clave-3-2"]
    son_ticks = sorted(e.tick for e in son.events)
    rumba_ticks = sorted(e.tick for e in rumba.events)
    assert son_ticks[:2] == rumba_ticks[:2]
    assert son_ticks[2] != rumba_ticks[2]
    assert son_ticks[3:] == rumba_ticks[3:]


def test_performance_four_bar_patterns_have_loop_start_and_turnaround(patterns):
    # Mixed-meter files loop via their own bar-by-bar meter alternation (see PLAN.md, "Special
    # documentation cases: mixed meters"), not the bar-4 subdivision-lift/turnaround convention.
    four_bar_performance = [
        p
        for p in patterns
        if p.area == "performance" and p.bar_count == 4 and p.count_in_bars == 0 and p.family != "mixed-meter"
    ]
    assert four_bar_performance
    for pattern in four_bar_performance:
        labels = [s.label for s in pattern.sections]
        assert any(label in ("BAR 1",) for label in labels) or pattern.sections[0].tick == 0
        assert any(label in ("TURNAROUND", "FILL CUE") for label in labels)


def test_count_in_patterns_have_markers_and_correct_loop_start(patterns):
    count_in_patterns = [p for p in patterns if p.count_in_bars > 0]
    assert count_in_patterns
    for pattern in count_in_patterns:
        labels = {s.label for s in pattern.sections}
        assert "COUNT-IN" in labels
        assert "LOOP START" in labels
        expected_loop_start = pattern.count_in_bars * pattern.meters[0].bar_ticks
        loop_start_section = next(s for s in pattern.sections if s.label == "LOOP START")
        assert loop_start_section.tick == expected_loop_start
