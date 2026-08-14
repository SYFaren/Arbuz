"""Arbuz Python plugin API.

Talks to the desktop app over stdin/stdout (NDJSON). Plugin authors only
use this module — never speak the wire protocol directly.

Minimal plugin (python-plugins/my-plugin/plugin.py)::

    import arbuz

    @arbuz.function("VAT", syntax="VAT(amount)", help_ru="НДС 20%")
    def vat(amount):
        return float(amount) * 0.20

    @arbuz.command("hello", "Привет в A1", "Hello into A1")
    def hello():
        arbuz.set("A1", "hello")

    @arbuz.on("cell_changed")
    def on_cell(msg):
        print(msg["a1"])
"""

from __future__ import annotations

import json
import sys
from typing import Any, Callable, Iterable, Sequence

_out = sys.stdout
_functions: dict[str, Callable] = {}
_commands: dict[str, Callable] = {}
_events: dict[str, list[Callable]] = {}
_fn_meta: list[dict] = []
_cmd_meta: list[dict] = []
_req_id = 0
_plugin_id = "plugin"


def _send(obj: dict) -> None:
    _out.write(json.dumps(obj, ensure_ascii=False) + "\n")
    _out.flush()


def _next_id() -> int:
    global _req_id
    _req_id += 1
    return _req_id


def _ask(method: str, **kwargs) -> Any:
    rid = _next_id()
    payload = {"op": "request", "id": rid, "method": method}
    payload.update(kwargs)
    _send(payload)
    while True:
        line = sys.stdin.readline()
        if not line:
            raise RuntimeError("Arbuz host closed")
        msg = json.loads(line)
        if msg.get("op") == "reply" and msg.get("id") == rid:
            if "error" in msg and "ok" not in msg and "value" not in msg and "values" not in msg:
                raise RuntimeError(msg["error"])
            return msg


def function(name: str, *, syntax: str = "", help_ru: str = "", help_en: str = ""):
    """Register a spreadsheet function, e.g. =VAT(A1)."""

    def deco(fn: Callable) -> Callable:
        key = name.upper()
        _functions[key] = fn
        _fn_meta.append(
            {
                "name": key,
                "syntax": syntax or f"{key}()",
                "help_ru": help_ru or fn.__doc__ or "",
                "help_en": help_en or fn.__doc__ or "",
                "plugin": _plugin_id,
            }
        )
        return fn

    return deco


def command(id: str, title_ru: str, title_en: str = ""):
    """Register a Плагины-menu action."""

    def deco(fn: Callable) -> Callable:
        _commands[id] = fn
        _cmd_meta.append(
            {
                "id": id,
                "title_ru": title_ru,
                "title_en": title_en or title_ru,
                "plugin": _plugin_id,
            }
        )
        return fn

    return deco


def on(event: str):
    """Subscribe to app events.

    Events: app_start, app_quit, cell_changed, sheet_changed,
    selection_changed, workbook_new, workbook_opened, before_save, after_save.
    The handler receives the event dict (name, a1, sheet, path, …).
    """

    def deco(fn: Callable) -> Callable:
        _events.setdefault(event, []).append(fn)
        return fn

    return deco


def error(code: str = "#VALUE!") -> dict:
    """Return an Excel-style error from a plugin function."""
    return {"error": code}


def col_letter(col: int) -> str:
    """0-based column index → A, B, …, Z, AA."""
    if col < 0:
        raise ValueError("col")
    name = ""
    n = col + 1
    while n > 0:
        n, rem = divmod(n - 1, 26)
        name = chr(ord("A") + rem) + name
    return name


def a1(row: int, col: int) -> str:
    """1-based row and 0-based? No: 1-based row and 1-based column → A1."""
    if row < 1 or col < 1:
        raise ValueError("row/col are 1-based")
    return f"{col_letter(col - 1)}{row}"


def get(addr: str, sheet: int | None = None) -> Any:
    """Displayed value of a cell (number if the cell looks numeric)."""
    req: dict[str, Any] = {"a1": addr}
    if sheet is not None:
        req["sheet"] = sheet
    msg = _ask("get", **req)
    raw = msg.get("value", "")
    if isinstance(raw, (int, float, bool)):
        return raw
    text = str(raw)
    try:
        if text and text[0] not in "#":
            return float(text) if ("." in text or "e" in text.lower()) else int(text)
    except ValueError:
        pass
    try:
        return float(text)
    except ValueError:
        return text


def get_raw(addr: str, sheet: int | None = None) -> str:
    """Formula or typed text, e.g. '=SUM(A1:A3)'."""
    req: dict[str, Any] = {"a1": addr}
    if sheet is not None:
        req["sheet"] = sheet
    return str(_ask("get", **req).get("raw", ""))


def set(addr: str, value: Any, sheet: int | None = None) -> None:
    req: dict[str, Any] = {"a1": addr, "value": "" if value is None else value}
    if sheet is not None:
        req["sheet"] = sheet
    _ask("set", **req)


def get_range(addr: str, sheet: int | None = None, *, raw: bool = False) -> list[list[Any]]:
    """2D list for A1:C3 (rows of columns)."""
    req: dict[str, Any] = {"a1": addr, "raw": raw}
    if sheet is not None:
        req["sheet"] = sheet
    return list(_ask("get_range", **req).get("values") or [])


def set_range(addr: str, values: Sequence[Any], sheet: int | None = None) -> None:
    """Write a 2D list (rows) or a 1D list along the long side of the range."""
    req: dict[str, Any] = {"a1": addr, "values": list(values)}
    if sheet is not None:
        req["sheet"] = sheet
    _ask("set_range", **req)


def sheets() -> list[str]:
    return list(_ask("sheets").get("sheets") or [])


def sheet_count() -> int:
    return int(_ask("sheets").get("count") or 0)


def current_sheet() -> int:
    return int(_ask("current_sheet").get("sheet") or 0)


def current_sheet_name() -> str:
    return str(_ask("current_sheet").get("name") or "")


def set_current_sheet(index: int) -> None:
    _ask("set_current_sheet", sheet=int(index))


def add_sheet(name: str = "") -> int:
    return int(_ask("add_sheet", name=name).get("sheet") or 0)


def rename_sheet(name: str, sheet: int | None = None) -> None:
    req: dict[str, Any] = {"name": name}
    if sheet is not None:
        req["sheet"] = sheet
    _ask("rename_sheet", **req)


def remove_sheet(sheet: int | None = None) -> None:
    req: dict[str, Any] = {}
    if sheet is not None:
        req["sheet"] = sheet
    _ask("remove_sheet", **req)


def current_cell() -> str:
    return str(_ask("current_cell").get("a1") or "A1")


def selection() -> str:
    """Active range as A1 or A1:B4."""
    return str(_ask("selection").get("range") or current_cell())


def row_count(sheet: int | None = None) -> int:
    req: dict[str, Any] = {}
    if sheet is not None:
        req["sheet"] = sheet
    return int(_ask("row_count", **req).get("rows") or 0)


def col_count(sheet: int | None = None) -> int:
    req: dict[str, Any] = {}
    if sheet is not None:
        req["sheet"] = sheet
    return int(_ask("col_count", **req).get("cols") or 0)


def numbers(values: Iterable[Any]) -> list[float]:
    """Flatten nested lists / range args and keep only numeric values."""
    out: list[float] = []

    def walk(x: Any) -> None:
        if isinstance(x, (list, tuple)):
            for i in x:
                walk(i)
            return
        if x is None or x == "":
            return
        try:
            out.append(float(x))
        except (TypeError, ValueError):
            return

    walk(values)
    return out


class Cell:
    """Lazy handle to one cell."""

    def __init__(self, addr: str, sheet: int | None = None):
        self.addr = addr
        self.sheet = sheet

    @property
    def value(self) -> Any:
        return get(self.addr, self.sheet)

    @value.setter
    def value(self, v: Any) -> None:
        set(self.addr, v, self.sheet)

    @property
    def raw(self) -> str:
        return get_raw(self.addr, self.sheet)


class Range:
    """Lazy handle to A1:C3 (or a single cell)."""

    def __init__(self, addr: str, sheet: int | None = None):
        self.addr = addr
        self.sheet = sheet

    def get(self, *, raw: bool = False) -> list[list[Any]]:
        return get_range(self.addr, self.sheet, raw=raw)

    def set(self, values: Sequence[Any]) -> None:
        set_range(self.addr, values, self.sheet)

    def numbers(self) -> list[float]:
        return numbers(self.get())


def _unwrap_arg(a: Any) -> Any:
    if isinstance(a, dict):
        if "error" in a:
            raise ValueError(a["error"])
        if "rows" in a:
            rows = a["rows"]
            if isinstance(rows, list) and len(rows) == 1 and isinstance(rows[0], list) and len(rows[0]) == 1:
                return rows[0][0]
            return rows
        if "values" in a:
            vals = a["values"]
            if isinstance(vals, list) and len(vals) == 1:
                return vals[0]
            return vals
        if "value" in a:
            return a["value"]
    return a


def _call_function(name: str, args: list) -> Any:
    fn = _functions.get(name.upper())
    if fn is None:
        raise NameError(name)
    return fn(*[_unwrap_arg(a) for a in args])


def registrations() -> dict:
    return {"functions": list(_fn_meta), "commands": list(_cmd_meta)}
