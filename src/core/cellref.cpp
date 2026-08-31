#include "cellref.h"

#include <QString>

namespace CellRef {

QString columnName(int colZeroBased)
{
    if (colZeroBased < 0)
        return QString();
    QString name;
    int n = colZeroBased + 1;
    while (n > 0) {
        --n;
        name.prepend(QChar(QLatin1Char('A' + (n % 26))));
        n /= 26;
    }
    return name;
}

bool parseColumn(const QString &text, int *colZeroBased, int *consumed)
{
    int i = 0;
    while (i < text.size() && text.at(i) == QLatin1Char('$'))
        ++i;
    int col = 0;
    int letters = 0;
    while (i < text.size()) {
        const QChar ch = text.at(i).toUpper();
        if (ch < QLatin1Char('A') || ch > QLatin1Char('Z'))
            break;
        col = col * 26 + (ch.unicode() - 'A' + 1);
        ++letters;
        ++i;
    }
    if (letters == 0)
        return false;
    if (colZeroBased)
        *colZeroBased = col - 1;
    if (consumed)
        *consumed = i;
    return true;
}

static bool parseAddrBody(const QString &s, int from, Addr *out, int *consumed)
{
    int i = from;
    Addr a;
    if (i < s.size() && s.at(i) == QLatin1Char('$')) {
        a.absCol = true;
        ++i;
    }
    int col = 0;
    int letters = 0;
    while (i < s.size()) {
        const QChar ch = s.at(i).toUpper();
        if (ch < QLatin1Char('A') || ch > QLatin1Char('Z'))
            break;
        col = col * 26 + (ch.unicode() - 'A' + 1);
        ++letters;
        ++i;
    }
    if (letters == 0)
        return false;
    if (i < s.size() && s.at(i) == QLatin1Char('$')) {
        a.absRow = true;
        ++i;
    }
    if (i >= s.size() || !s.at(i).isDigit())
        return false;
    int row = 0;
    while (i < s.size() && s.at(i).isDigit()) {
        row = row * 10 + s.at(i).digitValue();
        ++i;
    }
    if (row <= 0)
        return false;
    a.row = row - 1;
    a.col = col - 1;
    a.valid = true;
    if (out)
        *out = a;
    if (consumed)
        *consumed = i - from;
    return true;
}

static bool parseSheetPrefix(const QString &s, int from, QString *sheet, int *consumed)
{
    int i = from;
    if (i < s.size() && s.at(i) == QLatin1Char('\'')) {
        ++i;
        QString name;
        while (i < s.size()) {
            if (s.at(i) == QLatin1Char('\'')) {
                ++i;
                if (i < s.size() && s.at(i) == QLatin1Char('\'')) {
                    name.append(QLatin1Char('\''));
                    ++i;
                    continue;
                }
                break;
            }
            name.append(s.at(i));
            ++i;
        }
        if (i >= s.size() || s.at(i) != QLatin1Char('!'))
            return false;
        ++i;
        if (sheet)
            *sheet = name;
        if (consumed)
            *consumed = i - from;
        return true;
    }
    int j = i;
    while (j < s.size()) {
        const QChar c = s.at(j);
        if (c.isLetterOrNumber() || c == QLatin1Char('_') || c == QLatin1Char('.'))
            ++j;
        else
            break;
    }
    if (j < s.size() && s.at(j) == QLatin1Char('!')) {
        if (sheet)
            *sheet = s.mid(i, j - i);
        if (consumed)
            *consumed = j + 1 - from;
        return true;
    }
    return false;
}

bool parseA1(const QString &text, int *rowZeroBased, int *colZeroBased)
{
    Addr a;
    if (!parseAddr(text.trimmed(), &a) || !a.valid)
        return false;
    if (rowZeroBased)
        *rowZeroBased = a.row;
    if (colZeroBased)
        *colZeroBased = a.col;
    return true;
}

bool parseAddr(const QString &text, Addr *out)
{
    const QString s = text.trimmed();
    int i = 0;
    QString sheet;
    int used = 0;
    if (parseSheetPrefix(s, 0, &sheet, &used))
        i = used;
    Addr a;
    int body = 0;
    if (!parseAddrBody(s, i, &a, &body))
        return false;
    if (i + body != s.size())
        return false;
    a.sheet = sheet;
    if (out)
        *out = a;
    return true;
}

bool parseA1Range(const QString &text, int *r1, int *c1, int *r2, int *c2)
{
    Addr a, b;
    if (!parseRangeRef(text, &a, &b))
        return false;
    int ra = a.row, ca = a.col, rb = b.row, cb = b.col;
    if (ra > rb)
        std::swap(ra, rb);
    if (ca > cb)
        std::swap(ca, cb);
    if (r1)
        *r1 = ra;
    if (c1)
        *c1 = ca;
    if (r2)
        *r2 = rb;
    if (c2)
        *c2 = cb;
    return true;
}

bool parseRangeRef(const QString &text, Addr *a, Addr *b)
{
    const QString s = text.trimmed();
    int i = 0;
    QString sheet;
    int used = 0;
    if (parseSheetPrefix(s, 0, &sheet, &used))
        i = used;
    Addr first;
    int body = 0;
    if (!parseAddrBody(s, i, &first, &body))
        return false;
    first.sheet = sheet;
    i += body;
    Addr second = first;
    if (i < s.size() && s.at(i) == QLatin1Char(':')) {
        ++i;
        int body2 = 0;
        if (!parseAddrBody(s, i, &second, &body2))
            return false;
        second.sheet = sheet;
        i += body2;
    }
    if (i != s.size())
        return false;
    if (a)
        *a = first;
    if (b)
        *b = second;
    return true;
}

QString a1(int rowZeroBased, int colZeroBased)
{
    return columnName(colZeroBased) + QString::number(rowZeroBased + 1);
}

QString formatAddr(const Addr &a)
{
    if (!a.valid)
        return {};
    QString out;
    if (!a.sheet.isEmpty()) {
        bool quote = false;
        for (const QChar c : a.sheet) {
            if (!c.isLetterOrNumber() && c != QLatin1Char('_') && c != QLatin1Char('.')) {
                quote = true;
                break;
            }
        }
        if (quote) {
            QString q = a.sheet;
            q.replace(QLatin1Char('\''), QStringLiteral("''"));
            out += QLatin1Char('\'') + q + QLatin1Char('\'');
        } else {
            out += a.sheet;
        }
        out += QLatin1Char('!');
    }
    if (a.absCol)
        out += QLatin1Char('$');
    out += columnName(a.col);
    if (a.absRow)
        out += QLatin1Char('$');
    out += QString::number(a.row + 1);
    return out;
}

QString formatRange(const Addr &a, const Addr &b)
{
    if (!b.valid || (a.row == b.row && a.col == b.col && a.absRow == b.absRow && a.absCol == b.absCol))
        return formatAddr(a);
    Addr right = b;
    right.sheet.clear();
    return formatAddr(a) + QLatin1Char(':') + formatAddr(right);
}

QString rewriteFormula(const QString &formula, const AddrMap &map)
{
    QString out;
    int i = 0;
    bool inStr = false;
    while (i < formula.size()) {
        const QChar ch = formula.at(i);
        if (inStr) {
            out += ch;
            if (ch == QLatin1Char('"')) {
                if (i + 1 < formula.size() && formula.at(i + 1) == QLatin1Char('"')) {
                    out += formula.at(i + 1);
                    i += 2;
                    continue;
                }
                inStr = false;
            }
            ++i;
            continue;
        }
        if (ch == QLatin1Char('"')) {
            inStr = true;
            out += ch;
            ++i;
            continue;
        }

        QString sheet;
        int prefix = 0;
        const bool hasSheet = parseSheetPrefix(formula, i, &sheet, &prefix);
        const int addrAt = hasSheet ? i + prefix : i;
        Addr first;
        int body = 0;
        if (parseAddrBody(formula, addrAt, &first, &body)) {
            first.sheet = sheet;
            int after = addrAt + body;
            while (after < formula.size() && formula.at(after).isSpace())
                ++after;
            if (after < formula.size() && formula.at(after) == QLatin1Char('(')) {
                out += formula.mid(i, after - i);
                i = after;
                continue;
            }
            Addr second;
            bool range = false;
            int after2 = addrAt + body;
            if (after2 < formula.size() && formula.at(after2) == QLatin1Char(':')) {
                int body2 = 0;
                if (parseAddrBody(formula, after2 + 1, &second, &body2)) {
                    second.sheet = sheet;
                    range = true;
                    after2 = after2 + 1 + body2;
                }
            }
            const Addr na = map(first);
            if (range) {
                const Addr nb = map(second);
                if (!na.valid || !nb.valid)
                    out += QStringLiteral("#REF!");
                else
                    out += formatRange(na, nb);
                i = after2;
            } else {
                out += na.valid ? formatAddr(na) : QStringLiteral("#REF!");
                i = addrAt + body;
            }
            continue;
        }
        out += ch;
        ++i;
    }
    return out;
}

QString adjustFormula(const QString &formula, int dRow, int dCol)
{
    if (!formula.startsWith(QLatin1Char('=')))
        return formula;
    return rewriteFormula(formula, [dRow, dCol](const Addr &a) {
        Addr n = a;
        if (!n.absRow)
            n.row += dRow;
        if (!n.absCol)
            n.col += dCol;
        if (n.row < 0 || n.col < 0)
            n.valid = false;
        return n;
    });
}

static bool sheetMatches(const Addr &a, const QString &formulaSheet, const QString &targetSheet)
{
    const QString refSheet = a.sheet.isEmpty() ? formulaSheet : a.sheet;
    return refSheet.compare(targetSheet, Qt::CaseInsensitive) == 0;
}

QString shiftFormulaInsertRow(const QString &formula, const QString &formulaSheet, const QString &targetSheet,
                              int at, int count)
{
    if (!formula.startsWith(QLatin1Char('=')))
        return formula;
    return rewriteFormula(formula, [&](const Addr &a) {
        Addr n = a;
        if (sheetMatches(a, formulaSheet, targetSheet) && n.row >= at)
            n.row += count;
        return n;
    });
}

QString shiftFormulaInsertCol(const QString &formula, const QString &formulaSheet, const QString &targetSheet,
                              int at, int count)
{
    if (!formula.startsWith(QLatin1Char('=')))
        return formula;
    return rewriteFormula(formula, [&](const Addr &a) {
        Addr n = a;
        if (sheetMatches(a, formulaSheet, targetSheet) && n.col >= at)
            n.col += count;
        return n;
    });
}

QString shiftFormulaDeleteRow(const QString &formula, const QString &formulaSheet, const QString &targetSheet,
                              int at, int count)
{
    if (!formula.startsWith(QLatin1Char('=')))
        return formula;
    return rewriteFormula(formula, [&](const Addr &a) {
        Addr n = a;
        if (!sheetMatches(a, formulaSheet, targetSheet))
            return n;
        if (n.row >= at && n.row < at + count) {
            n.valid = false;
            return n;
        }
        if (n.row >= at + count)
            n.row -= count;
        return n;
    });
}

QString shiftFormulaDeleteCol(const QString &formula, const QString &formulaSheet, const QString &targetSheet,
                              int at, int count)
{
    if (!formula.startsWith(QLatin1Char('=')))
        return formula;
    return rewriteFormula(formula, [&](const Addr &a) {
        Addr n = a;
        if (!sheetMatches(a, formulaSheet, targetSheet))
            return n;
        if (n.col >= at && n.col < at + count) {
            n.valid = false;
            return n;
        }
        if (n.col >= at + count)
            n.col -= count;
        return n;
    });
}

static Addr cycleAbsFlags(const Addr &a)
{
    Addr b = a;
    if (!b.absRow && !b.absCol) {
        b.absRow = true;
        b.absCol = true;
    } else if (b.absRow && b.absCol) {
        b.absRow = true;
        b.absCol = false;
    } else if (b.absRow && !b.absCol) {
        b.absRow = false;
        b.absCol = true;
    } else {
        b.absRow = false;
        b.absCol = false;
    }
    return b;
}

QString cycleReferenceAt(const QString &formula, int cursorPos, int *newCursorPos)
{
    if (!formula.startsWith(QLatin1Char('=')))
        return formula;
    int i = 1;
    while (i < formula.size()) {
        const QChar ch = formula.at(i);
        if (ch == QLatin1Char('"')) {
            ++i;
            while (i < formula.size()) {
                if (formula.at(i) == QLatin1Char('"')) {
                    ++i;
                    if (i < formula.size() && formula.at(i) == QLatin1Char('"'))
                        ++i;
                    else
                        break;
                } else {
                    ++i;
                }
            }
            continue;
        }
        const int start = i;
        QString sheet;
        int used = 0;
        if (parseSheetPrefix(formula, i, &sheet, &used))
            i += used;
        Addr a;
        int body = 0;
        if (!parseAddrBody(formula, i, &a, &body)) {
            ++i;
            continue;
        }
        a.sheet = sheet;
        int end = i + body;
        Addr b = a;
        if (end < formula.size() && formula.at(end) == QLatin1Char(':')) {
            int body2 = 0;
            if (parseAddrBody(formula, end + 1, &b, &body2)) {
                b.sheet = sheet;
                end = end + 1 + body2;
            }
        }
        if (cursorPos >= start && cursorPos <= end) {
            const Addr na = cycleAbsFlags(a);
            const Addr nb = cycleAbsFlags(b);
            const QString rep = (na.row != nb.row || na.col != nb.col) ? formatRange(na, nb) : formatAddr(na);
            const QString out = formula.left(start) + rep + formula.mid(end);
            if (newCursorPos)
                *newCursorPos = start + rep.size();
            return out;
        }
        i = end;
    }
    return formula;
}

}
