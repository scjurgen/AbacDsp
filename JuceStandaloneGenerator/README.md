# Juce Standalone Generator

## Steps for standalone generation

- check blueprints, there are some examples.
- ./generate-juce-standalone.py --mode standalone --target-dir /path/to/juce-projects yourblueprint
- generated files are in /path/to/juce-projects/yourblueprint
- to setup the project run the init-project.sh (chmod +x first)

## Steps for example generation

- ./generate-juce-standalone.py --mode localexample yourblueprint
- generated files are in ./examples/yourblueprint
- add the folder generated to CMakeLists.txt

## Performance / Settings page

A blueprint can optionally add a `performance-page` section, structured like
`layout` (a `type` + `composition` string), to give the generated editor a
second, curated page:

```json
"performance-page": {
  "type": "-",
  "composition": "A1=(LVL,SIZES)"
}
```

- `composition` references existing `ports-control` items by their `short`
  code, reusing the same widgets shown on the full Settings page (today's
  `layout` section, unchanged) - nothing is duplicated or removed.
- If a blueprint defines a non-empty `performance-page`, the generated editor
  gets a small Performance/Settings switcher; widgets not referenced by
  `performance-page.composition` are hidden while Performance is active.
- If a blueprint has no `performance-page` section (or an empty one, e.g.
  `distortion`), the editor keeps today's single-page layout untouched.
- Typical candidates: the `levels` VU meter and any gauge with a `customtype`
  (bespoke visualisations like maxdiffuser's size display or the looper's
  CircularBarDisplays) - see the existing blueprints for examples.

## Unit tests

Generator-side logic that doesn't depend on JUCE (currently: `ThemeOrbit.h`) has its own
GoogleTest target, separate from the DSP library's `test/` tree:

- Tests live in `unittests/` (e.g. `unittests/ThemeOrbit_test.cpp`), any `*_test.cpp` there is
  picked up automatically.
- Built as part of the root project's normal `PACKAGE_TESTS` flow: `dev-test.sh` (or
  `ctest -R JuceStandaloneGeneratorTests`) runs it alongside the DSP library tests, no JUCE
  build required.

## Gold-master regression test

`check_gold_master.py` guards the generator's Python code (blueprint.py, report.py,
codegen_processor.py, codegen_widgets.py, template_engine.py, cli.py,
generate-juce-standalone.py) against accidental output changes, independent of
blueprint content changes:

- `./check_gold_master.py --check` (default) generates every blueprint into an
  isolated scratch dir and diffs the templated output (the generated `src/*.h`,
  `src/*.cpp`, `src/unittests/*`, and the two `CMakeLists.txt` files per module)
  against the committed baseline in `gold_master/`. Files that are plain copies
  from `templates/sourcefiles` (inc/*.h, themes, logo.png, gitignore) are excluded
  since the generator can't change them.
- `./check_gold_master.py --record` recaptures the baseline after an intentional
  output change (e.g. a template edit). Review the diff before committing.
- Both accept a list of blueprint names to limit the run, e.g.
  `./check_gold_master.py --check maxdiffuser looper`.
- `../CMakeLists.txt` is snapshotted and restored around the run, since
  `--mode localexample` would otherwise add `add_subdirectory(...)` entries for
  any blueprint not yet wired into it.

## Theme review tool

`theme-review/` is a standalone JUCE tool for visually auditing the OKLCH hue-rotation theme
(`ThemeOrbit.h` / `Themes::definition()`) across every hue step and light/dark mode, without
needing a real plugin host or manual clicking through the Settings menu.

- Instantiates the same widget classes a generated example actually uses (dial, toggle, label,
  CPU/level/spectrogram/signal gauges), fed synthetic data, and renders them off-screen via
  `juce::Component::createComponentSnapshot()` - no visible window needed.
- Renders all 12 hues x {Light, Dark} = 24 combinations, writing one PNG per combination plus a
  single stitched contact sheet, to `theme-review/theme-review-output/` (gitignored).
- Built only with `BUILD_FULL_PROJECT=ON` (it links JUCE): configure with
  `-DBUILD_FULL_PROJECT=ON`, then build the `ThemeReviewTool` target and run the resulting
  executable.

