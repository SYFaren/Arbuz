#!/usr/bin/env python3
"""Load python-plugins/* and speak NDJSON with the Arbuz process."""

from __future__ import annotations

import importlib.util
import json
import os
import sys
import traceback
from pathlib import Path

HOST = Path(__file__).resolve().parent
if str(HOST) not in sys.path:
    sys.path.insert(0, str(HOST))

import arbuz  # noqa: E402

SKIP_NAMES = {"README.txt", "README.md", "__pycache__", "arbuz"}


def load_plugin(path: Path) -> str | None:
    entry = path / "plugin.py" if path.is_dir() else path
    if entry.suffix != ".py" or not entry.is_file():
        return None
    if entry.name.startswith("_"):
        return None
    name = path.name if path.is_dir() else entry.stem
    arbuz._plugin_id = name
    spec = importlib.util.spec_from_file_location(f"arbuz_plugin_{name}", entry)
    if spec is None or spec.loader is None:
        return None
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return name


def emit_ready(loaded: list[str]) -> None:
    reg = arbuz.registrations()
    arbuz._send(
        {
            "op": "ready",
            "plugins": loaded,
            "functions": reg["functions"],
            "commands": reg["commands"],
        }
    )


def handle(msg: dict) -> None:
    op = msg.get("op")
    if op == "call_function":
        rid = msg.get("id")
        try:
            value = arbuz._call_function(msg.get("name", ""), msg.get("args") or [])
            if isinstance(value, dict) and "error" in value and len(value) == 1:
                out = {"op": "result", "id": rid, "error": value["error"]}
            else:
                out = {"op": "result", "id": rid, "value": value}
        except Exception as exc:
            out = {"op": "result", "id": rid, "error": str(exc) or type(exc).__name__}
        arbuz._send(out)
        return
    if op == "run_command":
        rid = msg.get("id")
        fn = arbuz._commands.get(msg.get("command", ""))
        try:
            if fn:
                fn()
            arbuz._send({"op": "result", "id": rid, "value": True})
        except Exception as exc:
            arbuz._send({"op": "result", "id": rid, "error": str(exc)})
        return
    if op == "event":
        name = msg.get("name", "")
        for fn in arbuz._events.get(name, []):
            try:
                fn(msg)
            except Exception:
                traceback.print_exc()
        return
    if op == "reply":
        return


def main() -> int:
    plug_dir = Path(sys.argv[1] if len(sys.argv) > 1 else os.environ.get("ARBUZ_PLUGINS_DIR", "."))
    loaded: list[str] = []
    if plug_dir.is_dir():
        for item in sorted(plug_dir.iterdir()):
            if item.name.startswith(".") or item.name in SKIP_NAMES:
                continue
            try:
                name = load_plugin(item)
                if name:
                    loaded.append(name)
            except Exception:
                traceback.print_exc()
    emit_ready(loaded)
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            handle(json.loads(line))
        except Exception:
            traceback.print_exc()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
