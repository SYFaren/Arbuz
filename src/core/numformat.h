#ifndef ARBUZ_NUMFORMAT_H
#define ARBUZ_NUMFORMAT_H

#include <QString>

// Excel built-in numFmtId (ECMA-376 18.8.30), same IDs QXlsx writes to styles.xml.
namespace NumFormat {
enum Id {
    General = 0,
    Integer = 1,
    Number2 = 2,
    Thousands = 3,
    Thousands2 = 4,
    Percent = 9,
    Percent2 = 10,
    Scientific = 11,
    Date = 14,
    DateTime = 22,
    Time = 20
};

QString format(double n, int id);
bool isDateTime(int id);
bool isPercent(int id);

// 1.5, 1,5, 1 234,56, 1,234.50, 25%. Last comma or dot is the decimal mark.
bool parse(const QString &s, double *n = nullptr);
}

#endif
