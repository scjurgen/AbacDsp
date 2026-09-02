"""Run directly: python3 -m unittest discover authoring
No CTest/CMake wiring - this repo has no existing Python test convention to
follow (JuceStandaloneGenerator/unittests/ is C++-only), and pulling in pytest
would add a dependency this stdlib-only tool otherwise has none of.
"""

import http.server
import json
import os
import subprocess
import sys
import threading
import unittest
from pathlib import Path

import client


class ResolveInstanceTests(unittest.TestCase):
    def setUp(self) -> None:
        self.alive_pid = os.getpid()  # this test process is always alive
        self.instances = [
            {"module": "dronesequencer", "pid": self.alive_pid, "port": 111, "token": "t1",
             "baseUrl": "http://127.0.0.1:111"},
            {"module": "resonik", "pid": self.alive_pid, "port": 222, "token": "t2",
             "baseUrl": "http://127.0.0.1:222"},
            {"module": "resonik", "pid": self.alive_pid, "port": 333, "token": "t3",
             "baseUrl": "http://127.0.0.1:333"},
        ]

    def test_resolves_by_pid_or_port(self) -> None:
        client.list_instances = lambda: self.instances
        self.assertEqual(client.resolve_instance(str(self.alive_pid))["port"], 111)
        self.assertEqual(client.resolve_instance("222")["module"], "resonik")

    def test_resolves_unambiguous_module_name(self) -> None:
        client.list_instances = lambda: self.instances
        self.assertEqual(client.resolve_instance("dronesequencer")["port"], 111)

    def test_ambiguous_module_name_raises(self) -> None:
        client.list_instances = lambda: self.instances
        with self.assertRaises(client.AuthoringError):
            client.resolve_instance("resonik")

    def test_unknown_selector_raises(self) -> None:
        client.list_instances = lambda: self.instances
        with self.assertRaises(client.AuthoringError):
            client.resolve_instance("nonexistent")

    def test_no_instances_raises(self) -> None:
        client.list_instances = lambda: []
        with self.assertRaises(client.AuthoringError):
            client.resolve_instance("anything")


class PidIsAliveTests(unittest.TestCase):
    def test_current_process_is_alive(self) -> None:
        self.assertTrue(client._pid_is_alive(os.getpid()))

    def test_a_definitely_dead_pid_is_not_alive(self) -> None:
        proc = subprocess.Popen([sys.executable, "-c", "pass"])
        proc.wait()
        self.assertFalse(client._pid_is_alive(proc.pid))


class ListInstancesStalePidTests(unittest.TestCase):
    def test_stale_discovery_file_is_skipped(self) -> None:
        import tempfile
        with tempfile.TemporaryDirectory() as tmp:
            directory = Path(tmp)
            proc = subprocess.Popen([sys.executable, "-c", "pass"])
            proc.wait()
            (directory / f"{proc.pid}-111.json").write_text(
                json.dumps({"module": "stale", "pid": proc.pid, "port": 111, "token": "x",
                            "baseUrl": "http://127.0.0.1:111"}))
            (directory / f"{os.getpid()}-222.json").write_text(
                json.dumps({"module": "live", "pid": os.getpid(), "port": 222, "token": "y",
                            "baseUrl": "http://127.0.0.1:222"}))
            client.discovery_dir = lambda: directory
            modules = [i["module"] for i in client.list_instances()]
            self.assertEqual(modules, ["live"])


class _FakeHandler(http.server.BaseHTTPRequestHandler):
    def log_message(self, *_args) -> None:
        pass

    def do_GET(self) -> None:
        self.server.last_path = self.path
        self.server.last_auth_header = self.headers.get("Authorization")
        if self.path == "/patches/Factory%2Fbeta":
            self._respond_raw('{"level": 0.5}')
            return
        if self.path == "/libraries/missing":
            self.send_response(404)
            self.end_headers()
            return
        self._respond({"module": "fake", "port": self.server.server_port})

    def do_POST(self) -> None:
        self.server.last_path = self.path
        length = int(self.headers.get("Content-Length", 0))
        self.server.last_body = json.loads(self.rfile.read(length)) if length else None
        if self.path.startswith("/midi"):
            self._respond({"injected": True})
            return
        self._respond({"compiled": (self.server.last_body or {}).get("text") == "good", "error": ""})

    def _respond(self, payload: dict) -> None:
        self._respond_raw(json.dumps(payload))

    def _respond_raw(self, text: str) -> None:
        data = text.encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.end_headers()
        self.wfile.write(data)


class HttpRequestShapeTests(unittest.TestCase):
    def setUp(self) -> None:
        self.server = http.server.HTTPServer(("127.0.0.1", 0), _FakeHandler)
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()
        self.instance = {"baseUrl": f"http://127.0.0.1:{self.server.server_port}", "token": "secret-token"}

    def tearDown(self) -> None:
        self.server.shutdown()
        self.thread.join()
        self.server.server_close()

    def test_get_sends_bearer_token(self) -> None:
        client.get_status(self.instance)
        self.assertEqual(self.server.last_auth_header, "Bearer secret-token")

    def test_apply_script_posts_json_body(self) -> None:
        result = client.apply_script(self.instance, "good")
        self.assertTrue(result["compiled"])
        result = client.apply_script(self.instance, "bad")
        self.assertFalse(result["compiled"])

    def test_get_patch_json_is_raw_text_and_url_quotes_the_name(self) -> None:
        # "Factory/beta" round-trips through the URL as one quoted path segment - the
        # fake handler only answers /patches/Factory%2Fbeta, proving that quoting happened.
        text = client.get_patch_json(self.instance, "Factory/beta")
        self.assertEqual(text, '{"level": 0.5}')

    def test_get_library_source_raises_a_clean_error_on_404(self) -> None:
        with self.assertRaises(client.AuthoringError):
            client.get_library_source(self.instance, "missing")

    def test_apply_library_posts_content_field(self) -> None:
        client.apply_library(self.instance, "helpers", "function Helper() end")
        self.assertEqual(self.server.last_path, "/libraries/helpers")
        self.assertEqual(self.server.last_body, {"content": "function Helper() end"})

    def test_inject_midi_posts_message_as_is(self) -> None:
        message = {"type": "noteOn", "channel": 0, "note": 60, "velocity": 100}
        client.inject_midi(self.instance, message)
        self.assertEqual(self.server.last_path, "/midi")
        self.assertEqual(self.server.last_body, message)


if __name__ == "__main__":
    unittest.main()
