#!/usr/bin/env python3
"""Inject a "Decisions" row into the overall table of a gcovr markdown report.

gcovr's markdown format emits only Lines/Functions/Branches. Decision coverage
(source-level if / ?: / && / || / switch, without the float/SIMD/library/throw
branch noise) is more meaningful for this header-only library, so add it to the
overall table next to Branches.

Usage: inject_decisions.py <build-dir> <coverage.md>
"""

from __future__ import annotations

import json
import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INCLUDE_ROOT = ROOT / "src" / "includes"


def decision_summary(build_dir: Path) -> tuple[int, int] | None:
    with tempfile.NamedTemporaryFile(suffix=".json", delete=False) as tmp:
        out_path = Path(tmp.name)
    try:
        # --json-summary takes an optional value; pass the path via -o so the
        # build dir stays positional instead of being read as the output file.
        cmd = [
            "gcovr", "--root", str(ROOT), "--filter", f"{INCLUDE_ROOT}/",
            "--exclude-unreachable-branches", "--exclude-throw-branches", "--decisions",
            "--json-summary", "-o", str(out_path), str(build_dir),
        ]
        out = subprocess.run(cmd, capture_output=True, text=True)
        if out.returncode != 0:
            return None
        try:
            summary = json.loads(out_path.read_text())
        except json.JSONDecodeError:
            return None
    finally:
        out_path.unlink(missing_ok=True)
    covered, total = summary.get("decision_covered"), summary.get("decision_total")
    if covered is None or total is None:
        return None
    return covered, total


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: inject_decisions.py <build-dir> <coverage.md>", file=sys.stderr)
        return 2
    build_dir = Path(sys.argv[1])
    md_path = Path(sys.argv[2])
    if not md_path.exists():
        return 0

    result = decision_summary(build_dir)
    if result is None:
        return 0
    covered, total = result
    pct = 100 * covered / total if total else 0.0
    marker = "\U0001F7E2" if pct >= 90 else ("\U0001F7E1" if pct >= 75 else "\U0001F534")
    row = f"| **Decisions** | {marker} {covered}/{total} ({pct:.1f}%) |"

    lines = md_path.read_text().splitlines()
    out_lines: list[str] = []
    branches_re = re.compile(r"^\|\s*\*\*Branches\*\*\s*\|")
    injected = False
    for line in lines:
        if line.strip().startswith("| **Decisions**"):
            continue  # drop any previously injected row so re-runs stay idempotent
        out_lines.append(line)
        if not injected and branches_re.match(line):
            out_lines.append(row)
            injected = True
    md_path.write_text("\n".join(out_lines) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
