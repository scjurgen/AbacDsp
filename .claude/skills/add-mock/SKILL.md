---
description: Generate a GoogleMock for a given interface or abstract class.
argument-hint: <InterfaceName>
---

Read the interface or abstract class `$ARGUMENTS` from the codebase.

Generate `Mock$ARGUMENTS`:
- One `MOCK_METHOD` per virtual/pure-virtual method
- Match C++20 signatures exactly — do not alter templates, namespaces, or parameter types
- `const` overloads mocked separately
- Place in `test/mocks/Mock$ARGUMENTS.h` (create path if absent)
- Include only `<gmock/gmock.h>` and the original header

Drop-in header only. No usage examples.
