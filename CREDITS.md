# Благодарности

Программа **Arbuz**, создатель **SYFaren**.

Ниже — открытые проекты, без которых Arbuz не был бы собран. Благодарим авторов даже там, где лицензия этого не требует.

## Qt

- Название: Qt
- Авторы: The Qt Company и сообщество
- URL: https://www.qt.io/ и https://code.qt.io/cgit/qt/qtbase.git/
- Лицензия: LGPL-3 / GPL-3 / коммерческая
- Что использовали: Qt 6 Widgets, `QTableView` / `QAbstractTableModel`, `QWizard`, QSS. Официальный пример spreadsheet (`qtbase/examples/widgets/itemviews/spreadsheet`) — образец сетки, заголовков A/B/C и простой формулы.

## QXlsx

- Название: QXlsx
- Авторы: j2doll / QtExcel
- URL: https://github.com/QtExcel/QXlsx
- Лицензия: MIT
- Что использовали: образец модели таблицы и загрузки ячеек из примеров ExcelViewer и ExcelTableEditor (доработаны под формулы, несколько листов и редактирование). Чтение/запись `.xlsx` в Arbuz — свой ZIP+XML, потому что QXlsx `saveAs` на этой сборке Qt зависает на приватном QZipWriter.

## xlfparser

- Название: xlfparser
- Авторы: PyXLL Ltd.
- URL: https://github.com/pyxll/xlfparser
- Лицензия: MIT
- Что использовали: токенизация Excel-формул. Вычислитель написан для Arbuz поверх этих токенов и идей Qt spreadsheet example.

## Прочие ориентиры (свой код, подход подсмотрен)

- NewBediver/Qt-SimpleSpreadSheet — https://github.com/NewBediver/Qt-SimpleSpreadSheet — меню, поиск, пересчёт.
- RedNeath/ExcelFormulaCalculationEngine — https://github.com/RedNeath/ExcelFormulaCalculationEngine — идея отдельного формульного движка от сетки.
- Qt spreadsheet demo (`qtbase/demos/spreadsheet`, зеркало radekp/qt) — строка формул: `currentItemChanged` → `QLineEdit`, Enter пишет в ячейку.
- Qt Quarterly 25 Undo Framework / `QUndoStack` — команда на правку ячейки, `push()` вызывает `redo()`.
- QtExcel/QXlsx `xlsxutility.cpp` — Excel serial дат (эпоха 1899-12-31 и фальшивый високосный 1900).
- QtExcel/QXlsx `xlsxstyles.cpp` / `xlsxworksheet.cpp` — `styles.xml`, `s=` на `<c>`, `<cols>`, `<mergeCells>`, `numFmtId` 9/14.
- Qt Frozen Column example — закреплённые области: дочерние `QTableView` с синхронизацией скролла.
- Gnumeric `sheet_find_boundary_*` / `scg_cursor_move` — Ctrl+стрелка прыгает к краю блока заполненных или пустых ячеек; Shift расширяет выделение. Ctrl+Home → A1, Ctrl+End → угол использованного диапазона (`gnm_sheet_get_last_row/col`).
- Gnumeric autofill / LibreOffice `fillAuto` — маркер заполнения берёт блок-источник: числа дают арифметическую серию, формулы сдвигают ссылки.
- Qt `QUndoStack`: снимок листов в одной команде; первый `redo()` от `push()` пропускаем, потому что правка уже применена.
- ECMA-376 18.8.30 / QtExcel/QXlsx `xlsxformat.cpp` — встроенные `numFmtId` (0, 2, 4, 9, 10, 11, 14, 20, 22).
- Excel / Gnumeric Find Previous — Shift+F3 ищет назад и зацикливается.
- Типичные Qt QSS-загрузчики тем на GitHub — JSON-палитра + генерация stylesheet.

Лицензии исходников лежат рядом с кодом в `third_party/`.
