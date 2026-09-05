# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.
Some of these rules get constantly broken by Claude: the discipline of claude is overall bad in following instruction 
on how to comment code, and often going ahead with tasks without being explicitly asked. When the user asks a 
question, Claude should answer the question, but NEVER go ahead implicitly; instead, wait for instructions.

## Agents

**Never** launch agents in this project.


## Project Overview

**AbacDsp** is a header-only C++20 DSP library for audio processing. The core library in `src/includes/` has zero external dependencies. JUCE is only used for the example plugins in `examples/`.

## Build Commands

**Always use `./dev-scripts/` for building, testing, configuring, coverage and warning
checks instead of raw `cmake`/`ctest`/compiler invocations.** This applies to planning
too: when a task's plan includes a build or test step, name the specific dev-script for
it rather than a generic "build/test" step; only fall back to a raw command if no script
covers the case, and say so explicitly.

- `dev-configure.sh` - configure `build-tests/` (core library + unit tests, no JUCE).
- `dev-build.sh [target...]` - build one or more targets in `build-tests/` (default:
  `run_unit_tests`).
- `dev-test.sh [ctest-regex]` - build and run tests via ctest in `build-tests/`.
- `dev-test-full.sh <target...> [-- ctest-regex]` - build and run tests for anything
  under `BUILD_FULL_PROJECT` (a JUCE example plugin target, its own unittests, etc.) in
  `cmake-build-debug/`; pass a regex that matches nothing (e.g. `-- "^$"`) for a
  build-only check without running the test suite.
- `dev-warnings.sh` - clean rebuild of `build-tests/`, reporting every non-3rdparty
  compiler warning.
- `dev-coverage.sh` / `dev-cov-file.sh` - full or single-file/single-target line/branch
  coverage (see Documentation section of `README.md` for detail).
- `dev-explore.sh [source.cpp]` / `dev-explore-lua.sh` - throwaway DSP/Lua experiments
  against the header-only library, built in the gitignored `explore/`.
- `dev-docs.sh` - build the Doxygen API site into `docs/doxygen/html/`.
- `dev-check-urls.sh` - verify documentation reference URLs resolve.

Key CMake options are declared in the top-level `CMakeLists.txt` (`option(...)` calls).

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

Header-only, organized by DSP domain into per-topic subdirectories (see `ls src/includes/`).


### Key Design Patterns

**Block-based processing:** Classes operate on fixed block sizes via template parameter. Use `std::array<float, BlockSize>` rather than raw pointers. Block sizes of 8–16 samples are typical for SIMD optimization.

**Template-heavy:** Filter type, wave shape, etc. are template parameters for zero-cost abstractions:
```cpp
Biquad<BiquadFilterType::LowPass> lp(44100.0f);
```

**Constructor takes sample rate:** All stateful processors are initialized with `float sampleRate`. No separate `prepare()` call needed.

**No `.cpp` files in core:** Everything is in headers. The optional `audio_dsp_lib` CMake target compiles any implementation `.cpp` files found in subdirectories for compile-time optimization.

### Tests (`test/`)

Test files follow `*_test.cpp` naming and mirror the `src/includes/` structure. CMake auto-discovers them.

### Examples (`examples/`)

JUCE-based audio plugin examples. Each example is self-contained with its own CMakeLists.txt.

### Third-Party (`3rdparty/`)

All are git submodules (see `.gitmodules`).

## Code Style

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
- functions and methods: max 3 lines of comment
- classes: max 12 lines of comment
- flow comments (if then else, variables, constants etc.): max 2 lines of comment

### General Robustness
- Zero-initialize structs at declaration: `MyStruct s{}`.
- Avoid static local mutable state; prefer dependency injection.
- Keep headers clean: use `inline` variables, avoid `extern` for constants.
- All compiler warnings must be clean (`-Wall -Wextra -Wpedantic`); never disable a warning without a comment explaining why.


### Generated files (`examples/*/src/`)
See `.claude/rules/generated-files.md` (loads automatically when working under `examples/*/src/`): generated files must never be hand-edited; edit the blueprint JSON or the generator templates instead.

## Documentation
- Check after a commit if the README.md needs to be updated.
- Keep the documentation always concise and precise. Prefer usage of ASCII (except when using math formulas). Don't overuse bold, the reader should not be forced in a mental model that is driven by bold text.
- Every Lua-scripted example (`"use-lua": true` in its blueprint) needs a `README.md` with a
  "Scripting" section documenting its own custom Lua API (bound functions, hooks, table
  shapes) - see `examples/dronesequencer/README.md`, `examples/resonik/README.md`, or
  `examples/pingsynth/README.md` for the shape to follow. This is not optional polish: it is
  the reference a human (or an LLM-assist session) needs to write a working script for that
  example at all, and it belongs alongside root `LUA-MANUAL.md` (the API shared by every
  such example) rather than only inline in the engine header's script-skeleton comments.
