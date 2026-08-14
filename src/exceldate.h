#ifndef ARBUZ_EXCELDATE_H
#define ARBUZ_EXCELDATE_H

#include <QDate>
#include <QDateTime>

// Excel 1900 date system, after QXlsx xlsxutility.cpp (MIT):
// epoch 1899-12-31 plus the fake 1900 leap day when serial > 59.
namespace ExcelDate {
double toSerial(const QDate &d);
double toSerial(const QDateTime &dt);
QDate toDate(double serial);
QDateTime toDateTime(double serial);
QString formatDate(double serial);
QString formatDateTime(double serial);
}

#endif
