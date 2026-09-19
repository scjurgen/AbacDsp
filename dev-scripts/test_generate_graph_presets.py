#!/usr/bin/env python3
"""Unit tests for generate_graph_presets.py.

Run: python3 -m unittest discover -s dev-scripts -p "test_generate_graph_presets.py"
"""

from __future__ import annotations

import contextlib
import io
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import generate_graph_presets as generator  # noqa: E402


class GeneratorTestCase(unittest.TestCase):
    def setUp(self) -> None:
        self._temp = tempfile.TemporaryDirectory()
        self.addCleanup(self._temp.cleanup)
        self.lua_dir = Path(self._temp.name) / "lua"
        self.lua_dir.mkdir()
        self.output = Path(self._temp.name) / "Presets.h"

    def write_lua(self, name: str, text: str | bytes) -> None:
        path = self.lua_dir / name
        path.write_bytes(text if isinstance(text, bytes) else text.encode("ascii"))

    def run_main(self, *extra: str) -> tuple[int, str, str]:
        out, err = io.StringIO(), io.StringIO()
        arguments = ["--lua-dir", str(self.lua_dir), "--output", str(self.output), *extra]
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            code = generator.main(arguments)
        return code, out.getvalue(), err.getvalue()


class NamingTests(GeneratorTestCase):
    def test_prefix_is_stripped_and_constant_is_camel_case(self) -> None:
        self.write_lua("01_tape_vibrato.lua", "return {}\n")
        self.write_lua("classic.lua", "return {}\n")
        presets = generator.load_presets(self.lua_dir)
        self.assertEqual([(p.name, p.constant) for p in presets],
                         [("tape_vibrato", "kTapeVibrato"), ("classic", "kClassic")])

    def test_files_are_ordered_by_file_name(self) -> None:
        self.write_lua("02_b.lua", "return 2\n")
        self.write_lua("01_a.lua", "return 1\n")
        self.assertEqual([p.name for p in generator.load_presets(self.lua_dir)], ["a", "b"])

    def test_invalid_name_is_rejected(self) -> None:
        self.write_lua("Bad-Name.lua", "return {}\n")
        with self.assertRaises(generator.PresetError):
            generator.load_presets(self.lua_dir)

    def test_two_files_with_the_same_name_are_rejected(self) -> None:
        self.write_lua("01_same.lua", "return 1\n")
        self.write_lua("02_same.lua", "return 2\n")
        with self.assertRaisesRegex(generator.PresetError, "same preset name"):
            generator.load_presets(self.lua_dir)


class ContentTests(GeneratorTestCase):
    def test_non_ascii_is_rejected(self) -> None:
        self.write_lua("a.lua", f"return '{chr(0xE4)}'\n".encode("utf-8"))
        with self.assertRaisesRegex(generator.PresetError, "non-ASCII"):
            generator.load_presets(self.lua_dir)

    def test_empty_file_is_rejected(self) -> None:
        self.write_lua("a.lua", "  \n")
        with self.assertRaisesRegex(generator.PresetError, "empty"):
            generator.load_presets(self.lua_dir)

    def test_oversize_body_is_rejected(self) -> None:
        self.write_lua("a.lua", "-- " + "x" * generator.MAX_BODY_BYTES + "\n")
        with self.assertRaisesRegex(generator.PresetError, "limit"):
            generator.load_presets(self.lua_dir)

    def test_line_endings_are_normalised_and_a_final_newline_is_added(self) -> None:
        self.write_lua("a.lua", b"return {\r\n}")
        self.assertEqual(generator.load_presets(self.lua_dir)[0].lua, "return {\n}\n")

    def test_lua_line_numbers_match_the_constant(self) -> None:
        self.write_lua("a.lua", "-- one\nreturn {}\n")
        constant = generator.render_constant(generator.load_presets(self.lua_dir)[0])
        lines = constant.splitlines()
        self.assertTrue(lines[0].endswith('R"lua(-- one'))
        self.assertEqual(lines[1:], ["return {}", ')lua";'])


class RenderingTests(GeneratorTestCase):
    def test_delimiter_grows_when_the_body_contains_the_terminator(self) -> None:
        self.assertEqual(generator.raw_delimiter("plain"), "lua")
        self.assertEqual(generator.raw_delimiter('x = ")lua"'), "luax")
        self.assertEqual(generator.raw_delimiter('")lua" and ")luax"'), "luaxx")

    def test_header_lists_every_preset_in_the_array(self) -> None:
        self.write_lua("01_a.lua", "return 1\n")
        self.write_lua("02_b.lua", "return 2\n")
        header = generator.render_header(generator.load_presets(self.lua_dir))
        self.assertIn("std::array<Preset, 2> kChorusPresets", header)
        self.assertIn('{"a", kA},', header)
        self.assertIn('{"b", kB},', header)
        self.assertIn("Do not edit", header.splitlines()[1])
        self.assertLess(header.index("// clang-format off"), header.index("kA ="))
        self.assertGreater(header.index("// clang-format on"), header.index("kChorusPresets"))

    def test_output_is_deterministic(self) -> None:
        self.write_lua("01_a.lua", "return 1\n")
        presets = generator.load_presets(self.lua_dir)
        self.assertEqual(generator.render_header(presets), generator.render_header(presets))


class CommandLineTests(GeneratorTestCase):
    def test_generate_writes_the_header_and_check_then_passes(self) -> None:
        self.write_lua("01_a.lua", "return 1\n")
        self.assertEqual(self.run_main()[0], 0)
        self.assertTrue(self.output.exists())
        self.assertEqual(self.run_main("--check")[0], 0)

    def test_check_fails_for_a_missing_or_stale_header(self) -> None:
        self.write_lua("01_a.lua", "return 1\n")
        self.assertEqual(self.run_main("--check")[0], 1)
        self.run_main()
        self.write_lua("01_a.lua", "return 2\n")
        code, _, err = self.run_main("--check")
        self.assertEqual(code, 1)
        self.assertIn("out of date", err)

    def test_check_never_writes(self) -> None:
        self.write_lua("01_a.lua", "return 1\n")
        self.run_main("--check")
        self.assertFalse(self.output.exists())

    def test_errors_exit_2_without_writing(self) -> None:
        self.write_lua("a.lua", "  \n")
        code, _, err = self.run_main()
        self.assertEqual(code, 2)
        self.assertIn("error:", err)
        self.assertFalse(self.output.exists())

    def test_an_empty_directory_is_an_error(self) -> None:
        self.assertEqual(self.run_main()[0], 2)


if __name__ == "__main__":
    unittest.main()
