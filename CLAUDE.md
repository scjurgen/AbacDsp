# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**AbacDsp** is a header-only C++20 DSP library for audio processing. The core library in `src/includes/` has zero external dependencies. JUCE is only used for the example plugins in `examples/`.

## Build Commands

```bash
# Tests only (default, no JUCE needed)
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --target run_unit_tests

# Full project with JUCE examples
cmake -DBUILD_FULL_PROJECT=ON -DCMAKE_BUILD_TYPE=Release ..
cmake --build .

# Run tests via Docker with Valgrind (auto-detects ARM64/x86_64)
./docker-unit-tests/run-on-mac.sh
```

Key CMake options: `BUILD_FULL_PROJECT` (OFF), `PACKAGE_TESTS` (ON), `PERFORMANCE_TESTS` (OFF), `EXPLORE_STUFF` (OFF).

## Running a Single Test

Tests use GoogleTest. After building:
```bash
ctest -V --output-on-failure -R <TestName>
```

Check test coverage against source files:
```bash
./check_test_coverage.sh
```

## Architecture

### Core Library (`src/includes/`)

Header-only, organized by DSP domain:
- `Analysis/` — FFT (via pffft), Yin pitch detection, spectrogram, envelope follower
- `Filters/` — Biquad (all types), ladder, SVF bandpass, one-pole
- `Reverbs/` — FDN with Hadamard mixing; 4/8/16/32 delay line variants
- `Generators/` / `NaiveGenerators/` — Band-limited and naive waveform generators
- `Delays/` — Delay lines with interpolation
- `Modulation/` — Wow/flutter modulation
- `Parameters/` — Parameter smoothing and ramping
- `Numbers/` — Math utilities: interpolation, easing, dB/frequency conversions
- `Wavetables/` — Wavetable oscillators
- `Diffuser/` — Schroeder diffusers, allpass chains

### Key Design Patterns

**Block-based processing:** Classes operate on fixed block sizes via template parameter. Use `std::array<float, BlockSize>` rather than raw pointers. Block sizes of 8–16 samples are typical for SIMD optimization.

**Template-heavy:** Filter type, wave shape, etc. are template parameters for zero-cost abstractions:
```cpp
Biquad<BiquadFilterType::LowPass> lp(44100.0f);
```

**Constructor takes sample rate:** All stateful processors are initialized with `float sampleRate`. No separate `prepare()` call needed.

**No `.cpp` files in core:** Everything is in headers. The optional `audio_dsp_lib` CMake target compiles any implementation `.cpp` files found in subdirectories for compile-time optimization.

### Tests (`test/`)

Test files follow `*_test.cpp` naming and mirror the `src/includes/` structure. CMake auto-discovers them. 64 test files across 18 categories.

### Examples (`examples/`)

JUCE-based audio plugin examples. Each example is self-contained with its own CMakeLists.txt. Key ones: `plaingain` (passthrough with metering), `guisandbox` (UI experimentation), `minireverb` (FDN reverb demo).

### Third-Party (`3rdparty/`)

All are git submodules: `googletest`, `JUCE` (v8), `AudioFile` (WAV I/O), `pffft` (SIMD FFT).

## Code Style

Standard: **C++20**. Compiler flags: `-Wall -Wextra -Wpedantic`. clang-format configured (`.clang-format` present).
Float-based by default; templates for type flexibility; block operations preferred over sample-by-sample.

### Realtime
dsp code is realtime, no allocations (if really needed we implement our own memory handling with a pool)
Optimise code in hot pathes.

### Type Safety
- Use `std::variant` + `std::visit` instead of raw unions or `void*`.
- Prefer `enum class` over unscoped `enum` to avoid implicit conversions.
- Constrain templates with **concepts** (`concept` / `requires`): errors surface at the call site, not inside the implementation.
- Use `{}`-initialization everywhere to get compile-time narrowing errors; never allow implicit narrowing.
- Use `std::span` over raw pointer + size pairs for non-owning views.
- Use `std::string_view` for read-only string parameters instead of `const std::string&`.

### Arrays & Containers
- Never use raw C arrays (`T arr[]` or `T arr[N]`). Always use `std::array<T, N>`.
- For `constexpr` tables whose size should be deduced, use `std::to_array<T>({...})`:
  ```cpp
  static constexpr auto kTable = std::to_array<MyStruct>({{val1, val2}, {val3, val4}});
  ```
- Never add a parallel `kNumFoo` constant. Use `.size()` at the call site; cast when needed: `static_cast<int>(kFoo.size()) - 1`.
- When indexing a `std::array` with a signed variable, cast at the subscript: `arr[static_cast<size_t>(i)]`.

### Resource Management (RAII)
- Never use raw `new` / `delete`. Use `std::unique_ptr` / `std::shared_ptr`.
- Own resources in objects with proper destructors; no manual cleanup paths.

### Functions & Interfaces
- Mark all single-argument constructors `explicit` unless implicit conversion is intentional.
- `[[nodiscard]]` on any function returning an error code or resource handle.
- Prefer return values over output parameters; use `std::optional` for nullable results.
- `noexcept` on functions that must not throw (destructors, move operations always).
- Pass by value when you'll own a copy; `const&` for read-only; `&&` for sink parameters.
- `const` on every member function that does not mutate state; `const` on every local variable and lambda parameter that is not reassigned.

### Immutability & Correctness
- `constexpr` / `consteval` aggressively for compile-time computation.
- `[[likely]]` / `[[unlikely]]` for branch hints in hot DSP paths.
- Always mark overrides with `override` (and `final` where appropriate); never rely on implicit virtual dispatch correctness.
- `const`-correctness everywhere: local variables, member functions, pointer targets.

### Error Handling
- Use exceptions for truly exceptional conditions; `std::optional` (or `std::expected` when available) for expected failure paths.
- Never swallow exceptions silently in a `catch` block.
- Validate preconditions with `assert()` in debug builds.

### Concurrency
- Never access shared data without synchronization; prefer `std::atomic` for simple flags and counters.
- Use `std::jthread` over `std::thread` (auto-joins, supports stop tokens).
- Never use `volatile` for thread safety — it provides no synchronization guarantees.

### Modern C++20 Features
- Use `std::ranges::` algorithms over raw iterator pairs.
- Prefer structured bindings (`auto [a, b] = ...`) for multi-value returns.
- Use `std::format` over `printf` / `std::stringstream` for type-safe formatting.
- Implement comparisons via `<=>` (three-way comparison) to derive all operators from one definition.

### Function Size & Decomposition
- Keep functions short and single-purpose. If a function no longer fits comfortably on one screen, split it.
- If a block of code inside a function needs a comment to explain what it does, extract it into a named private function or a local lambda instead — the name replaces the comment.
- Prefer local lambdas for self-contained logic that is only used in one place and captures its context naturally:
  ```cpp
  const auto clampedGain = [&]() noexcept { return std::clamp(raw, kMinGain, kMaxGain); };
  ```
- Prefer private member functions when the logic is reusable, testable, or non-trivial enough to deserve its own name in the class interface.

### Comments
- Write comments sparingly. Well-named identifiers and small functions are the primary documentation.
- Only add a comment when the **why** is non-obvious: a hidden constraint, a subtle invariant, a known hardware quirk, or a workaround for a specific external bug.
- Never write comments that describe **what** the code does — if the code needs that explanation, rename or restructure it.
- Do not reference the task, PR, or caller in comments (`// added for issue #123`, `// called by Foo`); that belongs in the commit message.
- Comments should be relevant for the current state, remove comments that are non relevant planning decisions. The Status Quo is important, not anecdotes.
- A class comment documents its own contract and invariants, not where its data comes from or who calls it; that context can change without the class itself changing. This matters most for library classes.

### General Robustness
- Zero-initialize structs at declaration: `MyStruct s{}`.
- Avoid static local mutable state; prefer dependency injection.
- Keep headers clean: use `inline` variables, avoid `extern` for constants.
- All compiler warnings must be clean (`-Wall -Wextra -Wpedantic`); never disable a warning without a comment explaining why.


### Generated files (`examples/*/src/`)
Files regenerated by `JuceStandaloneGenerator/generate-juce-standalone.py` (Processor.h, Editor.h, PatchParameters.h, GenericImpl.h, FileIo.h, Constants.h, CcMapping.h, CcSettings.h, UiElements.h) must **not** be edited by hand. All UI and parameter changes go in the blueprint JSON (`JuceStandaloneGenerator/blueprints/<name>.json`); then re-run the generator. The implementation file (`impl/<Name>Impl.h`) is hand-written and is never overwritten by the generator.
