#ifndef ARBUZ_CELLREF_H
#define ARBUZ_CELLREF_H

#include <QString>
#include <functional>
#include <utility>

namespace CellRef {

struct Addr {
    int row = 0;
    int col = 0;
    bool absRow = false;
    bool absCol = false;
    QString sheet;
    bool valid = false;
};

QString columnName(int colZeroBased);
bool parseColumn(const QString &text, int *colZeroBased, int *consumed);
bool parseA1(const QString &text, int *rowZeroBased, int *colZeroBased);
bool parseA1Range(const QString &text, int *r1, int *c1, int *r2, int *c2);
QString a1(int rowZeroBased, int colZeroBased);

bool parseAddr(const QString &text, Addr *out);
bool parseRangeRef(const QString &text, Addr *a, Addr *b);
QString formatAddr(const Addr &a);
QString formatRange(const Addr &a, const Addr &b);

using AddrMap = std::function<Addr(const Addr &)>;
QString rewriteFormula(const QString &formula, const AddrMap &map);

QString adjustFormula(const QString &formula, int dRow, int dCol);
QString shiftFormulaInsertRow(const QString &formula, const QString &formulaSheet, const QString &targetSheet,
                              int at, int count);
QString shiftFormulaInsertCol(const QString &formula, const QString &formulaSheet, const QString &targetSheet,
                              int at, int count);
QString shiftFormulaDeleteRow(const QString &formula, const QString &formulaSheet, const QString &targetSheet,
                              int at, int count);
QString shiftFormulaDeleteCol(const QString &formula, const QString &formulaSheet, const QString &targetSheet,
                              int at, int count);

}

#endif
