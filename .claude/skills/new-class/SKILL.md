---
description: Create a new C++20 class with a matching GoogleTest skeleton. Use when asked to add a new class or component.
argument-hint: <ClassName>
---

Create a C++20 class `$ARGUMENTS` following project conventions:

Header (`$ARGUMENTS.h`):
- `#pragma once`
- Trailing `_` on all member variables
- Minimal public interface, prefer private implementation
- Use concepts/requires for template constraints

Source (`$ARGUMENTS.cpp`):
- Minimal, self-explaining implementation
- No redundant comments

Test file (`$ARGUMENTS_test.cpp`):
- One `TEST_F` with a fixture if shared state is needed, `TEST` otherwise
- Fixture class named `$ARGUMENTSTest`
- One skeleton test per public method, clearly named

CMakeLists.txt:
- Add a `target_sources` or new `add_executable` entry for the test binary linked against `GTest::gtest_main`

Leave TODOs only where logic is genuinely non-trivial. No implementation — skeleton only.
