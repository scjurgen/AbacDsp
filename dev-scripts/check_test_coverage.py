#!/usr/bin/env python3
"""Structural test coverage checker for AbacDsp.

Unlike a naive "does Foo.h have a matching Foo_test.cpp" name match, this
walks the *actual* local #include graph: a header counts as structurally
covered if it is reachable, directly or transitively, from the #include
statements of some file under test/. That means:

  - Grouped test files (e.g. one test file including four Hadamard variants)
    correctly mark all four as covered.
  - A header only ever reached indirectly (e.g. a coefficient data table
    #include'd by the filter that uses it) is covered once that filter has
    a test, with no per-file bookkeeping needed.
  - Renaming a header does not silently "un-cover" it and does not require
    renaming the test file for the check to keep passing (though a naming
    mismatch is still reported so it can be cleaned up).

Headers that structurally can never be covered this way (e.g. a standalone
template scaffold that nothing includes) are declared in
test/coverage_exceptions.txt with a mandatory reason. That file is
cross-checked too: an entry for a header that *is* now reachable, or that
no longer exists, is reported as stale and fails the check, so the
exceptions list cannot drift the way test/COVERAGE_GAPS.md did.
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from collections import defaultdict
from dataclasses import dataclass, field
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INCLUDE_ROOT = ROOT / "src" / "includes"
SRC_ROOT = ROOT / "src"
TEST_ROOT = ROOT / "test"
EXCEPTIONS_FILE = TEST_ROOT / "coverage_exceptions.txt"
GAPS_REPORT = TEST_ROOT / "COVERAGE_GAPS.md"

RED = "\033[0;31m"
GREEN = "\033[0;32m"
YELLOW = "\033[1;33m"
BLUE = "\033[0;34m"
CYAN = "\033[0;36m"
ORANGE = "\033[0;33m"
NC = "\033[0m"

INCLUDE_RE = re.compile(r'^\s*#include\s*"([^"]+)"')
TEST_MACRO_RE = re.compile(
    r"^\s*(TEST|TEST_F|TEST_P|TYPED_TEST|TYPED_TEST_P|TYPED_TEST_SUITE|"
    r"INSTANTIATE_TEST_SUITE_P|REGISTER_TYPED_TEST_SUITE_P)\s*\(",
    re.M,
)


def rel_to_include_root(path: Path) -> str:
    return path.relative_to(INCLUDE_ROOT).as_posix()


def quoted_includes(path: Path) -> list[str]:
    includes = []
    for line in path.read_text(errors="replace").splitlines():
        m = INCLUDE_RE.match(line)
        if m:
            includes.append(m.group(1))
    return includes


def resolve_include(spec: str, from_dir: Path | None = None) -> str | None:
    """Resolve a quoted #include target the way the compiler would: relative to the
    including file's own directory first, then relative to the src/includes search path.
    Returns a path relative to src/includes, or None if it doesn't resolve under there."""
    search_dirs = []
    if from_dir is not None:
        search_dirs.append(from_dir)
    search_dirs.append(INCLUDE_ROOT)
    for base in search_dirs:
        candidate = (base / spec).resolve()
        if candidate.is_file() and INCLUDE_ROOT in candidate.parents:
            return rel_to_include_root(candidate)
    return None


@dataclass
class HeaderInfo:
    rel_path: str  # relative to src/includes, or "<repo>/..." for headers outside it
    in_include_root: bool
    reachable: bool = False
    direct_test_files: set[str] = field(default_factory=set)
    via: str | None = None  # one example reachability chain, for reporting


def find_all_headers() -> dict[str, HeaderInfo]:
    headers: dict[str, HeaderInfo] = {}
    for h in INCLUDE_ROOT.rglob("*.h"):
        rel = rel_to_include_root(h)
        headers[rel] = HeaderInfo(rel_path=rel, in_include_root=True)
    for h in SRC_ROOT.glob("*.h"):
        rel = str(h.relative_to(ROOT))
        headers[rel] = HeaderInfo(rel_path=rel, in_include_root=False)
    return headers


def build_header_graph(headers: dict[str, HeaderInfo]) -> dict[str, list[str]]:
    graph: dict[str, list[str]] = defaultdict(list)
    for rel, info in headers.items():
        if not info.in_include_root:
            continue
        header_path = INCLUDE_ROOT / rel
        for spec in quoted_includes(header_path):
            target = resolve_include(spec, from_dir=header_path.parent)
            if target:
                graph[rel].append(target)
    return graph


def find_test_files() -> list[Path]:
    return sorted(TEST_ROOT.rglob("*_test.cpp"))


def compute_reachability(headers: dict[str, HeaderInfo], graph: dict[str, list[str]], test_files: list[Path]):
    frontier: list[tuple[str, str, str]] = []  # (header_rel, test_file_rel, via)
    for tf in test_files:
        tf_rel = str(tf.relative_to(ROOT))
        for spec in quoted_includes(tf):
            target = resolve_include(spec)
            if target and target in headers:
                headers[target].direct_test_files.add(tf_rel)
                frontier.append((target, tf_rel, tf_rel))

    visited: set[str] = set()
    queue = list(frontier)
    while queue:
        rel, _tf_rel, via = queue.pop()
        if rel in visited:
            continue
        visited.add(rel)
        info = headers.get(rel)
        if info is None:
            continue
        info.reachable = True
        if info.via is None:
            info.via = via
        for dep in graph.get(rel, []):
            if dep not in visited:
                queue.append((dep, _tf_rel, f"{via} -> {rel}"))


def parse_exceptions() -> dict[str, str]:
    exceptions: dict[str, str] = {}
    if not EXCEPTIONS_FILE.exists():
        return exceptions
    for lineno, raw in enumerate(EXCEPTIONS_FILE.read_text().splitlines(), start=1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        if ":" not in line:
            print(f"{RED}Malformed exceptions entry (missing ':') at line {lineno}: {raw}{NC}")
            continue
        path, reason = line.split(":", 1)
        exceptions[path.strip()] = reason.strip()
    return exceptions


def analyze_test_file(path: Path) -> tuple[int, int]:
    text = path.read_text(errors="replace")
    tests = len(TEST_MACRO_RE.findall(text))
    expects = len(re.findall(r"EXPECT_", text))
    return tests, expects


def naming_mismatches(headers: dict[str, HeaderInfo], test_files: list[Path]) -> list[tuple[str, str]]:
    mismatches = []
    for tf in test_files:
        tf_rel = str(tf.relative_to(ROOT))
        project_includes = {resolve_include(s) for s in quoted_includes(tf)}
        project_includes.discard(None)
        if len(project_includes) != 1:
            continue
        (only_header,) = project_includes
        expected_stem = Path(only_header).stem
        actual_stem = tf.stem[: -len("_test")] if tf.stem.endswith("_test") else tf.stem
        if expected_stem != actual_stem:
            mismatches.append((tf_rel, f"includes only {only_header}, expected filename {expected_stem}_test.cpp"))
    return mismatches


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("-d", "--debug", action="store_true", help="print per-header reachability chains")
    parser.add_argument(
        "-c", "--coverage", action="store_true", help="also run dev-scripts/dev-coverage.sh for real line/branch coverage"
    )
    parser.add_argument(
        "--no-report", action="store_true", help=f"do not (re)write {GAPS_REPORT.relative_to(ROOT)}"
    )
    args = parser.parse_args()

    print(f"{BLUE}{'=' * 60}{NC}")
    print(f"{BLUE} Structural Test Coverage Checker (include-graph reachability){NC}")
    print(f"{BLUE}{'=' * 60}{NC}")

    headers = find_all_headers()
    graph = build_header_graph(headers)
    test_files = find_test_files()
    compute_reachability(headers, graph, test_files)
    exceptions = parse_exceptions()

    missing: list[HeaderInfo] = []
    exempt: list[tuple[HeaderInfo, str]] = []
    covered: list[HeaderInfo] = []
    stale_exceptions: list[str] = []
    invalid_exceptions: list[str] = []

    for path, reason in exceptions.items():
        if path not in headers:
            invalid_exceptions.append(path)
        elif headers[path].reachable:
            stale_exceptions.append(path)

    for rel in sorted(headers):
        info = headers[rel]
        if info.reachable:
            covered.append(info)
        elif rel in exceptions:
            exempt.append((info, exceptions[rel]))
        else:
            missing.append(info)

    for info in covered:
        chain = f" (via {info.via})" if args.debug else ""
        direct = ", ".join(sorted(info.direct_test_files)) or "transitively only"
        print(f"{GREEN}✓ COVERED:{NC} {info.rel_path} [{direct}]{chain}")

    for info, reason in exempt:
        print(f"{CYAN}○ EXEMPT:{NC} {info.rel_path} ({reason})")

    for info in missing:
        print(f"{RED}✗ MISSING:{NC} {info.rel_path}")

    empty_test_files = []
    total_tests = 0
    total_expects = 0
    for tf in test_files:
        tests, expects = analyze_test_file(tf)
        total_tests += tests
        total_expects += expects
        if tests == 0:
            empty_test_files.append(str(tf.relative_to(ROOT)))

    mismatches = naming_mismatches(headers, test_files)

    print(f"\n{BLUE}{'=' * 60}{NC}")
    print(f"{BLUE} SUMMARY{NC}")
    print(f"{BLUE}{'=' * 60}{NC}")
    total = len(headers)
    print(f"Total headers:        {BLUE}{total}{NC}")
    print(f"Covered:              {GREEN}{len(covered)}{NC}")
    print(f"Exempt (documented):  {CYAN}{len(exempt)}{NC}")
    print(f"Missing:              {RED}{len(missing)}{NC}")
    print(f"Test files found:     {BLUE}{len(test_files)}{NC}")
    print(f"Empty test files:     {ORANGE}{len(empty_test_files)}{NC}")
    print(f"Naming mismatches:    {ORANGE}{len(mismatches)}{NC}")
    print(f"Stale exceptions:     {RED}{len(stale_exceptions)}{NC}")
    print(f"Invalid exceptions:   {RED}{len(invalid_exceptions)}{NC}")
    print(f"Total tests found:    {CYAN}{total_tests}{NC}")
    print(f"Total expectations:   {CYAN}{total_expects}{NC}")

    if missing:
        print(f"\n{RED}Missing coverage for:{NC}")
        for info in missing:
            print(f"{RED} • {info.rel_path}{NC}")
        print(f"\n{YELLOW}If any of these are legitimately untestable (data tables, generated"
              f" files, dead scaffolding), add a line to {EXCEPTIONS_FILE.relative_to(ROOT)}:{NC}")
        print(f"{BLUE}  <path-relative-to-repo-or-src/includes> : <reason>{NC}")

    if empty_test_files:
        print(f"\n{ORANGE}Empty test files (no TEST/TEST_F/TEST_P/INSTANTIATE_TEST_SUITE_P found):{NC}")
        for tf in empty_test_files:
            print(f"{ORANGE} • {tf}{NC}")

    if mismatches:
        print(f"\n{ORANGE}Naming mismatches (single-header test file named after something else):{NC}")
        for tf_rel, msg in mismatches:
            print(f"{ORANGE} • {tf_rel}: {msg}{NC}")

    if stale_exceptions:
        print(f"\n{RED}Stale exceptions (now reachable from a test, remove from"
              f" {EXCEPTIONS_FILE.relative_to(ROOT)}):{NC}")
        for path in stale_exceptions:
            print(f"{RED} • {path}{NC}")

    if invalid_exceptions:
        print(f"\n{RED}Invalid exceptions (no such header, fix or remove from"
              f" {EXCEPTIONS_FILE.relative_to(ROOT)}):{NC}")
        for path in invalid_exceptions:
            print(f"{RED} • {path}{NC}")

    if not args.no_report:
        write_gaps_report(missing, exempt, empty_test_files, mismatches)
        print(f"\n{BLUE}Report written to {GAPS_REPORT.relative_to(ROOT)}{NC}")

    ok = not missing and not empty_test_files and not stale_exceptions and not invalid_exceptions

    if args.coverage:
        print(f"\n{BLUE}{'=' * 60}{NC}")
        print(f"{BLUE} Real Coverage (gcovr, via dev-coverage.sh){NC}")
        print(f"{BLUE}{'=' * 60}{NC}")
        coverage_script = ROOT / "dev-scripts" / "dev-coverage.sh"
        if coverage_script.is_file():
            result = subprocess.run([str(coverage_script)])
            ok = ok and result.returncode == 0
        else:
            print(f"{RED}dev-scripts/dev-coverage.sh not found{NC}")
            ok = False
    else:
        print(f"\n{BLUE}{'=' * 60}{NC}")
        print(f"{YELLOW}Note:{NC} this checks structural reachability (would a compile error or"
              f" obvious regression in this header be caught by some test), not how thoroughly")
        print(f"its logic is exercised. For real line/branch coverage, run:"
              f" {CYAN}./dev-scripts/dev-coverage.sh{NC} or {CYAN}./check_test_coverage.sh -c{NC}")

    return 0 if ok else 1


def write_gaps_report(missing, exempt, empty_test_files, mismatches) -> None:
    lines = [
        "# Test Coverage Gaps",
        "",
        "Auto-generated by `./check_test_coverage.sh`. Do not hand-edit; edit"
        " `test/coverage_exceptions.txt` instead and re-run the script.",
        "",
        "## Missing (no test reaches this header, directly or transitively)",
        "",
    ]
    if missing:
        for info in missing:
            lines.append(f"- `{info.rel_path}`")
    else:
        lines.append("_None._")
    lines += ["", "## Exempt (documented in `test/coverage_exceptions.txt`)", ""]
    if exempt:
        for info, reason in exempt:
            lines.append(f"- `{info.rel_path}` - {reason}")
    else:
        lines.append("_None._")
    lines += ["", "## Empty test files (file exists, no test cases written)", ""]
    if empty_test_files:
        for tf in empty_test_files:
            lines.append(f"- `{tf}`")
    else:
        lines.append("_None._")
    lines += ["", "## Naming mismatches", ""]
    if mismatches:
        for tf_rel, msg in mismatches:
            lines.append(f"- `{tf_rel}`: {msg}")
    else:
        lines.append("_None._")
    lines.append("")
    GAPS_REPORT.write_text("\n".join(lines))


if __name__ == "__main__":
    sys.exit(main())
