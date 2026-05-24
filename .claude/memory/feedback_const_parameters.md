---
name: feedback_const_parameters
description: All function/lambda parameters should be const unless they are actually modified in the body
metadata:
  type: feedback
---

Declare every function and lambda parameter `const` by default unless the parameter is actually mutated inside the function body.

**Why:** User's explicit style preference — const communicates intent, prevents accidental mutation, and is required unless modification is needed.

**How to apply:** On every new or edited function/lambda, mark all parameters `const`. Only omit `const` if the parameter is written to or passed to a non-const reference. Apply proactively; do not wait to be asked.
