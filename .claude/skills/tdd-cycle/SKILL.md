---
description: Drive a TDD red-green-refactor cycle for a specific function or method. Use when asked to implement something test-first.
argument-hint: <ClassName::methodName or free function>
disable-model-invocation: true
---

For `$ARGUMENTS`, follow strict TDD one cycle at a time:

1. **Red** — write the minimal failing test. Must either not compile or fail an assertion.
2. **Green** — write the minimal production code to make it pass. No over-engineering.
3. **Refactor** — apply C++20 idioms where they reduce noise: `std::span`, ranges, concepts,
   `std::format`, structured bindings, `[[nodiscard]]`. Do not change behavior.

Output one cycle per response. After Green, pause and ask before Refactoring if scope is unclear.
Continue with the next behavior only after the current cycle is complete.

Use GoogleTest (`EXPECT_*` / `ASSERT_*`). Prefer `EXPECT_*` unless a failed assertion makes
further test execution meaningless.
