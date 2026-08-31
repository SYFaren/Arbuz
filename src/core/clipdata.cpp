#include "clipdata.h"
#include "cellref.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace ClipData {

static QJsonObject cellToJson(const CellData &c)
{
    QJsonObject o;
    o.insert(QStringLiteral("raw"), c.raw);
    if (c.bold)
        o.insert(QStringLiteral("bold"), true);
    if (c.italic)
        o.insert(QStringLiteral("italic"), true);
    if (c.foreground.isValid())
        o.insert(QStringLiteral("fg"), c.foreground.name(QColor::HexArgb));
    if (c.background.isValid())
        o.insert(QStringLiteral("bg"), c.background.name(QColor::HexArgb));
    if (c.hAlign)
        o.insert(QStringLiteral("hAlign"), c.hAlign);
    if (c.vAlign)
        o.insert(QStringLiteral("vAlign"), c.vAlign);
    if (c.wrap)
        o.insert(QStringLiteral("wrap"), true);
    if (c.numFmt)
        o.insert(QStringLiteral("numFmt"), c.numFmt);
    if (c.border)
        o.insert(QStringLiteral("border"), c.border);
    return o;
}

static CellData cellFromJson(const QJsonObject &o)
{
    CellData c;
    c.raw = o.value(QStringLiteral("raw")).toString();
    c.bold = o.value(QStringLiteral("bold")).toBool();
    c.italic = o.value(QStringLiteral("italic")).toBool();
    const QString fg = o.value(QStringLiteral("fg")).toString();
    if (!fg.isEmpty())
        c.foreground = QColor(fg);
    const QString bg = o.value(QStringLiteral("bg")).toString();
    if (!bg.isEmpty())
        c.background = QColor(bg);
    c.hAlign = o.value(QStringLiteral("hAlign")).toInt();
    c.vAlign = o.value(QStringLiteral("vAlign")).toInt();
    c.wrap = o.value(QStringLiteral("wrap")).toBool();
    c.numFmt = o.value(QStringLiteral("numFmt")).toInt();
    c.border = o.value(QStringLiteral("border")).toInt();
    return c;
}

QMimeData *mimeFromBlock(int rows, int cols, const QVector<CellData> &cells, const QString &tsvText)
{
    QJsonObject root;
    root.insert(QStringLiteral("rows"), rows);
    root.insert(QStringLiteral("cols"), cols);
    QJsonArray arr;
    for (const CellData &c : cells)
        arr.append(cellToJson(c));
    root.insert(QStringLiteral("cells"), arr);

    auto *mime = new QMimeData;
    mime->setData(MimeType, QJsonDocument(root).toJson(QJsonDocument::Compact));
    mime->setText(tsvText);
    return mime;
}

bool blockFromMime(const QMimeData *mime, int *rows, int *cols, QVector<CellData> *cells, QString *tsvText)
{
    if (!mime)
        return false;
    if (tsvText)
        *tsvText = mime->text();
    if (rows)
        *rows = 0;
    if (cols)
        *cols = 0;
    if (cells)
        cells->clear();

    const QByteArray payload = mime->data(MimeType);
    if (payload.isEmpty())
        return tsvText && !mime->text().isEmpty();

    const QJsonObject root = QJsonDocument::fromJson(payload).object();
    const int r = root.value(QStringLiteral("rows")).toInt();
    const int c = root.value(QStringLiteral("cols")).toInt();
    if (r <= 0 || c <= 0 || !cells)
        return false;
    const QJsonArray arr = root.value(QStringLiteral("cells")).toArray();
    if (arr.size() != r * c)
        return false;
    cells->reserve(arr.size());
    for (const QJsonValue &v : arr)
        cells->append(cellFromJson(v.toObject()));
    if (rows)
        *rows = r;
    if (cols)
        *cols = c;
    return true;
}

CellData cellForPaste(const CellData &src, int dRow, int dCol)
{
    CellData out = src;
    if (out.raw.startsWith(QLatin1Char('=')))
        out.raw = CellRef::adjustFormula(out.raw, dRow, dCol);
    return out;
}

} // namespace ClipData
