#ifndef ARBUZ_CLIPDATA_H
#define ARBUZ_CLIPDATA_H

#include "workbook.h"

#include <QMimeData>
#include <QVector>

namespace ClipData {

constexpr const char *MimeType = "application/x-arbuz-cells";

QMimeData *mimeFromBlock(int rows, int cols, const QVector<CellData> &cells, const QString &tsvText);
bool blockFromMime(const QMimeData *mime, int *rows, int *cols, QVector<CellData> *cells, QString *tsvText);
CellData cellForPaste(const CellData &src, int dRow, int dCol);

} // namespace ClipData

#endif
