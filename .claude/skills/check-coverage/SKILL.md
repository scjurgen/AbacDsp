---
description: Build with coverage instrumentation, run tests via ctest, and summarize uncovered branches.
disable-model-invocation: true
allowed-tools: Bash(cmake *) Bash(ctest *) Bash(gcov *) Bash(lcov *) Bash(genhtml *)
---

1. Reconfigure with coverage flags:
   ```
   cmake -B build-cov -DCMAKE_CXX_FLAGS="--coverage" -DCMAKE_BUILD_TYPE=Debug
   cmake --build build-cov
   ```
2. Run: `ctest --test-dir build-cov --output-on-failure`
3. Collect:
   ```
   lcov --capture --directory build-cov --output-file coverage.info
   lcov --remove coverage.info '*/test/*' '/usr/*' --output-file coverage_filtered.info
   ```
4. Report:
   - Files with branch coverage below 80%
   - Uncovered functions by file
   - No HTML output unless explicitly requested
