#include "workbook.h"
#include "cellref.h"
#include "exceldate.h"
#include "formulaengine.h"
#include "numformat.h"

#include <QUndoCommand>
#include <QUndoStack>
#include <QtMath>
#include <algorithm>
#include <cmath>
#include <functional>

class SetCellCommand : public QUndoCommand
{
public:
    SetCellCommand(Workbook *wb, int sheet, int row, int col, const CellData &before, const CellData &after)
        : m_wb(wb)
        , m_sheet(sheet)
        , m_row(row)
        , m_col(col)
        , m_before(before)
        , m_after(after)
    {
        setText(QStringLiteral("Edit %1").arg(CellRef::a1(row, col)));
    }

    void undo() override { m_wb->applyCellData(m_sheet, m_row, m_col, m_before); }
    void redo() override { m_wb->applyCellData(m_sheet, m_row, m_col, m_after); }

private:
    Workbook *m_wb = nullptr;
    int m_sheet = 0;
    int m_row = 0;
    int m_col = 0;
    CellData m_before;
    CellData m_after;
};

class SheetsCommand : public QUndoCommand
{
public:
    SheetsCommand(Workbook *wb, const QVector<Worksheet> &before, const QVector<Worksheet> &after,
                  const QString &text)
        : m_wb(wb)
        , m_before(before)
        , m_after(after)
    {
        setText(text);
    }

    void undo() override { m_wb->restoreSheets(m_before); }
    void redo() override
    {
        if (m_skip) {
            m_skip = false;
            return;
        }
        m_wb->restoreSheets(m_after);
    }

private:
    Workbook *m_wb = nullptr;
    QVector<Worksheet> m_before;
    QVector<Worksheet> m_after;
    bool m_skip = true;
};

void Worksheet::setCell(int row, int col, const CellData &data)
{
    if (data.raw.isEmpty() && data.styleIsDefault()) {
        cells.remove(key(row, col));
        return;
    }
    if (row + 1 > rowCount)
        rowCount = row + 1 + 20;
    if (col + 1 > colCount)
        colCount = col + 1 + 5;
    cells.insert(key(row, col), data);
}

Workbook::Workbook(QObject *parent)
    : QObject(parent)
    , m_undo(new QUndoStack(this))
{
    resetToEmpty();
}

void Workbook::resetToEmpty()
{
    m_sheets.clear();
    addSheet(QStringLiteral("Лист1"));
    if (m_undo)
        m_undo->clear();
    emit structureChanged();
    emit contentsChanged();
}

void Workbook::pushSheetsUndo(const QVector<Worksheet> &before, const QString &text)
{
    if (!m_undoEnabled || m_applying || !m_undo)
        return;
    m_undo->push(new SheetsCommand(this, before, m_sheets, text));
}

void Workbook::restoreSheets(const QVector<Worksheet> &sheets)
{
    m_applying = true;
    m_sheets = sheets;
    m_applying = false;
    emit structureChanged();
    recalculate();
}

int Workbook::addSheet(const QString &name)
{
    const bool rec = m_undoEnabled && !m_applying && m_undo;
    QVector<Worksheet> before;
    if (rec)
        before = m_sheets;
    Worksheet ws;
    QString n = name;
    if (n.isEmpty())
        n = QStringLiteral("Лист%1").arg(m_sheets.size() + 1);
    ws.name = n;
    m_sheets.append(ws);
    emit structureChanged();
    if (rec)
        pushSheetsUndo(before, QStringLiteral("Add sheet"));
    return m_sheets.size() - 1;
}

bool Workbook::removeSheet(int index)
{
    if (m_sheets.size() <= 1 || index < 0 || index >= m_sheets.size())
        return false;
    const bool rec = m_undoEnabled && !m_applying && m_undo;
    QVector<Worksheet> before;
    if (rec)
        before = m_sheets;
    m_sheets.removeAt(index);
    emit structureChanged();
    emit contentsChanged();
    if (rec)
        pushSheetsUndo(before, QStringLiteral("Remove sheet"));
    return true;
}

bool Workbook::renameSheet(int index, const QString &name)
{
    if (index < 0 || index >= m_sheets.size() || name.trimmed().isEmpty())
        return false;
    const bool rec = m_undoEnabled && !m_applying && m_undo;
    QVector<Worksheet> before;
    if (rec)
        before = m_sheets;
    m_sheets[index].name = name.trimmed();
    emit structureChanged();
    if (rec)
        pushSheetsUndo(before, QStringLiteral("Rename sheet"));
    return true;
}

bool Workbook::moveSheet(int from, int to)
{
    if (from < 0 || to < 0 || from >= m_sheets.size() || to >= m_sheets.size() || from == to)
        return false;
    const bool rec = m_undoEnabled && !m_applying && m_undo;
    QVector<Worksheet> before;
    if (rec)
        before = m_sheets;
    m_sheets.move(from, to);
    emit structureChanged();
    if (rec)
        pushSheetsUndo(before, QStringLiteral("Move sheet"));
    return true;
}

int Workbook::sheetIndexByName(const QString &name) const
{
    for (int i = 0; i < m_sheets.size(); ++i) {
        if (m_sheets.at(i).name.compare(name, Qt::CaseInsensitive) == 0)
            return i;
    }
    return -1;
}

QString Workbook::cacheKey(int sheetIndex, int row, int col) const
{
    return QStringLiteral("%1!%2").arg(sheetIndex).arg(CellRef::a1(row, col));
}

static bool dateLikeFormula(const QString &raw)
{
    const QString u = raw.trimmed().toUpper();
    if (u.startsWith(QLatin1String("=YEAR(")) || u.startsWith(QLatin1String("=MONTH("))
        || u.startsWith(QLatin1String("=DAY(")) || u.startsWith(QLatin1String("=WEEKDAY(")))
        return false;
    return u.contains(QLatin1String("TODAY(")) || u.contains(QLatin1String("NOW("))
        || u.contains(QLatin1String("DATE(")) || u.contains(QLatin1String("EOMONTH("));
}

QString Workbook::evalCell(int sheetIndex, int row, int col)
{
    if (sheetIndex < 0 || sheetIndex >= m_sheets.size())
        return QStringLiteral("#REF!");
    const QString key = cacheKey(sheetIndex, row, col);
    if (m_cache.contains(key))
        return m_cache.value(key);
    if (m_visiting.contains(key))
        return QStringLiteral("#CYCLE!");

    const CellData cell = m_sheets[sheetIndex].cell(row, col);
    const QString raw = cell.raw.trimmed();
    if (raw.isEmpty()) {
        m_cache.insert(key, QString());
        return {};
    }
    if (!raw.startsWith(QLatin1Char('='))) {
        m_cache.insert(key, raw);
        return raw;
    }

    m_visiting.insert(key);
    FormulaEngine engine(
        [this](int sh, int r, int c) -> FormulaValue {
            const QString d = evalCell(sh, r, c);
            if (d.startsWith(QLatin1Char('#')))
                return FormulaValue::fromError(d);
            if (d.isEmpty())
                return FormulaValue();
            bool ok = false;
            const double n = d.toDouble(&ok);
            if (ok)
                return FormulaValue::fromNumber(n);
            if (d.compare(QLatin1String("TRUE"), Qt::CaseInsensitive) == 0)
                return FormulaValue::fromBool(true);
            if (d.compare(QLatin1String("FALSE"), Qt::CaseInsensitive) == 0)
                return FormulaValue::fromBool(false);
            const QDate iso = QDate::fromString(d.left(10), Qt::ISODate);
            if (iso.isValid() && d.size() >= 10 && d.at(4) == QLatin1Char('-'))
                return FormulaValue::fromNumber(ExcelDate::toSerial(iso));
            return FormulaValue::fromText(d);
        },
        sheetIndex,
        [this](const QString &name) { return sheetIndexByName(name); });
    const FormulaValue v = engine.evaluate(raw);
    m_visiting.remove(key);
    const QString display = v.toDisplay();
    m_cache.insert(key, display);
    return display;
}

QString Workbook::formatValue(int sheetIndex, int row, int col, const QString &rawDisplay) const
{
    const CellData cell = (sheetIndex >= 0 && sheetIndex < m_sheets.size())
        ? m_sheets[sheetIndex].cell(row, col)
        : CellData();
    if (rawDisplay.isEmpty() || rawDisplay.startsWith(QLatin1Char('#')))
        return rawDisplay;

    bool ok = false;
    double n = rawDisplay.toDouble(&ok);
    if (!ok) {
        const QDate iso = QDate::fromString(rawDisplay.left(10), Qt::ISODate);
        if (iso.isValid() && rawDisplay.size() >= 10 && rawDisplay.at(4) == QLatin1Char('-')) {
            n = ExcelDate::toSerial(iso);
            ok = true;
        }
    }
    if (!ok)
        return rawDisplay;

    int fmt = cell.numFmt;
    if (fmt == 0 && cell.raw.startsWith(QLatin1Char('=')) && dateLikeFormula(cell.raw))
        fmt = NumFormat::Date;
    if (fmt == 0)
        return rawDisplay;
    return NumFormat::format(n, fmt);
}

QString Workbook::displayText(int sheetIndex, int row, int col) const
{
    const QString t = const_cast<Workbook *>(this)->evalCell(sheetIndex, row, col);
    return formatValue(sheetIndex, row, col, t);
}

void Workbook::pushCellUndo(int sheet, int row, int col, const CellData &before, const CellData &after)
{
    if (!m_undoEnabled || m_applying || !m_undo)
        return;
    m_undo->push(new SetCellCommand(this, sheet, row, col, before, after));
}

void Workbook::applyCellData(int sheetIndex, int row, int col, const CellData &data)
{
    if (sheetIndex < 0 || sheetIndex >= m_sheets.size())
        return;
    m_applying = true;
    m_sheets[sheetIndex].setCell(row, col, data);
    m_applying = false;
    emit cellEdited(sheetIndex, row, col);
    recalculate();
}

void Workbook::setCellData(int sheetIndex, int row, int col, const CellData &data)
{
    if (sheetIndex < 0 || sheetIndex >= m_sheets.size())
        return;
    const CellData before = m_sheets[sheetIndex].cell(row, col);
    if (before.raw == data.raw && before.bold == data.bold && before.italic == data.italic
        && before.foreground == data.foreground && before.background == data.background
        && before.hAlign == data.hAlign && before.vAlign == data.vAlign && before.wrap == data.wrap
        && before.numFmt == data.numFmt && before.border == data.border)
        return;
    if (m_undoEnabled && !m_applying)
        pushCellUndo(sheetIndex, row, col, before, data);
    else
        applyCellData(sheetIndex, row, col, data);
}

void Workbook::setRaw(int sheetIndex, int row, int col, const QString &raw)
{
    if (sheetIndex < 0 || sheetIndex >= m_sheets.size())
        return;
    CellData cell = m_sheets[sheetIndex].cell(row, col);
    cell.raw = raw;
    setCellData(sheetIndex, row, col, cell);
}

void Workbook::setStyle(int sheetIndex, int row, int col, bool bold, bool italic, const QColor &fg, const QColor &bg)
{
    if (sheetIndex < 0 || sheetIndex >= m_sheets.size())
        return;
    CellData cell = m_sheets[sheetIndex].cell(row, col);
    cell.bold = bold;
    cell.italic = italic;
    cell.foreground = fg;
    cell.background = bg;
    setCellData(sheetIndex, row, col, cell);
}

void Workbook::recalculate()
{
    m_cache.clear();
    m_visiting.clear();
    emit contentsChanged();
}

void Workbook::beginUndoMacro(const QString &text)
{
    if (m_undo)
        m_undo->beginMacro(text);
}

void Workbook::endUndoMacro()
{
    if (m_undo)
        m_undo->endMacro();
}

void Workbook::rewriteFormulas(int targetSheet, const std::function<QString(const QString &, int)> &fn)
{
    if (targetSheet < 0 || targetSheet >= m_sheets.size())
        return;
    const QString targetName = m_sheets[targetSheet].name;
    Q_UNUSED(targetName);
    for (int s = 0; s < m_sheets.size(); ++s) {
        Worksheet &ws = m_sheets[s];
        const auto keys = ws.cells.keys();
        for (quint64 k : keys) {
            CellData d = ws.cells.value(k);
            if (!d.raw.startsWith(QLatin1Char('=')))
                continue;
            const QString next = fn(d.raw, s);
            if (next != d.raw) {
                d.raw = next;
                ws.cells.insert(k, d);
            }
        }
    }
}

void Workbook::insertRows(int sheetIndex, int at, int count)
{
    if (sheetIndex < 0 || sheetIndex >= m_sheets.size() || count <= 0 || at < 0)
        return;
    const bool rec = m_undoEnabled && !m_applying && m_undo;
    QVector<Worksheet> before;
    if (rec)
        before = m_sheets;
    applyInsertRows(sheetIndex, at, count);
    if (rec)
        pushSheetsUndo(before, QStringLiteral("Insert rows"));
}

void Workbook::applyInsertRows(int sheetIndex, int at, int count)
{
    Worksheet &ws = m_sheets[sheetIndex];
    QHash<quint64, CellData> next;
    for (auto it = ws.cells.cbegin(); it != ws.cells.cend(); ++it) {
        int r = int(it.key() >> 32);
        int c = int(it.key() & 0xffffffffu);
        if (r >= at)
            r += count;
        next.insert(Worksheet::key(r, c), it.value());
    }
    ws.cells = next;
    ws.rowCount += count;
    for (MergeRange &m : ws.merges) {
        if (m.r1 >= at)
            m.r1 += count;
        if (m.r2 >= at)
            m.r2 += count;
    }
    const QString target = ws.name;
    rewriteFormulas(sheetIndex, [&](const QString &f, int s) {
        return CellRef::shiftFormulaInsertRow(f, m_sheets[s].name, target, at, count);
    });
    emit structureChanged();
    recalculate();
}

void Workbook::removeRows(int sheetIndex, int at, int count)
{
    if (sheetIndex < 0 || sheetIndex >= m_sheets.size() || count <= 0 || at < 0)
        return;
    const bool rec = m_undoEnabled && !m_applying && m_undo;
    QVector<Worksheet> before;
    if (rec)
        before = m_sheets;
    applyRemoveRows(sheetIndex, at, count);
    if (rec)
        pushSheetsUndo(before, QStringLiteral("Delete rows"));
}

void Workbook::applyRemoveRows(int sheetIndex, int at, int count)
{
    Worksheet &ws = m_sheets[sheetIndex];
    QHash<quint64, CellData> next;
    for (auto it = ws.cells.cbegin(); it != ws.cells.cend(); ++it) {
        int r = int(it.key() >> 32);
        int c = int(it.key() & 0xffffffffu);
        if (r >= at && r < at + count)
            continue;
        if (r >= at + count)
            r -= count;
        next.insert(Worksheet::key(r, c), it.value());
    }
    ws.cells = next;
    ws.rowCount = qMax(1, ws.rowCount - count);
    QVector<MergeRange> merges;
    for (MergeRange m : ws.merges) {
        if (m.r2 < at || m.r1 >= at + count) {
            if (m.r1 >= at + count)
                m.r1 -= count;
            if (m.r2 >= at + count)
                m.r2 -= count;
            merges.append(m);
        }
    }
    ws.merges = merges;
    const QString target = ws.name;
    rewriteFormulas(sheetIndex, [&](const QString &f, int s) {
        return CellRef::shiftFormulaDeleteRow(f, m_sheets[s].name, target, at, count);
    });
    emit structureChanged();
    recalculate();
}

void Workbook::insertColumns(int sheetIndex, int at, int count)
{
    if (sheetIndex < 0 || sheetIndex >= m_sheets.size() || count <= 0 || at < 0)
        return;
    const bool rec = m_undoEnabled && !m_applying && m_undo;
    QVector<Worksheet> before;
    if (rec)
        before = m_sheets;
    applyInsertColumns(sheetIndex, at, count);
    if (rec)
        pushSheetsUndo(before, QStringLiteral("Insert columns"));
}

void Workbook::applyInsertColumns(int sheetIndex, int at, int count)
{
    Worksheet &ws = m_sheets[sheetIndex];
    QHash<quint64, CellData> next;
    QHash<int, int> widths;
    for (auto it = ws.cells.cbegin(); it != ws.cells.cend(); ++it) {
        int r = int(it.key() >> 32);
        int c = int(it.key() & 0xffffffffu);
        if (c >= at)
            c += count;
        next.insert(Worksheet::key(r, c), it.value());
    }
    for (auto it = ws.columnWidths.cbegin(); it != ws.columnWidths.cend(); ++it) {
        int c = it.key();
        if (c >= at)
            c += count;
        widths.insert(c, it.value());
    }
    ws.cells = next;
    ws.columnWidths = widths;
    ws.colCount += count;
    for (MergeRange &m : ws.merges) {
        if (m.c1 >= at)
            m.c1 += count;
        if (m.c2 >= at)
            m.c2 += count;
    }
    const QString target = ws.name;
    rewriteFormulas(sheetIndex, [&](const QString &f, int s) {
        return CellRef::shiftFormulaInsertCol(f, m_sheets[s].name, target, at, count);
    });
    emit structureChanged();
    recalculate();
}

void Workbook::removeColumns(int sheetIndex, int at, int count)
{
    if (sheetIndex < 0 || sheetIndex >= m_sheets.size() || count <= 0 || at < 0)
        return;
    const bool rec = m_undoEnabled && !m_applying && m_undo;
    QVector<Worksheet> before;
    if (rec)
        before = m_sheets;
    applyRemoveColumns(sheetIndex, at, count);
    if (rec)
        pushSheetsUndo(before, QStringLiteral("Delete columns"));
}

void Workbook::applyRemoveColumns(int sheetIndex, int at, int count)
{
    Worksheet &ws = m_sheets[sheetIndex];
    QHash<quint64, CellData> next;
    QHash<int, int> widths;
    for (auto it = ws.cells.cbegin(); it != ws.cells.cend(); ++it) {
        int r = int(it.key() >> 32);
        int c = int(it.key() & 0xffffffffu);
        if (c >= at && c < at + count)
            continue;
        if (c >= at + count)
            c -= count;
        next.insert(Worksheet::key(r, c), it.value());
    }
    for (auto it = ws.columnWidths.cbegin(); it != ws.columnWidths.cend(); ++it) {
        int c = it.key();
        if (c >= at && c < at + count)
            continue;
        if (c >= at + count)
            c -= count;
        widths.insert(c, it.value());
    }
    ws.cells = next;
    ws.columnWidths = widths;
    ws.colCount = qMax(1, ws.colCount - count);
    QVector<MergeRange> merges;
    for (MergeRange m : ws.merges) {
        if (m.c2 < at || m.c1 >= at + count) {
            if (m.c1 >= at + count)
                m.c1 -= count;
            if (m.c2 >= at + count)
                m.c2 -= count;
            merges.append(m);
        }
    }
    ws.merges = merges;
    const QString target = ws.name;
    rewriteFormulas(sheetIndex, [&](const QString &f, int s) {
        return CellRef::shiftFormulaDeleteCol(f, m_sheets[s].name, target, at, count);
    });
    emit structureChanged();
    recalculate();
}

void Workbook::mergeCells(int sheetIndex, int r1, int c1, int r2, int c2)
{
    if (sheetIndex < 0 || sheetIndex >= m_sheets.size())
        return;
    if (r1 > r2)
        std::swap(r1, r2);
    if (c1 > c2)
        std::swap(c1, c2);
    if (r1 == r2 && c1 == c2)
        return;
    const bool rec = m_undoEnabled && !m_applying && m_undo;
    QVector<Worksheet> before;
    if (rec)
        before = m_sheets;
    Worksheet &ws = m_sheets[sheetIndex];
    QVector<MergeRange> kept;
    for (const MergeRange &m : ws.merges) {
        const bool overlap = !(m.r2 < r1 || m.r1 > r2 || m.c2 < c1 || m.c1 > c2);
        if (!overlap)
            kept.append(m);
    }
    ws.merges = kept;
    ws.merges.append(MergeRange{r1, c1, r2, c2});
    emit structureChanged();
    emit contentsChanged();
    if (rec)
        pushSheetsUndo(before, QStringLiteral("Merge"));
}

void Workbook::unmergeAt(int sheetIndex, int row, int col)
{
    if (sheetIndex < 0 || sheetIndex >= m_sheets.size())
        return;
    const bool rec = m_undoEnabled && !m_applying && m_undo;
    QVector<Worksheet> before;
    if (rec)
        before = m_sheets;
    Worksheet &ws = m_sheets[sheetIndex];
    QVector<MergeRange> kept;
    for (const MergeRange &m : ws.merges) {
        if (!(row >= m.r1 && row <= m.r2 && col >= m.c1 && col <= m.c2))
            kept.append(m);
    }
    ws.merges = kept;
    emit structureChanged();
    emit contentsChanged();
    if (rec)
        pushSheetsUndo(before, QStringLiteral("Unmerge"));
}

MergeRange Workbook::mergeAt(int sheetIndex, int row, int col) const
{
    MergeRange none;
    if (sheetIndex < 0 || sheetIndex >= m_sheets.size())
        return none;
    for (const MergeRange &m : m_sheets[sheetIndex].merges) {
        if (row >= m.r1 && row <= m.r2 && col >= m.c1 && col <= m.c2)
            return m;
    }
    return none;
}

void Workbook::setFreeze(int sheetIndex, int rows, int cols)
{
    if (sheetIndex < 0 || sheetIndex >= m_sheets.size())
        return;
    const bool rec = m_undoEnabled && !m_applying && m_undo;
    QVector<Worksheet> before;
    if (rec)
        before = m_sheets;
    m_sheets[sheetIndex].freezeRows = qMax(0, rows);
    m_sheets[sheetIndex].freezeCols = qMax(0, cols);
    emit structureChanged();
    if (rec)
        pushSheetsUndo(before, QStringLiteral("Freeze"));
}

void Workbook::sortRange(int sheetIndex, int r1, int c1, int r2, int c2, int keyCol, bool ascending)
{
    if (sheetIndex < 0 || sheetIndex >= m_sheets.size())
        return;
    if (r1 > r2)
        std::swap(r1, r2);
    if (c1 > c2)
        std::swap(c1, c2);
    if (keyCol < c1 || keyCol > c2)
        keyCol = c1;
    const bool rec = m_undoEnabled && !m_applying && m_undo;
    QVector<Worksheet> before;
    if (rec)
        before = m_sheets;
    Worksheet &ws = m_sheets[sheetIndex];
    QVector<int> order;
    for (int r = r1; r <= r2; ++r)
        order.append(r);
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
        const QString sa = displayText(sheetIndex, a, keyCol);
        const QString sb = displayText(sheetIndex, b, keyCol);
        bool oka = false, okb = false;
        const double na = sa.toDouble(&oka);
        const double nb = sb.toDouble(&okb);
        int cmp = 0;
        if (oka && okb)
            cmp = (na < nb) ? -1 : (na > nb ? 1 : 0);
        else
            cmp = QString::compare(sa, sb, Qt::CaseInsensitive);
        return ascending ? cmp < 0 : cmp > 0;
    });
    QVector<QVector<CellData>> rows;
    for (int r = r1; r <= r2; ++r) {
        QVector<CellData> row;
        for (int c = c1; c <= c2; ++c)
            row.append(ws.cell(r, c));
        rows.append(row);
    }
    for (int i = 0; i < order.size(); ++i) {
        const int src = order.at(i) - r1;
        for (int c = c1; c <= c2; ++c)
            ws.setCell(r1 + i, c, rows.at(src).at(c - c1));
    }
    recalculate();
    if (rec)
        pushSheetsUndo(before, QStringLiteral("Sort"));
}

static int posMod(int a, int m)
{
    if (m <= 0)
        return 0;
    int r = a % m;
    if (r < 0)
        r += m;
    return r;
}

static QString numberRaw(double n)
{
    if (std::isfinite(n) && qAbs(n - std::round(n)) < 1e-9)
        return QString::number(qint64(std::llround(n)));
    return QString::number(n, 'g', 12);
}

static bool parseFillNumber(const CellData &d, double *n, bool *isDate)
{
    *isDate = false;
    const QString t = d.raw.trimmed();
    if (t.isEmpty() || t.startsWith(QLatin1Char('=')))
        return false;
    bool ok = false;
    *n = t.toDouble(&ok);
    if (ok) {
        *isDate = NumFormat::isDateTime(d.numFmt);
        return true;
    }
    const QDate iso = QDate::fromString(t.left(10), Qt::ISODate);
    if (iso.isValid() && t.size() >= 10 && t.at(4) == QLatin1Char('-')) {
        *n = ExcelDate::toSerial(iso);
        *isDate = true;
        return true;
    }
    return false;
}

void Workbook::fill(int sheetIndex, int sr1, int sc1, int sr2, int sc2, int er1, int ec1, int er2, int ec2)
{
    if (sheetIndex < 0 || sheetIndex >= m_sheets.size())
        return;
    if (sr1 > sr2)
        std::swap(sr1, sr2);
    if (sc1 > sc2)
        std::swap(sc1, sc2);
    if (er1 > er2)
        std::swap(er1, er2);
    if (ec1 > ec2)
        std::swap(ec1, ec2);
    er1 = qMin(er1, sr1);
    er2 = qMax(er2, sr2);
    ec1 = qMin(ec1, sc1);
    ec2 = qMax(ec2, sc2);
    if (er1 == sr1 && er2 == sr2 && ec1 == sc1 && ec2 == sc2)
        return;

    const int srcH = sr2 - sr1 + 1;
    const int srcW = sc2 - sc1 + 1;
    const int extraV = (er2 - er1 + 1) - srcH;
    const int extraH = (ec2 - ec1 + 1) - srcW;
    const bool vertical = extraV >= extraH;
    const Worksheet &srcSheet = m_sheets[sheetIndex];

    const bool rec = m_undoEnabled && m_undo;
    if (rec)
        beginUndoMacro(QStringLiteral("Fill"));

    auto writeCell = [&](int r, int c, const CellData &d) {
        if (r >= sr1 && r <= sr2 && c >= sc1 && c <= sc2)
            return;
        setCellData(sheetIndex, r, c, d);
    };

    auto fillAxis = [&](int srcA1, int srcA2, int destA1, int destA2, int srcB, int destB, bool vert) {
        const int srcLen = srcA2 - srcA1 + 1;
        QVector<CellData> srcs;
        srcs.reserve(srcLen);
        for (int a = srcA1; a <= srcA2; ++a)
            srcs.append(vert ? srcSheet.cell(a, srcB) : srcSheet.cell(srcB, a));

        bool allN = srcLen > 0;
        QVector<double> nums;
        bool anyDate = false;
        for (const CellData &d : srcs) {
            double n = 0;
            bool isDate = false;
            if (!parseFillNumber(d, &n, &isDate)) {
                allN = false;
                break;
            }
            nums.append(n);
            anyDate = anyDate || isDate;
        }

        double step = 1.0;
        if (allN && srcLen >= 2)
            step = (nums.last() - nums.first()) / double(srcLen - 1);

        const int dB = destB - srcB;
        for (int a = destA1; a <= destA2; ++a) {
            const int k = a - srcA1;
            const int idx = posMod(k, srcLen);
            CellData d = srcs.at(idx);
            if (allN) {
                d.raw = numberRaw(nums.first() + step * k);
                if (anyDate && d.numFmt == 0)
                    d.numFmt = NumFormat::Date;
            } else if (d.raw.startsWith(QLatin1Char('='))) {
                const int srcA = srcA1 + idx;
                const int dA = a - srcA;
                d.raw = CellRef::adjustFormula(d.raw, vert ? dA : dB, vert ? dB : dA);
            }
            if (vert)
                writeCell(a, destB, d);
            else
                writeCell(destB, a, d);
        }
    };

    if (vertical) {
        for (int c = ec1; c <= ec2; ++c) {
            const int srcC = sc1 + posMod(c - sc1, srcW);
            fillAxis(sr1, sr2, er1, er2, srcC, c, true);
        }
    } else {
        for (int r = er1; r <= er2; ++r) {
            const int srcR = sr1 + posMod(r - sr1, srcH);
            fillAxis(sc1, sc2, ec1, ec2, srcR, r, false);
        }
    }
    if (rec)
        endUndoMacro();
}

bool Workbook::findNext(int sheetIndex, const QString &needle, int fromRow, int fromCol, int *row, int *col,
                        bool wrap) const
{
    if (needle.isEmpty() || sheetIndex < 0 || sheetIndex >= m_sheets.size() || !row || !col)
        return false;
    const Worksheet &ws = m_sheets[sheetIndex];
    const int rows = ws.rowCount;
    const int cols = ws.colCount;
    if (rows <= 0 || cols <= 0)
        return false;

    auto match = [&](int r, int c) {
        const QString d = displayText(sheetIndex, r, c);
        const QString raw = ws.cell(r, c).raw;
        return d.contains(needle, Qt::CaseInsensitive) || raw.contains(needle, Qt::CaseInsensitive);
    };

    int r = fromRow;
    int c = fromCol + 1;
    if (c < 0) {
        c = 0;
    } else if (c >= cols) {
        c = 0;
        ++r;
    }
    for (; r < rows; ++r) {
        for (; c < cols; ++c) {
            if (match(r, c)) {
                *row = r;
                *col = c;
                return true;
            }
        }
        c = 0;
    }
    if (!wrap)
        return false;
    const int lastR = qBound(0, fromRow, rows - 1);
    const int lastC = qBound(0, fromCol, cols - 1);
    for (r = 0; r <= lastR; ++r) {
        const int cMax = (r == lastR) ? lastC : cols - 1;
        for (c = 0; c <= cMax; ++c) {
            if (match(r, c)) {
                *row = r;
                *col = c;
                return true;
            }
        }
    }
    return false;
}

bool Workbook::findPrev(int sheetIndex, const QString &needle, int fromRow, int fromCol, int *row, int *col,
                        bool wrap) const
{
    if (needle.isEmpty() || sheetIndex < 0 || sheetIndex >= m_sheets.size() || !row || !col)
        return false;
    const Worksheet &ws = m_sheets[sheetIndex];
    const int rows = ws.rowCount;
    const int cols = ws.colCount;
    if (rows <= 0 || cols <= 0)
        return false;

    auto match = [&](int r, int c) {
        const QString d = displayText(sheetIndex, r, c);
        const QString raw = ws.cell(r, c).raw;
        return d.contains(needle, Qt::CaseInsensitive) || raw.contains(needle, Qt::CaseInsensitive);
    };

    int r = fromRow;
    int c = fromCol - 1;
    if (c >= cols)
        c = cols - 1;
    if (c < 0) {
        c = cols - 1;
        --r;
    }
    for (; r >= 0; --r) {
        for (; c >= 0; --c) {
            if (match(r, c)) {
                *row = r;
                *col = c;
                return true;
            }
        }
        c = cols - 1;
    }
    if (!wrap)
        return false;
    const int lastR = qBound(0, fromRow, rows - 1);
    const int lastC = qBound(0, fromCol, cols - 1);
    for (r = rows - 1; r >= lastR; --r) {
        const int cMin = (r == lastR) ? lastC : 0;
        for (c = cols - 1; c >= cMin; --c) {
            if (match(r, c)) {
                *row = r;
                *col = c;
                return true;
            }
        }
    }
    return false;
}

bool Workbook::usedCorner(int sheetIndex, int *row, int *col) const
{
    if (!row || !col || sheetIndex < 0 || sheetIndex >= m_sheets.size())
        return false;
    const Worksheet &ws = m_sheets[sheetIndex];
    int r = 0;
    int c = 0;
    bool any = false;
    for (auto it = ws.cells.cbegin(); it != ws.cells.cend(); ++it) {
        if (it.value().raw.isEmpty() && it.value().styleIsDefault())
            continue;
        any = true;
        r = qMax(r, int(it.key() >> 32));
        c = qMax(c, int(it.key() & 0xffffffffu));
    }
    *row = r;
    *col = c;
    return any;
}
