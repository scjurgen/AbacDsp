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

## Unit tests

Generator-side logic that doesn't depend on JUCE (currently: `ThemeOrbit.h`) has its own
GoogleTest target, separate from the DSP library's `test/` tree:

- Tests live in `unittests/` (e.g. `unittests/ThemeOrbit_test.cpp`), any `*_test.cpp` there is
  picked up automatically.
- Built as part of the root project's normal `PACKAGE_TESTS` flow: `dev-test.sh` (or
  `ctest -R JuceStandaloneGeneratorTests`) runs it alongside the DSP library tests, no JUCE
  build required.

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

