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
