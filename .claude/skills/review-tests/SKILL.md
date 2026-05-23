---
description: Review GoogleTest unit tests for quality, coverage gaps, and mock misuse.
context: fork
agent: Explore
---

Review all `*_test.cpp` files reachable from the current directory.

Check for:
- Tests asserting implementation details instead of observable behavior
- Missing edge cases: null/empty/boundary values, error paths
- Overcomplicated mocks — prefer real objects or fakes where feasible
- `TEST` vs `TEST_F` misuse
- `EXPECT_*` vs `ASSERT_*` misuse
- Tests with multiple unrelated assertions that should be split
- Fixture state leaking between tests
- Missing `EXPECT_CALL` verification for mocks that matter

Report findings grouped by file with line numbers. No fixes — observations only.
