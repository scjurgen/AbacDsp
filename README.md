# AbacDsp
Dsp code for abacad projects

## Goals

Have handy classes for various DSP tasks.
No dependencies for the dsp code it self (examples and unit-test have submodules based code)

### Submodules used
- googletest
- Audiofile
- juce v8
- pffft

### Class design

- float based unless we really need precision for iterative algorithms
- mostly templates based for adaptive code and better optimisations.
- classes should ctor with the samplerate
- blockoperations of BlockSize=8 or 16 Samples for better compiler optimisations
- operations on simple buffer design with interleaved or mono array
- raw float operations allowed but with BlockSize only
- testability

## What
- Analysis (FFT, Yin pitch detection, spectrogram, envelope follower)
- Audio buffers, fader and fixed-size block processor building blocks
- Delays, Diffuser and Reverbs (FDN with Hadamard mixing)
- Filters (biquad, ladder, SVF bandpass, one-pole)
- Generators (naive and band-limited) and Wavetables
- Modulation (wow/flutter)
- Non-linear (hysteresis / saturation)
- Parameter smoothing and ramping
- Sample playback, pitch/time-stretching and sample-rate conversion
- Numbers: math/conversion helpers (interpolation, easing, dB/frequency)
- WAV/OGG file I/O



### Usage

For usage check always the unit-tests or examples, these contain implementations that should cover and 
which should be self-explanatory.

## Testing

Unit tests use GoogleTest/CTest (see CLAUDE.md for build commands, or `dev-scripts/`
for wrapper scripts that build and run tests without cd-ing around).

### Test coverage

`./check_test_coverage.sh` is a fast static check (no build required). It walks
the local `#include` graph starting from every `test/**/*_test.cpp` file and
flags any `src` header that is never reached, directly or transitively. This
means grouped test files (one file testing several related headers) and
headers only reached indirectly (e.g. a coefficient table pulled in by the
filter that uses it) are recognized automatically, with no per-file
bookkeeping. It also reports:

- **empty test files** (a `*_test.cpp` exists but has no `TEST`/`TEST_F`/
  `TEST_P`/`TYPED_TEST`/`INSTANTIATE_TEST_SUITE_P`),
- **naming mismatches** (a test file that includes exactly one project header
  but isn't named after it), and
- **exceptions hygiene**: headers intentionally excluded from the check are
  declared with a reason in `test/coverage_exceptions.txt`; an entry that has
  become reachable (stale) or points at a header that no longer exists
  (invalid) fails the check, so that file can't silently drift out of date.

A Markdown summary is regenerated at `test/COVERAGE_GAPS.md` on every run
(auto-generated, do not hand-edit).

For real line/branch coverage (which lines are actually exercised, not just
whether a test file exists), run:
```bash
./dev-scripts/dev-coverage.sh
# or: ./check_test_coverage.sh -c
```
This configures a separate `build-coverage/` directory with `-DENABLE_COVERAGE=ON`,
builds and runs all unit tests instrumented with `--coverage`, and generates a
per-file Markdown summary at `COVERAGE.md` in the repo root, plus a full annotated
HTML report at `build-coverage/coverage/index.html`. Requires `gcovr`
(`brew install gcovr` or `pip install gcovr`).

### Why Valgrind, not AddressSanitizer

Memory checking is done with Valgrind, run through `docker-unit-tests/run-on-mac.sh`
(a Linux container is used because Valgrind itself has no native Apple Silicon build).
AddressSanitizer was tried as a native, faster alternative, but its dynamic runtime
currently hangs during process startup on this toolchain (Apple clang 17 / macOS 26),
independent of anything in this codebase. Stick with Valgrind for now; revisit ASan
once that toolchain issue is fixed upstream, or run it inside the Linux container
instead of natively.

## IDE Setup

### clangd / static analysis

The repo includes a `.clangd` file that points clangd to `cmake-build-debug/compile_commands.json`,
so no symlink is needed. You only need to create that build directory once:

**Tests-only build** (no JUCE required):
```bash
mkdir cmake-build-debug && cd cmake-build-debug
cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ..
```

**Full build** (includes JUCE examples — required for metronome, reverb, etc.):
```bash
mkdir cmake-build-debug && cd cmake-build-debug
cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DBUILD_FULL_PROJECT=ON ..
```

After configuring, restart your language server (or reopen the project). With the full build
the IDE will resolve all JUCE headers and the `AbacDsp` includes inside `examples/`.
