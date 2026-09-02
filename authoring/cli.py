#!/usr/bin/env python3
"""abacdsp-authoring: talk to a running plugin instance's Authoring HTTP API.

Usage:
  python3 cli.py list
  python3 cli.py status <instance>
  python3 cli.py context <instance>
  python3 cli.py diagnostics <instance>
  python3 cli.py apply-script <instance> <script.lua>
  python3 cli.py list-patches <instance>
  python3 cli.py get-patch <instance> <name>
  python3 cli.py save-patch <instance> <name>
  python3 cli.py load-patch <instance> <name>
  python3 cli.py delete-patch <instance> <name>
  python3 cli.py export-patch <instance> <name> [--into DIR]
  python3 cli.py list-libraries <instance>
  python3 cli.py get-library <instance> <name>
  python3 cli.py apply-library <instance> <name> <library.lua>
  python3 cli.py midi <instance> <note-on|note-off|cc|program-change|pitch-bend|
                                   aftertouch|poly-pressure> [--channel N] [--note N]
                                   [--velocity N] [--controller N] [--value N] [--program N]

<instance> is a pid, a port, or a module name (only if it names exactly one
running instance) - see client.resolve_instance(). Run "list" first to see
what's currently running.
"""

import argparse
import json
import sys
from pathlib import Path
from typing import Any

import client


def cmd_list(_args: argparse.Namespace) -> int:
    instances = client.list_instances()
    if not instances:
        print("no running Authoring Mode instances found")
        return 0
    for info in instances:
        print(f"{info.get('module')}\tpid={info.get('pid')}\tport={info.get('port')}\t{info.get('baseUrl')}")
    return 0


def _print_json(payload: dict) -> None:
    print(json.dumps(payload, indent=2))


def cmd_status(args: argparse.Namespace) -> int:
    _print_json(client.get_status(client.resolve_instance(args.instance)))
    return 0


def cmd_context(args: argparse.Namespace) -> int:
    _print_json(client.get_context(client.resolve_instance(args.instance)))
    return 0


def cmd_diagnostics(args: argparse.Namespace) -> int:
    _print_json(client.get_diagnostics(client.resolve_instance(args.instance)))
    return 0


def cmd_apply_script(args: argparse.Namespace) -> int:
    instance = client.resolve_instance(args.instance)
    text = Path(args.script).read_text()
    result = client.apply_script(instance, text)
    if result.get("compiled"):
        print(f"applied '{args.script}' to {instance.get('module')} (pid={instance.get('pid')})")
        return 0
    print(f"compile error: {result.get('error')}", file=sys.stderr)
    return 1


def cmd_list_patches(args: argparse.Namespace) -> int:
    for name in client.list_patches(client.resolve_instance(args.instance)):
        print(name)
    return 0


def cmd_get_patch(args: argparse.Namespace) -> int:
    print(client.get_patch_json(client.resolve_instance(args.instance), args.name))
    return 0


def cmd_save_patch(args: argparse.Namespace) -> int:
    instance = client.resolve_instance(args.instance)
    if client.save_patch(instance, args.name):
        print(f"saved '{args.name}' on {instance.get('module')} (pid={instance.get('pid')})")
        return 0
    print(f"error: failed to save '{args.name}'", file=sys.stderr)
    return 1


def cmd_load_patch(args: argparse.Namespace) -> int:
    instance = client.resolve_instance(args.instance)
    if client.load_patch(instance, args.name):
        print(f"loaded '{args.name}' on {instance.get('module')} (pid={instance.get('pid')})")
        return 0
    print(f"error: no patch named '{args.name}'", file=sys.stderr)
    return 1


def cmd_delete_patch(args: argparse.Namespace) -> int:
    instance = client.resolve_instance(args.instance)
    if client.delete_patch(instance, args.name):
        print(f"deleted '{args.name}' on {instance.get('module')} (pid={instance.get('pid')})")
        return 0
    print(f"error: no patch named '{args.name}'", file=sys.stderr)
    return 1


# Repo root is this script's own grandparent directory (authoring/cli.py -> repo root).
_REPO_ROOT = Path(__file__).resolve().parent.parent


def cmd_export_patch(args: argparse.Namespace) -> int:
    instance = client.resolve_instance(args.instance)
    json_text = client.get_patch_json(instance, args.name)
    try:
        json.loads(json_text)
    except json.JSONDecodeError as e:
        print(f"error: server returned invalid JSON for patch '{args.name}': {e}", file=sys.stderr)
        return 1

    module = str(instance.get("module", "")).lower()
    target_dir = Path(args.into) if args.into else _REPO_ROOT / "examples" / module / "factory-patches"
    target_dir.mkdir(parents=True, exist_ok=True)
    target_file = target_dir / f"{args.name}.json"
    target_file.write_text(json_text)
    print(f"exported '{args.name}' -> {target_file}")
    print("commit this file, then it's synced back in as 'Factory/<name>' on every launch")
    return 0


def cmd_list_libraries(args: argparse.Namespace) -> int:
    for name in client.list_libraries(client.resolve_instance(args.instance)):
        print(name)
    return 0


def cmd_get_library(args: argparse.Namespace) -> int:
    print(client.get_library_source(client.resolve_instance(args.instance), args.name))
    return 0


def cmd_apply_library(args: argparse.Namespace) -> int:
    instance = client.resolve_instance(args.instance)
    content = Path(args.library).read_text()
    result = client.apply_library(instance, args.name, content)
    if result.get("compiled"):
        print(f"saved '{args.name}' and current script still compiles on {instance.get('module')}")
        return 0
    print(f"saved '{args.name}', but the current script now fails: {result.get('error')}", file=sys.stderr)
    return 1


# CLI spelling -> the server's own "type" field (see AuthoringHttpServer.h's parseMidiMessage).
_MIDI_TYPES = {
    "note-on": "noteOn", "note-off": "noteOff", "cc": "cc", "program-change": "programChange",
    "pitch-bend": "pitchBend", "aftertouch": "aftertouch", "poly-pressure": "polyPressure",
}


def cmd_midi(args: argparse.Namespace) -> int:
    instance = client.resolve_instance(args.instance)
    message: dict[str, Any] = {"type": _MIDI_TYPES[args.midi_type], "channel": args.channel}
    for field in ("note", "velocity", "controller", "value", "program"):
        value = getattr(args, field)
        if value is not None:
            message[field] = value
    client.inject_midi(instance, message)
    print(f"sent {args.midi_type} to {instance.get('module')} (pid={instance.get('pid')})")
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    subparsers = parser.add_subparsers(dest="command", required=True)

    subparsers.add_parser("list", help="list running Authoring Mode instances").set_defaults(func=cmd_list)

    for name, func, help_text in [
        ("status", cmd_status, "instance identity + diagnostics + current script/patch name"),
        ("context", cmd_context, "current script text, patch name, UI params, libraries"),
        ("diagnostics", cmd_diagnostics, "script error state, CPU load, Lua pool usage"),
    ]:
        sub = subparsers.add_parser(name, help=help_text)
        sub.add_argument("instance", help="pid, port, or module name of a running instance")
        sub.set_defaults(func=func)

    apply_script_parser = subparsers.add_parser("apply-script", help="submit and apply a Lua script")
    apply_script_parser.add_argument("instance", help="pid, port, or module name of a running instance")
    apply_script_parser.add_argument("script", help="path to a .lua file")
    apply_script_parser.set_defaults(func=cmd_apply_script)

    list_patches_parser = subparsers.add_parser("list-patches", help="list saved patch names")
    list_patches_parser.add_argument("instance", help="pid, port, or module name of a running instance")
    list_patches_parser.set_defaults(func=cmd_list_patches)

    for name, func, help_text in [
        ("get-patch", cmd_get_patch, "print a saved patch's raw JSON"),
        ("save-patch", cmd_save_patch, "save current live parameters + script under a name"),
        ("load-patch", cmd_load_patch, "load and apply a saved patch (discards unsaved changes, no prompt)"),
        ("delete-patch", cmd_delete_patch, "delete a saved patch"),
    ]:
        sub = subparsers.add_parser(name, help=help_text)
        sub.add_argument("instance", help="pid, port, or module name of a running instance")
        sub.add_argument("name", help="patch name")
        sub.set_defaults(func=func)

    export_patch_parser = subparsers.add_parser(
        "export-patch", help="write a saved patch's JSON into a repo factory-patches/ folder")
    export_patch_parser.add_argument("instance", help="pid, port, or module name of a running instance")
    export_patch_parser.add_argument("name", help="patch name")
    export_patch_parser.add_argument(
        "--into", help="target directory (default: examples/<module>/factory-patches/ under the repo root)")
    export_patch_parser.set_defaults(func=cmd_export_patch)

    list_libraries_parser = subparsers.add_parser("list-libraries", help="list installed library names")
    list_libraries_parser.add_argument("instance", help="pid, port, or module name of a running instance")
    list_libraries_parser.set_defaults(func=cmd_list_libraries)

    get_library_parser = subparsers.add_parser("get-library", help="print a library's Lua source")
    get_library_parser.add_argument("instance", help="pid, port, or module name of a running instance")
    get_library_parser.add_argument("name", help="library name")
    get_library_parser.set_defaults(func=cmd_get_library)

    apply_library_parser = subparsers.add_parser(
        "apply-library", help="save a Library/User/ script and re-apply the current script against it")
    apply_library_parser.add_argument("instance", help="pid, port, or module name of a running instance")
    apply_library_parser.add_argument("name", help="library name")
    apply_library_parser.add_argument("library", help="path to a .lua file")
    apply_library_parser.set_defaults(func=cmd_apply_library)

    midi_parser = subparsers.add_parser("midi", help="inject a synthetic MIDI event")
    midi_parser.add_argument("instance", help="pid, port, or module name of a running instance")
    midi_parser.add_argument("midi_type", choices=sorted(_MIDI_TYPES), help="MIDI message type")
    midi_parser.add_argument("--channel", type=int, default=0, help="0-based MIDI channel (default: 0)")
    midi_parser.add_argument("--note", type=int, help="note number (note-on/note-off/poly-pressure)")
    midi_parser.add_argument("--velocity", type=int, help="velocity 0-127 (note-on/note-off)")
    midi_parser.add_argument("--controller", type=int, help="controller number (cc)")
    midi_parser.add_argument("--value", type=int, help="value (cc/pitch-bend/aftertouch/poly-pressure)")
    midi_parser.add_argument("--program", type=int, help="program number (program-change)")
    midi_parser.set_defaults(func=cmd_midi)

    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        return args.func(args)
    except client.AuthoringError as e:
        print(f"error: {e}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
