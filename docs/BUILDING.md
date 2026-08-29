# Building, testing and documentation

## Building

By default CMake configures only the header-only library and its unit tests (no
external toolkits needed). Two option switches pull in the heavier, optional
parts of the tree:

- `-DBUILD_FULL_PROJECT=ON` builds the JUCE **example plugins** under `examples/`
  (metronome, reverbs, looper, ...). This needs the JUCE submodule and a longer
  build; leave it OFF for a fast tests-only build.
- `-DEXPLORE_STUFF=ON` builds the standalone **documentation explore programs**
  under `documentation/` (for example `documentation/Slicer/`, `VelvetNoise/`,
  `Filters/BandpassImpulses/`). These are small offline tools for prototyping and
  tuning DSP, not part of the library or its tests.

The two switches are independent and can be combined:

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..                          # library + tests only
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_FULL_PROJECT=ON ..  # + JUCE example plugins
cmake -DCMAKE_BUILD_TYPE=Release -DEXPLORE_STUFF=ON ..       # + documentation explore tools
cmake --build .
```

Each explore program has its own target (for example `SlicerExplore`), so you can
also build just one with `cmake --build . --target <name>`.

`./dev-scripts/` wraps this workflow for a tests-only `build-tests/` directory
(configure/build/test/warnings without cd-ing around) - see the top-level
`CLAUDE.md` for the individual scripts.

`tapelooper`'s optional drum-groove playback (an alternative to its synthetic click) needs a
user-supplied sample kit and MIDI groove file under `samples/drums/` and `MidiDrums/`
respectively - both third-party, copyrighted content not included in this repository. See
`samples/README.md` and `MidiDrums/README.md` for the expected layout.

## Testing

Unit tests use GoogleTest/CTest (see `CLAUDE.md` for build commands, or `dev-scripts/`
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

## API documentation

Every public type in `src/includes/` carries a Doxygen brief. Build the site
with:
```bash
./dev-scripts/dev-docs.sh          # writes docs/doxygen/html/index.html
./dev-scripts/dev-docs.sh --open   # and opens it
```
Requires `doxygen` (`brew install doxygen`); graphviz is used for inheritance
graphs if present. Output goes to `docs/doxygen/html/` and is gitignored; the
config (`docs/doxygen/Doxyfile`), the vendored theme (`docs/doxygen/theme/`)
and the module pages (`docs/doxygen/groups.dox`, `docs/doxygen/mainpage.dox`)
are tracked.

The build runs with `WARN_AS_ERROR=FAIL_ON_WARNINGS`, so a malformed command or
an unresolvable cross-reference fails rather than producing a quietly wrong page.

Comments document a class's own contract and the reasoning behind it, not who
calls it; use cases belong in separate documents. External citations point at
stable sources and are collected in `WEB-REFERENCES.md`, checked with:
```bash
./dev-scripts/dev-check-urls.sh    # every URL in src/includes and WEB-REFERENCES.md
```
