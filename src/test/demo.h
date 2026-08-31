#ifndef ARBUZ_DEMO_H
#define ARBUZ_DEMO_H

#include <QString>

class Workbook;

void buildDemoWorkbook(Workbook *wb);
int writeDemoWorkbook(const QString &path);

#endif
