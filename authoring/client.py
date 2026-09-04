"""Thin HTTP client for the Authoring HTTP API - see CLAUDE.md in this folder.

Stdlib-only by design, matching JuceStandaloneGenerator's own Python tooling: no
pip install, just `python3 authoring/cli.py ...`.
"""

import json
import os
import platform
import time
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path
from typing import Any


class AuthoringError(Exception):
    """Raised for anything the CLI should report as a clean error, not a traceback."""


def discovery_dir() -> Path:
    """The shared, per-machine directory every running Authoring Mode instance
    writes a discovery file into - mirrors AuthoringHttpServer.h's
    discoveryDirectory(), which itself mirrors JUCE's userApplicationDataDirectory
    per platform (see juce_Files_windows.cpp / juce_Files_linux.cpp)."""
    system = platform.system()
    if system == "Darwin":
        base = Path.home() / "Library" / "Application Support"
    elif system == "Windows":
        base = Path(os.environ.get("APPDATA", Path.home() / "AppData" / "Roaming"))
    else:
        base = Path(os.environ.get("XDG_CONFIG_HOME", Path.home() / ".config"))
    return base / "AbacDsp" / "AuthoringInstances"


def _pid_is_alive(pid: int) -> bool:
    try:
        os.kill(pid, 0)
        return True
    except ProcessLookupError:
        return False
    except PermissionError:
        return True  # exists, just not ours to signal
    except OSError:
        return False


def list_instances() -> list[dict[str, Any]]:
    """Every currently-running Authoring Mode instance, newest first. A discovery
    file whose pid is no longer running is treated as stale and skipped - the
    plugin only deletes it on a clean shutdown, never on a crash."""
    directory = discovery_dir()
    if not directory.is_dir():
        return []
    instances = []
    for entry in directory.glob("*.json"):
        try:
            info = json.loads(entry.read_text())
        except (OSError, json.JSONDecodeError):
            continue
        pid = info.get("pid")
        if not isinstance(pid, int) or not _pid_is_alive(pid):
            continue
        instances.append(info)
    instances.sort(key=lambda i: i.get("startedAtEpochMs", 0), reverse=True)
    return instances


def resolve_instance(selector: str) -> dict[str, Any]:
    """Accepts a pid, a port, or (if it names exactly one running instance) a
    module name. Raises AuthoringError with the live candidate list on any
    ambiguity or miss, rather than guessing."""
    instances = list_instances()
    if not instances:
        raise AuthoringError("no running Authoring Mode instances found - is one enabled?")

    if selector.isdigit():
        value = int(selector)
        for info in instances:
            if info.get("pid") == value or info.get("port") == value:
                return info
        raise AuthoringError(f"no running instance with pid or port {value}")

    matches = [info for info in instances if info.get("module") == selector]
    if len(matches) == 1:
        return matches[0]
    if len(matches) > 1:
        ports = ", ".join(str(m.get("port")) for m in matches)
        raise AuthoringError(f"multiple running '{selector}' instances (ports: {ports}) - use a port or pid instead")
    names = ", ".join(f"{i.get('module')}:{i.get('port')}" for i in instances)
    raise AuthoringError(f"no running instance named '{selector}' - currently running: {names}")


def _request(instance: dict[str, Any], method: str, path: str, body: dict[str, Any] | None = None,
             timeout: float = 5.0, raw: bool = False) -> Any:
    """raw=True returns the response body's text as-is instead of JSON-decoding it - used
    for GET /patches/{name}, whose body should round-trip byte-for-byte into a
    factory-patches/*.json file, not get re-serialized through a Python dict first."""
    url = instance["baseUrl"].rstrip("/") + path
    data = json.dumps(body).encode("utf-8") if body is not None else None
    req = urllib.request.Request(url, data=data, method=method)
    req.add_header("Authorization", "Bearer " + instance["token"])
    if data is not None:
        req.add_header("Content-Type", "application/json")
    try:
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            text = resp.read().decode("utf-8")
            return text if raw else json.loads(text)
    except urllib.error.HTTPError as e:
        with e:
            detail = e.read().decode("utf-8", errors="replace")
        raise AuthoringError(f"{method} {path} -> HTTP {e.code}: {detail}") from e
    except urllib.error.URLError as e:
        raise AuthoringError(f"{method} {path} -> could not reach {url}: {e.reason}") from e


def get_status(instance: dict[str, Any]) -> dict[str, Any]:
    return _request(instance, "GET", "/status")


def get_instance_info(instance: dict[str, Any]) -> dict[str, Any]:
    return _request(instance, "GET", "/instance")


def get_diagnostics(instance: dict[str, Any]) -> dict[str, Any]:
    return _request(instance, "GET", "/diagnostics")


def get_context(instance: dict[str, Any]) -> dict[str, Any]:
    return _request(instance, "GET", "/context")


def apply_script(instance: dict[str, Any], text: str) -> dict[str, Any]:
    """Submits and applies a script in one call - the response's "compiled"/"error"
    fields are the same synchronous result the old folder watchdog's
    state-<name>.json used to report, just returned directly instead of polled for."""
    return _request(instance, "POST", "/script", body={"text": text})


def _patch_path(name: str) -> str:
    return "/patches/" + urllib.parse.quote(name, safe="")


def list_patches(instance: dict[str, Any]) -> list[str]:
    return _request(instance, "GET", "/patches").get("patches", [])


def get_patch_json(instance: dict[str, Any], name: str) -> str:
    """Raw saved-patch JSON text (every knob value plus the embedded script), exactly as
    stored - the shape to write straight into a factory-patches/*.json file."""
    try:
        return _request(instance, "GET", _patch_path(name), raw=True)
    except AuthoringError as e:
        raise AuthoringError(f"no patch named '{name}' ({e})") from e


def save_patch(instance: dict[str, Any], name: str) -> bool:
    """Saves the instance's current live parameters + script under this name."""
    return _request(instance, "POST", _patch_path(name)).get("saved", False)


def load_patch(instance: dict[str, Any], name: str) -> bool:
    """Loads and applies a saved named patch - discards unsaved live changes, no prompt."""
    return _request(instance, "POST", _patch_path(name) + "/load").get("loaded", False)


def delete_patch(instance: dict[str, Any], name: str) -> bool:
    return _request(instance, "DELETE", _patch_path(name)).get("deleted", False)


def _library_path(name: str) -> str:
    return "/libraries/" + urllib.parse.quote(name, safe="")


def list_libraries(instance: dict[str, Any]) -> list[str]:
    return _request(instance, "GET", "/libraries").get("libraries", [])


def get_library_source(instance: dict[str, Any], name: str) -> str:
    try:
        return _request(instance, "GET", _library_path(name)).get("content", "")
    except AuthoringError as e:
        raise AuthoringError(f"no library named '{name}' ({e})") from e


def apply_library(instance: dict[str, Any], name: str, content: str) -> dict[str, Any]:
    """Saves a Library/User/ script and re-applies the current script against it - the
    response's "compiled"/"error" describe that re-apply, not the library file alone."""
    return _request(instance, "POST", _library_path(name), body={"content": content})


def inject_midi(instance: dict[str, Any], message: dict[str, Any]) -> None:
    """message e.g. {"type": "noteOn", "channel": 0, "note": 60, "velocity": 100} - channel
    is 0-based, matching LUA.md's OnNoteOn/etc. See CLAUDE.md for every message type."""
    _request(instance, "POST", "/midi", body=message)


def record_start(instance: dict[str, Any]) -> dict[str, Any]:
    """Starts capturing this instance's own audio output to a WAV file; the response's
    "path" is where it's being written - see CLAUDE.md for the temp-folder/cleanup contract."""
    return _request(instance, "POST", "/record/start")


def record_stop(instance: dict[str, Any]) -> dict[str, Any]:
    """Stops the current recording, if any, and finalizes its WAV file."""
    return _request(instance, "POST", "/record/stop")


def current_epoch_ms() -> int:
    return int(time.time() * 1000)
