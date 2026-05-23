---
description: Stage and commit changes using this project's commit conventions
---

Commit all staged changes (or stage and commit everything if nothing is staged).

Steps:
1. Run `git status` and `git diff --staged` to understand what is staged.
   If nothing is staged, run `git diff` to see unstaged changes, then stage all modified and untracked files with `git add`.
2. Analyse the changes and write a commit message following these rules:
   - Subject line: short (≤ 72 chars), lowercase, imperative mood, no trailing period
   - If the change is non-trivial, add a blank line followed by a bullet-point body
   - **Never** add a `Co-Authored-By` line or any trailer lines
   - Match the terse style of this project's history (e.g. "add tests for X", "fix include path in Y", "overhaul Z")
3. Run `git commit -m "..."` with the message via a heredoc to preserve formatting.
4. Confirm the commit was created with `git log --oneline -1`.

Optional hint from user: $ARGUMENTS
