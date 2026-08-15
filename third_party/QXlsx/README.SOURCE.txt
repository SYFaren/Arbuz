QXlsx (https://github.com/QtExcel/QXlsx) — MIT

Arbuz does not compile or link QXlsx. The full vendor tree was removed to keep
the repo small. We kept only this LICENSE for attribution.

What we used as reference (see CREDITS.md):
- ExcelViewer / ExcelTableEditor — table model and cell loading patterns
- xlsxutility / xlsxstyles / xlsxworksheet / xlsxformat — Excel dates, styles.xml,
  numFmtId, cols, mergeCells ideas

Reading and writing .xlsx in Arbuz is our own ZIP+XML in src/io/fileio.cpp,
because QXlsx saveAs hung on this Qt build (private QZipWriter).
