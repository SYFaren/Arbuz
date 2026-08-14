#include "exceldate.h"

#include <cmath>

// Adapted from QtExcel/QXlsx QXlsx/source/xlsxutility.cpp (MIT).
// Date-only path skips QXlsx daylight-saving hour tweaks (those follow the
// local TZ and would shift serials). Leap-1900 compensation is kept.

namespace ExcelDate {

double toSerial(const QDateTime &dt)
{
    const QDateTime epoch(QDate(1899, 12, 31), QTime(0, 0));
    double excelTime = epoch.msecsTo(dt) / (1000.0 * 60 * 60 * 24);
    if (excelTime > 59)
        excelTime += 1;
    return excelTime;
}

double toSerial(const QDate &d)
{
    return toSerial(QDateTime(d, QTime(0, 0)));
}

QDateTime toDateTime(double serial)
{
    if (serial > 60)
        serial -= 1;
    const QDateTime epoch(QDate(1899, 12, 31), QTime(0, 0));
    const qint64 msecs = qint64(serial * 1000.0 * 60 * 60 * 24 + (serial >= 0 ? 0.5 : -0.5));
    return epoch.addMSecs(msecs);
}

QDate toDate(double serial)
{
    return toDateTime(serial).date();
}

QString formatDate(double serial)
{
    const QDate d = toDate(serial);
    if (!d.isValid())
        return QString::number(serial, 'g', 12);
    return d.toString(Qt::ISODate);
}

QString formatDateTime(double serial)
{
    const QDateTime dt = toDateTime(serial);
    if (!dt.isValid())
        return QString::number(serial, 'g', 12);
    if (dt.time() == QTime(0, 0))
        return dt.date().toString(Qt::ISODate);
    return dt.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
}

}
