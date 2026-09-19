#!/usr/bin/env python3
"""Splice a generated node reference table into examples/pathfinder/README.md.

The README holds the table between two marker comments; everything between them is replaced.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
README = ROOT / "examples" / "pathfinder" / "README.md"
BEGIN = "<!-- NODE-REFERENCE:BEGIN -->"
END = "<!-- NODE-REFERENCE:END -->"


def splice(readme_text: str, table: str) -> str:
    begin = readme_text.find(BEGIN)
    end = readme_text.find(END)
    if begin < 0 or end < begin:
        raise ValueError(f"README needs {BEGIN} followed by {END}")
    body = table if table.endswith("\n") else table + "\n"
    return readme_text[: begin + len(BEGIN)] + "\n" + body + readme_text[end:]


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("table", type=Path, help="file holding the generated markdown table")
    parser.add_argument("--readme", type=Path, default=README)
    arguments = parser.parse_args(argv)
    try:
        updated = splice(arguments.readme.read_text(encoding="utf-8"), arguments.table.read_text(encoding="utf-8"))
    except ValueError as error:
        print(f"error: {error}", file=sys.stderr)
        return 2
    arguments.readme.write_text(updated, encoding="utf-8")
    print(f"{arguments.readme.name}: node reference updated")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
