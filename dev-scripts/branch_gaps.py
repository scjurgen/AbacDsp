#!/usr/bin/env python3
"""Actionable branch-coverage gap report for AbacDsp.

Raw gcov/gcovr branch coverage on this header-only, template-heavy, SIMD-heavy
library is a mix of two very different things:

  1. Real, source-level decisions that no test exercises (an unhit `if`, a
     never-taken early-return, one overload that is never called). These are
     genuine gaps worth closing with a targeted test.
  2. Machine-level "branches" that gcov attributes to straight-line floating
     point, SIMD intrinsics, FFT butterflies and constexpr array construction.
     No test can ever reach these; counting them as missing coverage is noise.

This tool separates the two. It reads the gcovr JSON produced from the coverage
build, classifies every uncovered branch by whether its source line contains a
real decision keyword, and reports the genuine gaps ranked by file. Headers
that are 100% category (2) are listed in test/branch_coverage_exceptions.txt
and dropped from the actionable denominator (their line/function coverage is
still tracked normally in COVERAGE.md).

Usage:
  dev-scripts/branch_gaps.py                 # summary + ranked gap list, writes report
  dev-scripts/branch_gaps.py --file OnePole  # per-line uncovered branches for a header
  dev-scripts/branch_gaps.py --json cov.json # reuse an existing gcovr json (skip gcovr)
  dev-scripts/branch_gaps.py --no-report     # do not (re)write test/BRANCH_GAPS.md
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INCLUDE_ROOT = ROOT / "src" / "includes"
BUILD_DIR = ROOT / "build-coverage"
EXCEPTIONS_FILE = ROOT / "test" / "branch_coverage_exceptions.txt"
REPORT_FILE = ROOT / "test" / "BRANCH_GAPS.md"

RED = "\033[0;31m"
GREEN = "\033[0;32m"
YELLOW = "\033[1;33m"
BLUE = "\033[0;34m"
CYAN = "\033[0;36m"
NC = "\033[0m"

# A source line carries a real, testable decision if it mentions one of these.
# Anything else that gcov flagged as a branch (bare float math, an intrinsic
# call, a constexpr expansion) is treated as machine-level noise.
LOGIC_RE = re.compile(r"\bif\b|\bfor\b|\bwhile\b|\bswitch\b|\bcase\b|\?|&&|\|\|")


@dataclass
class LineGap:
    line: int
    uncovered: int
    total: int
    text: str
    is_logic: bool


@dataclass
class FileGaps:
    rel: str  # relative to src/includes
    branch_total: int = 0
    branch_covered: int = 0
    logic_uncovered: int = 0
    phantom_uncovered: int = 0
    gaps: list[LineGap] = field(default_factory=list)


def run_gcovr_json(build_dir: Path) -> dict:
    import tempfile
    with tempfile.NamedTemporaryFile(suffix=".json", delete=False) as tmp:
        out_path = Path(tmp.name)
    try:
        # --json takes an optional value, so hand it the output path via -o and
        # keep the build dir strictly positional to avoid it being swallowed.
        cmd = [
            "gcovr", "--root", str(ROOT), "--filter", f"{INCLUDE_ROOT}/",
            "--exclude-unreachable-branches", "--exclude-throw-branches",
            "--json", "-o", str(out_path), str(build_dir),
        ]
        out = subprocess.run(cmd, capture_output=True, text=True)
        if out.returncode != 0:
            sys.exit(f"{RED}gcovr failed:{NC}\n{out.stderr}")
        return json.loads(out_path.read_text())
    finally:
        out_path.unlink(missing_ok=True)


def parse_exceptions() -> dict[str, str]:
    exceptions: dict[str, str] = {}
    if not EXCEPTIONS_FILE.exists():
        return exceptions
    for raw in EXCEPTIONS_FILE.read_text().splitlines():
        line = raw.strip()
        if not line or line.startswith("#") or ":" not in line:
            continue
        path, reason = line.split(":", 1)
        rel = path.strip()
        rel = rel[len("src/includes/"):] if rel.startswith("src/includes/") else rel
        exceptions[rel] = reason.strip()
    return exceptions


def collect(data: dict) -> dict[str, FileGaps]:
    files: dict[str, FileGaps] = {}
    for f in data["files"]:
        abspath = Path(f["file"])
        try:
            rel = abspath.resolve().relative_to(INCLUDE_ROOT).as_posix()
        except ValueError:
            continue
        src = abspath.read_text(errors="replace").splitlines() if abspath.exists() else []
        fg = FileGaps(rel=rel)
        for ln in f["lines"]:
            brs = ln.get("branches", [])
            if not brs:
                continue
            cov = sum(1 for b in brs if b["count"] > 0)
            fg.branch_total += len(brs)
            fg.branch_covered += cov
            unc = len(brs) - cov
            if unc == 0:
                continue
            n = ln["line_number"]
            text = src[n - 1].strip() if n <= len(src) else ""
            is_logic = bool(LOGIC_RE.search(text))
            if is_logic:
                fg.logic_uncovered += unc
            else:
                fg.phantom_uncovered += unc
            fg.gaps.append(LineGap(n, unc, len(brs), text, is_logic))
        if fg.branch_total:
            files[rel] = fg
    return files


def validate_exceptions(files: dict[str, FileGaps], exceptions: dict[str, str]) -> tuple[list[str], list[str]]:
    stale, invalid = [], []
    for rel in exceptions:
        fg = files.get(rel)
        if fg is None or fg.branch_total == 0:
            invalid.append(rel)
        elif fg.logic_uncovered > 0:
            stale.append(rel)
    return stale, invalid


def print_file_detail(fg: FileGaps) -> None:
    print(f"\n{BLUE}{fg.rel}{NC}  branches {fg.branch_covered}/{fg.branch_total}"
          f"  (logic gaps {fg.logic_uncovered}, phantom {fg.phantom_uncovered})")
    for g in fg.gaps:
        tag = f"{RED}LOGIC {NC}" if g.is_logic else f"{YELLOW}noise {NC}"
        print(f"  {tag} L{g.line:<4} {g.uncovered}/{g.total} uncovered  {g.text}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--build-dir", default=str(BUILD_DIR), help="coverage build directory (default build-coverage)")
    parser.add_argument("--json", help="reuse an existing gcovr json instead of running gcovr")
    parser.add_argument("--file", help="show per-line uncovered branches for headers matching this substring")
    parser.add_argument("--no-report", action="store_true", help=f"do not (re)write {REPORT_FILE.relative_to(ROOT)}")
    args = parser.parse_args()

    if args.json:
        data = json.loads(Path(args.json).read_text())
    else:
        data = run_gcovr_json(Path(args.build_dir))

    files = collect(data)
    exceptions = parse_exceptions()

    if args.file:
        matches = [fg for rel, fg in sorted(files.items()) if args.file.lower() in rel.lower()]
        if not matches:
            print(f"{RED}no covered header matches '{args.file}'{NC}")
            return 1
        for fg in matches:
            print_file_detail(fg)
        return 0

    raw_total = sum(f.branch_total for f in files.values())
    raw_cov = sum(f.branch_covered for f in files.values())
    exempt_total = sum(f.branch_total for rel, f in files.items() if rel in exceptions)
    exempt_cov = sum(f.branch_covered for rel, f in files.items() if rel in exceptions)
    act_total = raw_total - exempt_total
    act_cov = raw_cov - exempt_cov
    logic_gaps = sum(f.logic_uncovered for rel, f in files.items() if rel not in exceptions)
    phantom_gaps = sum(f.phantom_uncovered for rel, f in files.items() if rel not in exceptions)

    stale, invalid = validate_exceptions(files, exceptions)

    print(f"{BLUE}{'=' * 64}{NC}")
    print(f"{BLUE} Branch-coverage gap analysis{NC}")
    print(f"{BLUE}{'=' * 64}{NC}")
    print(f"Raw branches:            {raw_cov}/{raw_total} ({100 * raw_cov / raw_total:.1f}%)")
    print(f"Exempt (noise) headers:  {len(exceptions)} files, {exempt_total} branches set aside")
    print(f"Actionable branches:     {act_cov}/{act_total} ({100 * act_cov / act_total:.1f}%)")
    print(f"  real logic gaps left:  {RED}{logic_gaps}{NC}")
    print(f"  phantom in kept files: {YELLOW}{phantom_gaps}{NC} (float/SIMD noise inside otherwise-real headers)")

    ranked = sorted((f for rel, f in files.items() if rel not in exceptions and f.logic_uncovered),
                    key=lambda f: f.logic_uncovered, reverse=True)
    print(f"\n{BLUE}Top files by real logic-branch gaps:{NC}")
    for fg in ranked[:20]:
        print(f"  {RED}{fg.logic_uncovered:4d}{NC} logic  ({fg.phantom_uncovered:3d} noise)  "
              f"{fg.branch_covered}/{fg.branch_total}  {fg.rel}")

    if stale:
        print(f"\n{RED}STALE exceptions (now have a real logic gap; re-classify or write a test):{NC}")
        for rel in stale:
            print(f"  {RED}- {rel}{NC}")
    if invalid:
        print(f"\n{RED}INVALID exceptions (no such header / no branches):{NC}")
        for rel in invalid:
            print(f"  {RED}- {rel}{NC}")

    if not args.no_report:
        write_report(ranked, exceptions, raw_cov, raw_total, act_cov, act_total, logic_gaps)
        print(f"\n{BLUE}Report written to {REPORT_FILE.relative_to(ROOT)}{NC}")

    return 0 if not stale and not invalid else 1


def write_report(ranked, exceptions, raw_cov, raw_total, act_cov, act_total, logic_gaps) -> None:
    lines = [
        "# Branch Coverage Gaps",
        "",
        "Auto-generated by `dev-scripts/branch_gaps.py`. Do not hand-edit; edit"
        " `test/branch_coverage_exceptions.txt` and re-run the script.",
        "",
        f"- Raw branches: {raw_cov}/{raw_total} ({100 * raw_cov / raw_total:.1f}%)",
        f"- Actionable (noise headers excluded): {act_cov}/{act_total} ({100 * act_cov / act_total:.1f}%)",
        f"- Real logic-branch gaps remaining: **{logic_gaps}**",
        "",
        "## Files with real logic-branch gaps (write targeted tests)",
        "",
        "| logic gaps | noise | branches | file |",
        "|-----------:|------:|:--------:|------|",
    ]
    for fg in ranked:
        lines.append(f"| {fg.logic_uncovered} | {fg.phantom_uncovered} | "
                     f"{fg.branch_covered}/{fg.branch_total} | `{fg.rel}` |")
    lines += ["", "## Exempt headers (branch noise, not counted)", ""]
    for rel, reason in sorted(exceptions.items()):
        lines.append(f"- `{rel}` - {reason}")
    lines.append("")
    REPORT_FILE.write_text("\n".join(lines))


if __name__ == "__main__":
    sys.exit(main())
