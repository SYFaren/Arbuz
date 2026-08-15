#ifndef ARBUZ_FILEIO_H
#define ARBUZ_FILEIO_H

#include <QString>

class Workbook;

class FileIo
{
public:
    static bool load(Workbook *wb, const QString &path, QString *error);
    static bool save(Workbook *wb, const QString &path, QString *error);

private:
    static bool loadXlsx(Workbook *wb, const QString &path, QString *error);
    static bool saveXlsx(Workbook *wb, const QString &path, QString *error);
    static bool loadCsv(Workbook *wb, const QString &path, QString *error);
    static bool saveCsv(Workbook *wb, const QString &path, QString *error);
};

#endif
