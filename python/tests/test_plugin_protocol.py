#!/usr/bin/env python3
"""Talk to plugin_runner.py over NDJSON without the C++ host."""

from __future__ import annotations

import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
RUNNER = ROOT / "python" / "plugin_runner.py"
PLUGIN = """
import arbuz

@arbuz.function("DOUBLE", syntax="DOUBLE(n)", help_ru="x2")
def double(n):
    if isinstance(n, list):
        n = n[0] if n else 0
    return float(n) * 2

@arbuz.command("probe.hi", "Привет", "Hello")
def hi():
    pass
"""


class Protocol(unittest.TestCase):
    def test_ready_and_call(self):
        with tempfile.TemporaryDirectory() as td:
            plug = Path(td) / "demo"
            plug.mkdir()
            (plug / "plugin.py").write_text(PLUGIN, encoding="utf-8")
            env = os.environ.copy()
            env["PYTHONUNBUFFERED"] = "1"
            env["PYTHONPATH"] = str(ROOT / "python")
            proc = subprocess.Popen(
                [sys.executable, str(RUNNER), str(td)],
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                env=env,
                text=True,
            )
            assert proc.stdin and proc.stdout
            ready = json.loads(proc.stdout.readline())
            self.assertEqual(ready.get("op"), "ready")
            self.assertIn("demo", ready.get("plugins") or [])
            names = [f["name"] for f in ready.get("functions") or []]
            self.assertIn("DOUBLE", names)
            proc.stdin.write(
                json.dumps({"op": "call_function", "id": 1, "name": "DOUBLE", "args": [21]}) + "\n"
            )
            proc.stdin.flush()
            result = json.loads(proc.stdout.readline())
            self.assertEqual(result.get("op"), "result")
            self.assertEqual(result.get("value"), 42)
            proc.stdin.close()
            try:
                proc.terminate()
                proc.wait(timeout=5)
            finally:
                if proc.stdout:
                    proc.stdout.close()
                if proc.stderr:
                    proc.stderr.close()


if __name__ == "__main__":
    unittest.main(verbosity=2)
