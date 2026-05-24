---
name: feedback_function_length
description: Decompose long functions using named lambdas (preferred ≤10 lines) or private functions (>10 lines, soft rule)
metadata:
  type: feedback
---

When a function grows too long, split it into named lambdas or private functions. Prefer lambdas for short extracted pieces (≤10 lines, soft rule); use private member functions for longer ones.

**Why:** User's explicit style preference — naming replaces comments, and smaller units of logic are easier to read.

**How to apply:** If a function body exceeds ~10–15 lines, look for cohesive sub-operations that can be named. Define them as local lambdas just before the loop or block that uses them. Only promote to a private function if the body is long enough to make a lambda unwieldy.
