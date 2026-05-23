---
description: Refactor a C++ source file to idiomatic C++20 without changing behavior.
argument-hint: <filename>
---

Read `$ARGUMENTS`. Apply C++20 improvements where they reduce boilerplate or improve safety:

- `std::span<T>` over raw pointer + size pairs
- Concepts / `requires` over SFINAE or `std::enable_if`
- Ranges (`std::ranges::*`) over hand-written index loops where intent is clearer
- `std::format` over `printf` / `ostringstream`
- Structured bindings for pair/tuple destructuring
- `[[nodiscard]]` on functions whose return value must not be ignored
- `[[likely]]` / `[[unlikely]]` on hot branches where profiling justifies it
- `constexpr` / `consteval` where applicable
- Designated initializers for aggregate init

Do not change:
- Template definitions or parameters
- Namespaces
- Public API signatures
- Observable behavior

Produce a complete, drop-in replacement file. No inline comments explaining the changes.
