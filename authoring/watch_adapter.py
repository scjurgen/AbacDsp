"""Optional watched-folder adapter, for anyone who prefers dropping a file over
calling the CLI directly. Reimplements the old in-plugin folder watchdog's
protocol as a pure HTTP client - the plugin itself never touches this folder,
this script polls it and calls POST /script on your behalf. See CLAUDE.md.

Usage: python3 watch_adapter.py <instance> <folder> [--interval SECONDS]
"""

import argparse
import sys
import time
from pathlib import Path

import client


def _select_candidate(folder: Path) -> Path | None:
    candidates = [f for f in folder.glob("*.lua") if not f.name.startswith("pulled-")]
    if not candidates:
        return None
    return max(candidates, key=lambda f: (f.stat().st_mtime, f.name))


def _pull(source: Path) -> None:
    pulled = source.with_name(f"pulled-{source.stem}-{client.current_epoch_ms()}.lua")
    source.rename(pulled)


def _write_state(folder: Path, name: str, compiled: bool, error: str) -> None:
    state_file = folder / f"state-{name}.json"
    state_file.write_text(f'{{"compiled": {str(compiled).lower()}, "error": "{error}"}}\n')


def poll_once(instance: dict, folder: Path) -> bool:
    """Returns True if a candidate was found and processed (whether or not it
    compiled), False if the folder had nothing new to apply."""
    candidate = _select_candidate(folder)
    if candidate is None:
        return False
    name = candidate.stem
    text = candidate.read_text()
    result = client.apply_script(instance, text)
    _write_state(folder, name, result.get("compiled", False), result.get("error", ""))
    _pull(candidate)
    status = "compiled" if result.get("compiled") else f"error: {result.get('error')}"
    print(f"[{name}] {status}")
    return True


def watch(instance: dict, folder: Path, interval_seconds: float) -> None:
    folder.mkdir(parents=True, exist_ok=True)
    print(f"watching {folder} for *.lua files, applying to {instance.get('module')} "
          f"(pid={instance.get('pid')}) every {interval_seconds}s - Ctrl+C to stop")
    while True:
        poll_once(instance, folder)
        time.sleep(interval_seconds)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("instance", help="pid, port, or module name of a running instance")
    parser.add_argument("folder", help="folder to watch for dropped .lua scripts")
    parser.add_argument("--interval", type=float, default=2.0, help="poll interval in seconds (default: 2.0)")
    args = parser.parse_args(argv)

    try:
        instance = client.resolve_instance(args.instance)
        watch(instance, Path(args.folder), args.interval)
    except client.AuthoringError as e:
        print(f"error: {e}", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        return 0
    return 0


if __name__ == "__main__":
    sys.exit(main())
