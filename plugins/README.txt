Arbuz plugins (Python 3)
========================

This folder sits NEXT TO Arbuz.exe / Arbuz.sh — you see it immediately,
without digging through DLLs in runtime/.

How to add a plugin
-------------------
1. Create a subfolder, e.g. my-tools/
2. Put plugin.py in it
3. Menu Plugins → Reload plugins (or restart Arbuz)

Needs Python 3 on the system (python3 / python.exe on PATH).
The portable zip does NOT include an interpreter — the archive stays small,
and the launcher is not buried under thousands of files.

Minimal plugin.py
-----------------
    import arbuz

    @arbuz.function("VAT", syntax="VAT(amount)", help_en="20% VAT of the amount")
    def vat(amount):
        return float(amount or 0) * 0.20

    @arbuz.command("fill.hello", "Hello into current cell", "Hello into current cell")
    def hello():
        arbuz.set(arbuz.current_cell(), "hello")

    @arbuz.on("cell_changed")
    def changed(msg):
        pass  # msg["a1"], msg["sheet"]

API (module arbuz)
------------------
Functions and menu:
  @function(name, syntax=..., help_ru=..., help_en=...)
  @command(id, title_ru, title_en="")
  @on(event)          app_start, app_quit, cell_changed, sheet_changed,
                      selection_changed, workbook_new, workbook_opened,
                      before_save, after_save
  error("#VALUE!")    return an error from a plugin function

Cells:
  get("B2") / get_raw("B2") / set("B2", value)
  get_range("A1:C3") / set_range("A1:C3", [[1,2,3], ...])
  current_cell() / selection()
  Cell("A1"), Range("A1:B10")
  a1(row, col)        1-based, e.g. a1(1, 1) == "A1"
  numbers(values)     pull numbers from a range / nested lists

Sheets:
  sheets() / sheet_count() / current_sheet() / current_sheet_name()
  set_current_sheet(i) / add_sheet("Name") / rename_sheet("Name") / remove_sheet()
  row_count() / col_count()

Plugin functions show up in Insert → Function, category “Plugins”.
A range argument (VAT(A1:A3) or =MY(A1:B2)) arrives as a list of values.

Examples live in the Arbuz source tree (`example-vat`, `example-hello`, `example-tools`), not in the portable zip.

Environment:
  ARBUZ_PYTHON         full path to python if it is not on PATH
  ARBUZ_NO_PLUGINS=1   do not start the host (for tests)

Russian: see README.ru.txt
