#!/usr/bin/env python3
"""Generate examples/<name>/scripting.html for each Lua-scripted example.

Combines LUA-MANUAL.md (the API shared by every Lua-scripted example) with one
example's own README.md "## Scripting" section into a single self-contained
HTML page, rendered client-side with a vendored copy of marked.js (no network
access needed to view it, no server-side markdown parsing here). Both markdown
sources are embedded base64-encoded in <script type="text/plain"> tags so no
character in either source needs escaping.

The set of Lua-scripted examples is derived from JuceStandaloneGenerator's
blueprints ("use-lua": true), not hardcoded, so a new one is picked up
automatically.
"""

from __future__ import annotations

import argparse
import base64
import html
import json
import re
import sys
import webbrowser
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BLUEPRINTS_DIR = ROOT / "JuceStandaloneGenerator" / "blueprints"
LUA_MANUAL = ROOT / "LUA-MANUAL.md"
MARKED_JS = Path(__file__).resolve().parent / "vendor" / "marked.min.js"

SCRIPTING_HEADING = re.compile(r"^## Scripting\s*$")
NEXT_H2_HEADING = re.compile(r"^## ")
TITLE_HEADING = re.compile(r"^#\s+(.+?)\s*$")

PAGE_TEMPLATE = """<!doctype html>
<html>
<head>
<meta charset="utf-8">
<title>{title} scripting</title>
<style>
:root {{ color-scheme: light dark; }}
body {{
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Helvetica, Arial, sans-serif;
    max-width: 860px; margin: 0 auto; padding: 24px 20px 80px;
    background: #ffffff; color: #1a1a1a; line-height: 1.55;
}}
@media (prefers-color-scheme: dark) {{
    body {{ background: #1e1e1e; color: #d4d4d4; }}
    a {{ color: #6ab0f3; }}
    code, pre {{ background: #2a2a2a; }}
    hr {{ border-color: #444; }}
    th, td {{ border-color: #555; }}
}}
h1, h2, h3 {{ line-height: 1.25; }}
h1 {{ border-bottom: 2px solid currentColor; padding-bottom: 6px; }}
h2 {{ margin-top: 2.2em; border-bottom: 1px solid #8888; padding-bottom: 4px; }}
code {{ font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, monospace; }}
pre {{ padding: 12px 14px; border-radius: 6px; background: #f2f2f2; overflow-x: auto; }}
@media (prefers-color-scheme: dark) {{ pre {{ background: #2a2a2a; }} }}
table {{ border-collapse: collapse; margin: 1em 0; }}
th, td {{ border: 1px solid #ccc; padding: 4px 10px; text-align: left; }}
.doc-nav {{ font-size: 0.9em; margin-bottom: 1.5em; }}
.doc-section {{ margin-bottom: 3em; }}
</style>
</head>
<body>
<div class="doc-nav"><a href="#example-section">{title} scripting</a> | <a href="#manual-section">Lua manual</a></div>
<div id="example-section" class="doc-section"></div>
<hr>
<div id="manual-section" class="doc-section"></div>

<script type="text/plain" id="example-md-b64">{example_b64}</script>
<script type="text/plain" id="manual-md-b64">{manual_b64}</script>
<script>
{marked_js}
</script>
<script>
function b64ToUtf8(b64) {{
    const bin = atob(b64);
    const bytes = Uint8Array.from(bin, c => c.charCodeAt(0));
    return new TextDecoder("utf-8").decode(bytes);
}}
const exampleMd = b64ToUtf8(document.getElementById("example-md-b64").textContent);
const manualMd = b64ToUtf8(document.getElementById("manual-md-b64").textContent);
document.getElementById("example-section").innerHTML = marked.parse(exampleMd);
document.getElementById("manual-section").innerHTML = marked.parse(manualMd);
</script>
</body>
</html>
"""


def lua_scripted_example_names() -> list[str]:
    names = []
    for blueprint_path in sorted(BLUEPRINTS_DIR.glob("*.json")):
        blueprint = json.loads(blueprint_path.read_text(encoding="utf-8"))
        if blueprint.get("use-lua"):
            names.append(blueprint["name"])
    return names


def extract_scripting_section(readme_text: str) -> str:
    lines = readme_text.splitlines()
    start = next((i for i, line in enumerate(lines) if SCRIPTING_HEADING.match(line)), None)
    if start is None:
        raise ValueError('no "## Scripting" heading found')
    end = next(
        (i for i in range(start + 1, len(lines)) if NEXT_H2_HEADING.match(lines[i])),
        len(lines),
    )
    return "\n".join(lines[start:end]).strip() + "\n"


def example_title(readme_text: str, fallback_name: str) -> str:
    match = TITLE_HEADING.match(readme_text.splitlines()[0]) if readme_text else None
    return match.group(1) if match else fallback_name.capitalize()


def to_b64(text: str) -> str:
    return base64.b64encode(text.encode("utf-8")).decode("ascii")


def generate_one(name: str, manual_md: str, marked_js: str) -> Path:
    readme_path = ROOT / "examples" / name / "README.md"
    readme_text = readme_path.read_text(encoding="utf-8")
    section = extract_scripting_section(readme_text)
    title = html.escape(example_title(readme_text, name))

    page = PAGE_TEMPLATE.format(
        title=title,
        example_b64=to_b64(section),
        manual_b64=to_b64(manual_md),
        marked_js=marked_js,
    )
    out_path = ROOT / "examples" / name / "scripting.html"
    out_path.write_text(page, encoding="utf-8")
    return out_path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("example", nargs="?", help="only regenerate this example (default: all)")
    parser.add_argument("--open", action="store_true", help="open the (first) generated file in the browser")
    args = parser.parse_args()

    all_names = lua_scripted_example_names()
    if args.example:
        if args.example not in all_names:
            print(f"error: {args.example!r} is not a Lua-scripted example ({', '.join(all_names)})", file=sys.stderr)
            return 1
        names = [args.example]
    else:
        names = all_names

    manual_md = LUA_MANUAL.read_text(encoding="utf-8")
    marked_js = MARKED_JS.read_text(encoding="utf-8")

    written = []
    for name in names:
        try:
            written.append(generate_one(name, manual_md, marked_js))
        except ValueError as exc:
            print(f"error: {name}: {exc}", file=sys.stderr)
            return 1

    for path in written:
        print(path.relative_to(ROOT))

    if args.open and written:
        webbrowser.open(written[0].as_uri())

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
