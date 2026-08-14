#ifndef ARBUZ_WORKBOOK_H
#define ARBUZ_WORKBOOK_H

#include <QColor>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QVector>
#include <functional>

class QUndoStack;

struct MergeRange {
    int r1 = 0;
    int c1 = 0;
    int r2 = 0;
    int c2 = 0;
};

struct CellData {
    QString raw;
    bool bold = false;
    bool italic = false;
    QColor foreground;
    QColor background;
    int hAlign = 0;
    int vAlign = 0;
    bool wrap = false;
    int numFmt = 0;
    int border = 0;

    bool styleIsDefault() const
    {
        return !bold && !italic && !foreground.isValid() && !background.isValid() && hAlign == 0 && vAlign == 0
            && !wrap && numFmt == 0 && border == 0;
    }
};

class Worksheet
{
public:
    QString name;
    int rowCount = 200;
    int colCount = 40;
    QHash<quint64, CellData> cells;
    QHash<int, int> columnWidths;
    QVector<MergeRange> merges;
    int freezeRows = 0;
    int freezeCols = 0;

    static quint64 key(int row, int col) { return (quint64(uint(row)) << 32) | uint(col); }

    CellData cell(int row, int col) const { return cells.value(key(row, col)); }
    void setCell(int row, int col, const CellData &data);
    void clearCell(int row, int col) { cells.remove(key(row, col)); }
};

class Workbook : public QObject
{
    Q_OBJECT
public:
    explicit Workbook(QObject *parent = nullptr);

    void resetToEmpty();
    int sheetCount() const { return m_sheets.size(); }
    Worksheet &sheet(int i) { return m_sheets[i]; }
    const Worksheet &sheet(int i) const { return m_sheets.at(i); }
    int addSheet(const QString &name = QString());
    bool removeSheet(int index);
    bool renameSheet(int index, const QString &name);
    bool moveSheet(int from, int to);
    int sheetIndexByName(const QString &name) const;

    QString displayText(int sheetIndex, int row, int col) const;
    void setRaw(int sheetIndex, int row, int col, const QString &raw);
    void setStyle(int sheetIndex, int row, int col, bool bold, bool italic, const QColor &fg, const QColor &bg);
    void setCellData(int sheetIndex, int row, int col, const CellData &data);
    void applyCellData(int sheetIndex, int row, int col, const CellData &data);

    void insertRows(int sheetIndex, int at, int count = 1);
    void removeRows(int sheetIndex, int at, int count = 1);
    void insertColumns(int sheetIndex, int at, int count = 1);
    void removeColumns(int sheetIndex, int at, int count = 1);

    void fill(int sheetIndex, int sr1, int sc1, int sr2, int sc2, int er1, int ec1, int er2, int ec2);
    void restoreSheets(const QVector<Worksheet> &sheets);
    bool findNext(int sheetIndex, const QString &needle, int fromRow, int fromCol, int *row, int *col,
                  bool wrap) const;
    bool findPrev(int sheetIndex, const QString &needle, int fromRow, int fromCol, int *row, int *col,
                  bool wrap) const;
    bool usedCorner(int sheetIndex, int *row, int *col) const;

    void mergeCells(int sheetIndex, int r1, int c1, int r2, int c2);
    void unmergeAt(int sheetIndex, int row, int col);
    MergeRange mergeAt(int sheetIndex, int row, int col) const;

    void setFreeze(int sheetIndex, int rows, int cols);
    void sortRange(int sheetIndex, int r1, int c1, int r2, int c2, int keyCol, bool ascending);

    void recalculate();

    QUndoStack *undoStack() { return m_undo; }
    void beginUndoMacro(const QString &text);
    void endUndoMacro();
    void setUndoEnabled(bool on) { m_undoEnabled = on; }

signals:
    void structureChanged();
    void contentsChanged();
    void cellEdited(int sheet, int row, int col);

private:
    friend class SetCellCommand;
    friend class SheetsCommand;

    QVector<Worksheet> m_sheets;
    QHash<QString, QString> m_cache;
    QSet<QString> m_visiting;
    QUndoStack *m_undo = nullptr;
    bool m_undoEnabled = true;
    bool m_applying = false;

    QString evalCell(int sheetIndex, int row, int col);
    QString cacheKey(int sheetIndex, int row, int col) const;
    QString formatValue(int sheetIndex, int row, int col, const QString &rawDisplay) const;
    void rewriteFormulas(int targetSheet, const std::function<QString(const QString &, int)> &fn);
    void pushCellUndo(int sheet, int row, int col, const CellData &before, const CellData &after);
    void pushSheetsUndo(const QVector<Worksheet> &before, const QString &text);
    void applyInsertRows(int sheetIndex, int at, int count);
    void applyRemoveRows(int sheetIndex, int at, int count);
    void applyInsertColumns(int sheetIndex, int at, int count);
    void applyRemoveColumns(int sheetIndex, int at, int count);
};

#endif
