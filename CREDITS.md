# Credits

**Arbuz** is a table calculator by **SYFaren**.

The idea of “calculate in a grid on the screen” is older than Excel: VisiCalc (1979) was named *visible calculator*. Arbuz goes that way, not toward a spreadsheet suite with charts and pivot tables.

Below are the open projects without which Arbuz would not have been built. Thanks to the authors even where the license does not require it.

## Qt

- Name: Qt
- Authors: The Qt Company and contributors
- URL: https://www.qt.io/ and https://code.qt.io/cgit/qt/qtbase.git/
- License: LGPL-3 / GPL-3 / commercial
- What we used: Qt 6 Widgets, `QTableView` / `QAbstractTableModel`, `QWizard`, QSS. The official spreadsheet example (`qtbase/examples/widgets/itemviews/spreadsheet`) is the model for the grid, A/B/C headers, and a simple formula.

## QXlsx

- Name: QXlsx
- Authors: j2doll / QtExcel
- URL: https://github.com/QtExcel/QXlsx
- License: MIT
- What we used: the table-model and cell-loading samples from ExcelViewer and ExcelTableEditor (adapted for formulas, several sheets, and editing). Arbuz reads/writes `.xlsx` with its own ZIP+XML, because QXlsx `saveAs` hangs on this Qt build via the private QZipWriter.

## xlfparser

- Name: xlfparser
- Authors: PyXLL Ltd.
- URL: https://github.com/pyxll/xlfparser
- License: MIT
- What we used: Excel formula tokenization. The evaluator is written for Arbuz on top of those tokens and ideas from the Qt spreadsheet example.

## Other references (our code, approach borrowed)

- NewBediver/Qt-SimpleSpreadSheet — https://github.com/NewBediver/Qt-SimpleSpreadSheet — menu, find, recalc.
- RedNeath/ExcelFormulaCalculationEngine — https://github.com/RedNeath/ExcelFormulaCalculationEngine — keep the formula engine separate from the grid.
- Qt spreadsheet demo (`qtbase/demos/spreadsheet`, mirror radekp/qt) — formula bar: `currentItemChanged` → `QLineEdit`, Enter writes the cell.
- Qt Quarterly 25 Undo Framework / `QUndoStack` — a command per cell edit; `push()` runs `redo()`.
- QtExcel/QXlsx `xlsxutility.cpp` — Excel serial dates (epoch 1899-12-31 and the fake leap year 1900).
- QtExcel/QXlsx `xlsxstyles.cpp` / `xlsxworksheet.cpp` — `styles.xml`, `s=` on `<c>`, `<cols>`, `<mergeCells>`, `numFmtId` 9/14.
- Qt Frozen Column example — frozen panes: child `QTableView`s with synced scroll.
- Gnumeric `sheet_find_boundary_*` / `scg_cursor_move` — Ctrl+arrow jumps to the edge of a filled or empty block; Shift extends the selection. Ctrl+Home → A1, Ctrl+End → the used-range corner (`gnm_sheet_get_last_row/col`).
- Gnumeric autofill / LibreOffice `fillAuto` — the fill handle takes a source block: numbers become an arithmetic series, formulas shift references.
- Qt `QUndoStack`: a snapshot of sheets in one command; skip the first `redo()` from `push()` because the edit is already applied.
- ECMA-376 18.8.30 / QtExcel/QXlsx `xlsxformat.cpp` — built-in `numFmtId` (0, 2, 4, 9, 10, 11, 14, 20, 22).
- Excel / Gnumeric Find Previous — Shift+F3 searches backward and wraps.
- Typical Qt QSS theme loaders on GitHub — JSON palette + generated stylesheet.

Licenses: `third_party/QXlsx/LICENSE`, `third_party/xlfparser/LICENSE`, overview — `third_party/SOURCE.txt`.
