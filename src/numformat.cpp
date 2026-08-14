#include "numformat.h"
#include "exceldate.h"

#include <QTime>
#include <cmath>

namespace NumFormat {

bool isDateTime(int id)
{
    return (id >= 14 && id <= 22) || (id >= 45 && id <= 47);
}

bool isPercent(int id)
{
    return id == Percent || id == Percent2;
}

static QString withThousands(const QString &intPart)
{
    QString s = intPart;
    const int start = s.startsWith(QLatin1Char('-')) ? 1 : 0;
    for (int i = s.size() - 3; i > start; i -= 3)
        s.insert(i, QLatin1Char(','));
    return s;
}

static QString groupedFixed(double n, int decimals)
{
    const QString s = QString::number(n, 'f', decimals);
    const int dot = s.indexOf(QLatin1Char('.'));
    if (dot < 0)
        return withThousands(s);
    return withThousands(s.left(dot)) + s.mid(dot);
}

QString format(double n, int id)
{
    switch (id) {
    case Integer:
        return QString::number(qint64(std::llround(n)));
    case Number2:
        return QString::number(n, 'f', 2);
    case Thousands:
        return withThousands(QString::number(qint64(std::llround(n))));
    case Thousands2:
        return groupedFixed(n, 2);
    case Percent:
        return QString::number(qint64(std::llround(n * 100.0))) + QLatin1Char('%');
    case Percent2:
        return QString::number(n * 100.0, 'f', 2) + QLatin1Char('%');
    case Scientific:
        return QString::asprintf("%.2E", n);
    case Date:
        return ExcelDate::formatDate(n);
    case 15:
    case 16:
    case 17:
        return ExcelDate::formatDate(n);
    case Time:
    case 18:
    case 19:
    case 21: {
        const QTime t = ExcelDate::toDateTime(n).time();
        if (!t.isValid())
            return QString::number(n, 'g', 12);
        if (id == 21 || id == 19)
            return t.toString(QStringLiteral("HH:mm:ss"));
        return t.toString(QStringLiteral("HH:mm"));
    }
    case DateTime:
        return ExcelDate::formatDateTime(n);
    case General:
    default:
        if (std::floor(n) == n && std::fabs(n) < 1e12)
            return QString::number(qint64(n));
        return QString::number(n, 'g', 12);
    }
}

}
