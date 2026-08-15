#include "fileio.h"
#include "cellref.h"
#include "numformat.h"
#include "workbook.h"

#include <QBuffer>
#include <QColor>
#include <QFile>
#include <QHash>
#include <QMap>
#include <QPair>
#include <QTextStream>
#include <QUndoStack>
#include <QVector>
#include <QXmlStreamReader>
#include <QtEndian>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <zlib.h>

namespace {

quint32 crc32Of(const QByteArray &data)
{
    static quint32 table[256];
    static bool init = false;
    if (!init) {
        for (quint32 i = 0; i < 256; ++i) {
            quint32 c = i;
            for (int j = 0; j < 8; ++j)
                c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            table[i] = c;
        }
        init = true;
    }
    quint32 crc = 0xFFFFFFFFu;
    for (unsigned char b : data)
        crc = table[(crc ^ b) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

struct ZipItem {
    QByteArray name;
    QByteArray data;
    quint32 crc = 0;
    quint32 offset = 0;
};

class StoreZip
{
public:
    void add(const QString &name, const QByteArray &data)
    {
        ZipItem it;
        it.name = name.toUtf8();
        it.data = data;
        it.crc = crc32Of(data);
        m_items.append(it);
    }

    QByteArray finish()
    {
        QByteArray out;
        QBuffer buf(&out);
        buf.open(QIODevice::WriteOnly);
        for (ZipItem &it : m_items) {
            it.offset = quint32(buf.pos());
            writeU32(buf, 0x04034b50);
            writeU16(buf, 20);
            writeU16(buf, 0);
            writeU16(buf, 0); // store
            writeU16(buf, 0);
            writeU16(buf, 0);
            writeU32(buf, it.crc);
            writeU32(buf, quint32(it.data.size()));
            writeU32(buf, quint32(it.data.size()));
            writeU16(buf, quint16(it.name.size()));
            writeU16(buf, 0);
            buf.write(it.name);
            buf.write(it.data);
        }
        const quint32 cdStart = quint32(buf.pos());
        for (const ZipItem &it : m_items) {
            writeU32(buf, 0x02014b50);
            writeU16(buf, 20);
            writeU16(buf, 20);
            writeU16(buf, 0);
            writeU16(buf, 0);
            writeU16(buf, 0);
            writeU16(buf, 0);
            writeU32(buf, it.crc);
            writeU32(buf, quint32(it.data.size()));
            writeU32(buf, quint32(it.data.size()));
            writeU16(buf, quint16(it.name.size()));
            writeU16(buf, 0);
            writeU16(buf, 0);
            writeU16(buf, 0);
            writeU16(buf, 0);
            writeU32(buf, 0);
            writeU32(buf, it.offset);
            buf.write(it.name);
        }
        const quint32 cdSize = quint32(buf.pos()) - cdStart;
        writeU32(buf, 0x06054b50);
        writeU16(buf, 0);
        writeU16(buf, 0);
        writeU16(buf, quint16(m_items.size()));
        writeU16(buf, quint16(m_items.size()));
        writeU32(buf, cdSize);
        writeU32(buf, cdStart);
        writeU16(buf, 0);
        buf.close();
        return out;
    }

private:
    QVector<ZipItem> m_items;
    static void writeU16(QBuffer &b, quint16 v)
    {
        const quint16 le = qToLittleEndian(v);
        b.write(reinterpret_cast<const char *>(&le), 2);
    }
    static void writeU32(QBuffer &b, quint32 v)
    {
        const quint32 le = qToLittleEndian(v);
        b.write(reinterpret_cast<const char *>(&le), 4);
    }
};

QByteArray xmlEscape(const QString &s)
{
    QString e = s;
    e.replace(QLatin1Char('&'), QStringLiteral("&amp;"));
    e.replace(QLatin1Char('<'), QStringLiteral("&lt;"));
    e.replace(QLatin1Char('>'), QStringLiteral("&gt;"));
    e.replace(QLatin1Char('"'), QStringLiteral("&quot;"));
    return e.toUtf8();
}

static QByteArray rgbHex(const QColor &c)
{
    if (!c.isValid())
        return QByteArray("000000");
    return QByteArray::number((c.red() << 16) | (c.green() << 8) | c.blue(), 16).rightJustified(6, '0').toUpper();
}

struct XfStyle {
    bool bold = false;
    bool italic = false;
    QColor fg;
    QColor bg;
    int hAlign = 0;
    int vAlign = 0;
    bool wrap = false;
    int numFmt = 0;
    int border = 0;

    bool operator==(const XfStyle &o) const
    {
        return bold == o.bold && italic == o.italic && fg == o.fg && bg == o.bg && hAlign == o.hAlign
            && vAlign == o.vAlign && wrap == o.wrap && numFmt == o.numFmt && border == o.border;
    }
};

static XfStyle xfFromCell(const CellData &d)
{
    XfStyle s;
    s.bold = d.bold;
    s.italic = d.italic;
    s.fg = d.foreground;
    s.bg = d.background;
    s.hAlign = d.hAlign;
    s.vAlign = d.vAlign;
    s.wrap = d.wrap;
    s.numFmt = d.numFmt;
    s.border = d.border;
    return s;
}

static int xfIndex(QVector<XfStyle> *book, const CellData &d)
{
    const XfStyle s = xfFromCell(d);
    if (s == XfStyle())
        return 0;
    for (int i = 0; i < book->size(); ++i) {
        if (book->at(i) == s)
            return i;
    }
    book->append(s);
    return book->size() - 1;
}

static QByteArray stylesXml(const QVector<XfStyle> &book)
{
    QByteArray fonts = "<fonts count=\"" + QByteArray::number(book.size()) + "\">";
    QByteArray fills = "<fills count=\"" + QByteArray::number(book.size() + 2) + "\">"
                       "<fill><patternFill patternType=\"none\"/></fill>"
                       "<fill><patternFill patternType=\"gray125\"/></fill>";
    QByteArray borders = "<borders count=\"" + QByteArray::number(book.size()) + "\">";
    QByteArray xfs = "<cellXfs count=\"" + QByteArray::number(book.size()) + "\">";
    for (int i = 0; i < book.size(); ++i) {
        const XfStyle &s = book.at(i);
        fonts += "<font>";
        if (s.bold)
            fonts += "<b/>";
        if (s.italic)
            fonts += "<i/>";
        if (s.fg.isValid())
            fonts += "<color rgb=\"FF" + rgbHex(s.fg) + "\"/>";
        fonts += "</font>";
        fills += "<fill><patternFill patternType=\"";
        fills += s.bg.isValid() ? "solid" : "none";
        fills += "\">";
        if (s.bg.isValid())
            fills += "<fgColor rgb=\"FF" + rgbHex(s.bg) + "\"/>";
        fills += "</patternFill></fill>";
        borders += "<border>";
        const char *sides[] = {"left", "right", "top", "bottom"};
        for (int b = 0; b < 4; ++b) {
            borders += "<";
            borders += sides[b];
            if (s.border & (1 << b))
                borders += " style=\"thin\"";
            borders += "/>";
        }
        borders += "</border>";
        xfs += "<xf numFmtId=\"" + QByteArray::number(s.numFmt) + "\" fontId=\"" + QByteArray::number(i)
               + "\" fillId=\"" + QByteArray::number(s.bg.isValid() ? i + 2 : 0) + "\" borderId=\""
               + QByteArray::number(i) + "\" applyFont=\"1\" applyFill=\"1\" applyBorder=\"1\" applyAlignment=\"1\" "
                 "applyNumberFormat=\"1\"><alignment";
        if (s.hAlign == 1)
            xfs += " horizontal=\"left\"";
        else if (s.hAlign == 2)
            xfs += " horizontal=\"center\"";
        else if (s.hAlign == 3)
            xfs += " horizontal=\"right\"";
        if (s.vAlign == 1)
            xfs += " vertical=\"top\"";
        else if (s.vAlign == 2)
            xfs += " vertical=\"bottom\"";
        if (s.wrap)
            xfs += " wrapText=\"1\"";
        xfs += "/></xf>";
    }
    fonts += "</fonts>";
    fills += "</fills>";
    borders += "</borders>";
    xfs += "</cellXfs>";
    return QByteArray("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
                      "<styleSheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">")
        + fonts + fills + borders + xfs + "</styleSheet>";
}

static CellData styleToCell(const XfStyle &s)
{
    CellData d;
    d.bold = s.bold;
    d.italic = s.italic;
    d.foreground = s.fg;
    d.background = s.bg;
    d.hAlign = s.hAlign;
    d.vAlign = s.vAlign;
    d.wrap = s.wrap;
    d.numFmt = s.numFmt;
    d.border = s.border;
    return d;
}

static QVector<XfStyle> parseStylesXml(const QByteArray &xml)
{
    QVector<XfStyle> fonts, fills, borders, xfs;
    QXmlStreamReader r(xml);
    XfStyle cur;
    QString ctx;
    while (!r.atEnd()) {
        r.readNext();
        if (r.isStartElement()) {
            const QString n = r.name().toString();
            if (n == QLatin1String("font")) {
                cur = XfStyle();
                ctx = n;
            } else if (n == QLatin1String("b") && ctx == QLatin1String("font"))
                cur.bold = true;
            else if (n == QLatin1String("i") && ctx == QLatin1String("font"))
                cur.italic = true;
            else if (n == QLatin1String("color") && ctx == QLatin1String("font")) {
                const QString rgb = r.attributes().value(QStringLiteral("rgb")).toString();
                if (rgb.size() >= 6)
                    cur.fg = QColor(QLatin1Char('#') + rgb.right(6));
            } else if (n == QLatin1String("fill")) {
                cur = XfStyle();
                ctx = n;
            } else if (n == QLatin1String("fgColor") && ctx == QLatin1String("fill")) {
                const QString rgb = r.attributes().value(QStringLiteral("rgb")).toString();
                if (rgb.size() >= 6)
                    cur.bg = QColor(QLatin1Char('#') + rgb.right(6));
            } else if (n == QLatin1String("border")) {
                cur = XfStyle();
                ctx = n;
            } else if (ctx == QLatin1String("border")
                       && (n == QLatin1String("left") || n == QLatin1String("right") || n == QLatin1String("top")
                           || n == QLatin1String("bottom"))) {
                if (!r.attributes().value(QStringLiteral("style")).isEmpty()) {
                    int bit = 0;
                    if (n == QLatin1String("left"))
                        bit = 1;
                    else if (n == QLatin1String("right"))
                        bit = 2;
                    else if (n == QLatin1String("top"))
                        bit = 4;
                    else
                        bit = 8;
                    cur.border |= bit;
                }
            } else if (n == QLatin1String("xf")) {
                XfStyle xf;
                xf.numFmt = r.attributes().value(QStringLiteral("numFmtId")).toInt();
                const int fontId = r.attributes().value(QStringLiteral("fontId")).toInt();
                const int fillId = r.attributes().value(QStringLiteral("fillId")).toInt();
                const int borderId = r.attributes().value(QStringLiteral("borderId")).toInt();
                if (fontId >= 0 && fontId < fonts.size()) {
                    xf.bold = fonts.at(fontId).bold;
                    xf.italic = fonts.at(fontId).italic;
                    xf.fg = fonts.at(fontId).fg;
                }
                if (fillId >= 0 && fillId < fills.size())
                    xf.bg = fills.at(fillId).bg;
                if (borderId >= 0 && borderId < borders.size())
                    xf.border = borders.at(borderId).border;
                ctx = n;
                xfs.append(xf);
            } else if (n == QLatin1String("alignment") && ctx == QLatin1String("xf") && !xfs.isEmpty()) {
                const QString h = r.attributes().value(QStringLiteral("horizontal")).toString();
                const QString v = r.attributes().value(QStringLiteral("vertical")).toString();
                if (h == QLatin1String("left"))
                    xfs.last().hAlign = 1;
                else if (h == QLatin1String("center"))
                    xfs.last().hAlign = 2;
                else if (h == QLatin1String("right"))
                    xfs.last().hAlign = 3;
                if (v == QLatin1String("top"))
                    xfs.last().vAlign = 1;
                else if (v == QLatin1String("bottom"))
                    xfs.last().vAlign = 2;
                if (r.attributes().value(QStringLiteral("wrapText")).toString() == QLatin1String("1"))
                    xfs.last().wrap = true;
            }
        } else if (r.isEndElement()) {
            const QString n = r.name().toString();
            if (n == QLatin1String("font"))
                fonts.append(cur);
            else if (n == QLatin1String("fill"))
                fills.append(cur);
            else if (n == QLatin1String("border"))
                borders.append(cur);
            if (n == ctx)
                ctx.clear();
        }
    }
    if (xfs.isEmpty())
        xfs.append(XfStyle());
    return xfs;
}

QByteArray sheetXml(const Worksheet &ws, const QVector<int> &xfOfCell, const QHash<quint64, int> &xfMap)
{
    QByteArray xml = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
                     "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">";
    if (ws.freezeRows > 0 || ws.freezeCols > 0) {
        xml += "<sheetViews><sheetView workbookViewId=\"0\"><pane ySplit=\"";
        xml += QByteArray::number(ws.freezeRows);
        xml += "\" xSplit=\"";
        xml += QByteArray::number(ws.freezeCols);
        xml += "\" topLeftCell=\"";
        xml += CellRef::a1(ws.freezeRows, ws.freezeCols).toLatin1();
        xml += "\" state=\"frozen\"/></sheetView></sheetViews>";
    }
    if (!ws.columnWidths.isEmpty()) {
        xml += "<cols>";
        QList<int> keys = ws.columnWidths.keys();
        std::sort(keys.begin(), keys.end());
        for (int c : keys) {
            const double w = ws.columnWidths.value(c) / 7.0;
            xml += "<col min=\"" + QByteArray::number(c + 1) + "\" max=\"" + QByteArray::number(c + 1)
                   + "\" width=\"" + QByteArray::number(w, 'f', 4) + "\" customWidth=\"1\"/>";
        }
        xml += "</cols>";
    }
    xml += "<sheetData>";
    QMap<int, QList<QPair<int, CellData>>> rows;
    for (auto it = ws.cells.cbegin(); it != ws.cells.cend(); ++it) {
        const int row = int(it.key() >> 32);
        const int col = int(it.key() & 0xffffffffu);
        rows[row].append(qMakePair(col, it.value()));
    }
    for (auto rit = rows.begin(); rit != rows.end(); ++rit) {
        xml += "<row r=\"" + QByteArray::number(rit.key() + 1) + "\">";
        for (const auto &cell : rit.value()) {
            const int col = cell.first;
            const CellData &d = cell.second;
            const QByteArray ref = CellRef::a1(rit.key(), col).toLatin1();
            const int xf = xfMap.value(Worksheet::key(rit.key(), col), 0);
            QByteArray sAttr;
            if (xf > 0)
                sAttr = " s=\"" + QByteArray::number(xf) + "\"";
            if (d.raw.startsWith(QLatin1Char('='))) {
                xml += "<c r=\"" + ref + "\"" + sAttr + "><f>" + xmlEscape(d.raw.mid(1)) + "</f></c>";
            } else {
                double n = 0;
                if (NumFormat::parse(d.raw, &n)) {
                    xml += "<c r=\"" + ref + "\"" + sAttr + " t=\"n\"><v>" + QByteArray::number(n, 'g', 15)
                           + "</v></c>";
                } else {
                    xml += "<c r=\"" + ref + "\"" + sAttr + " t=\"inlineStr\"><is><t>" + xmlEscape(d.raw)
                           + "</t></is></c>";
                }
            }
        }
        xml += "</row>";
    }
    xml += "</sheetData>";
    if (!ws.merges.isEmpty()) {
        xml += "<mergeCells count=\"" + QByteArray::number(ws.merges.size()) + "\">";
        for (const MergeRange &m : ws.merges) {
            xml += "<mergeCell ref=\"" + CellRef::a1(m.r1, m.c1).toLatin1() + ":"
                   + CellRef::a1(m.r2, m.c2).toLatin1() + "\"/>";
        }
        xml += "</mergeCells>";
    }
    xml += "</worksheet>";
    Q_UNUSED(xfOfCell);
    return xml;
}

static quint16 ru16(const char *p)
{
    quint16 v;
    std::memcpy(&v, p, 2);
    return qFromLittleEndian(v);
}

static quint32 ru32(const char *p)
{
    quint32 v;
    std::memcpy(&v, p, 4);
    return qFromLittleEndian(v);
}

static QByteArray inflateRaw(const QByteArray &src, quint32 outHint)
{
    QByteArray out;
    out.resize(int(outHint ? outHint : quint32(src.size() * 4 + 64)));
    z_stream st;
    std::memset(&st, 0, sizeof(st));
    st.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(src.data()));
    st.avail_in = uInt(src.size());
    if (inflateInit2(&st, -MAX_WBITS) != Z_OK)
        return {};
    int ret = Z_OK;
    while (ret != Z_STREAM_END) {
        if (st.total_out >= uLong(out.size()))
            out.resize(out.size() * 2 + 1024);
        st.next_out = reinterpret_cast<Bytef *>(out.data() + st.total_out);
        st.avail_out = uInt(out.size() - int(st.total_out));
        ret = inflate(&st, Z_NO_FLUSH);
        if (ret != Z_OK && ret != Z_STREAM_END) {
            inflateEnd(&st);
            return {};
        }
    }
    out.resize(int(st.total_out));
    inflateEnd(&st);
    return out;
}

static QHash<QString, QByteArray> unzipAll(const QByteArray &zip)
{
    QHash<QString, QByteArray> files;
    if (zip.size() < 22)
        return files;
    int eocd = -1;
    for (int i = zip.size() - 22; i >= 0 && i >= zip.size() - 22 - 65535; --i) {
        if (ru32(zip.constData() + i) == 0x06054b50u) {
            eocd = i;
            break;
        }
    }
    if (eocd < 0)
        return files;
    const quint16 nrec = ru16(zip.constData() + eocd + 10);
    quint32 cdOff = ru32(zip.constData() + eocd + 16);
    for (quint16 i = 0; i < nrec; ++i) {
        if (cdOff + 46 > quint32(zip.size()) || ru32(zip.constData() + int(cdOff)) != 0x02014b50u)
            break;
        const quint16 method = ru16(zip.constData() + int(cdOff) + 10);
        const quint32 comp = ru32(zip.constData() + int(cdOff) + 20);
        const quint32 uncomp = ru32(zip.constData() + int(cdOff) + 24);
        const quint16 namelen = ru16(zip.constData() + int(cdOff) + 28);
        const quint16 extra = ru16(zip.constData() + int(cdOff) + 30);
        const quint16 comment = ru16(zip.constData() + int(cdOff) + 32);
        const quint32 localOff = ru32(zip.constData() + int(cdOff) + 42);
        const QString name = QString::fromUtf8(zip.constData() + int(cdOff) + 46, namelen);
        const quint32 local = localOff;
        if (local + 30 > quint32(zip.size()))
            break;
        const quint16 lname = ru16(zip.constData() + int(local) + 26);
        const quint16 lextra = ru16(zip.constData() + int(local) + 28);
        const int dataOff = int(local) + 30 + lname + lextra;
        const QByteArray payload = zip.mid(dataOff, int(comp));
        QByteArray data;
        if (method == 0)
            data = payload;
        else if (method == 8)
            data = inflateRaw(payload, uncomp);
        if (!name.endsWith(QLatin1Char('/')))
            files.insert(name, data);
        cdOff += 46u + namelen + extra + comment;
    }
    return files;
}

static QHash<QString, QString> parseRels(const QByteArray &xml)
{
    QHash<QString, QString> map;
    QXmlStreamReader r(xml);
    while (!r.atEnd()) {
        r.readNext();
        if (!r.isStartElement() || r.name() != QLatin1String("Relationship"))
            continue;
        map.insert(r.attributes().value(QStringLiteral("Id")).toString(),
                   r.attributes().value(QStringLiteral("Target")).toString());
    }
    return map;
}

static void parseSheetXml(const QByteArray &xml, Worksheet *ws, const QStringList &shared,
                          const QVector<XfStyle> &xfs)
{
    QXmlStreamReader r(xml);
    while (!r.atEnd()) {
        r.readNext();
        if (!r.isStartElement())
            continue;
        if (r.name() == QLatin1String("pane")) {
            ws->freezeRows = r.attributes().value(QStringLiteral("ySplit")).toInt();
            ws->freezeCols = r.attributes().value(QStringLiteral("xSplit")).toInt();
            continue;
        }
        if (r.name() == QLatin1String("col")) {
            const int min = r.attributes().value(QStringLiteral("min")).toInt() - 1;
            const int max = r.attributes().value(QStringLiteral("max")).toInt() - 1;
            const double w = r.attributes().value(QStringLiteral("width")).toDouble();
            const int px = int(w * 7.0 + 0.5);
            for (int c = min; c <= max; ++c)
                ws->columnWidths.insert(c, px);
            continue;
        }
        if (r.name() == QLatin1String("mergeCell")) {
            const QString ref = r.attributes().value(QStringLiteral("ref")).toString();
            int r1 = 0, c1 = 0, r2 = 0, c2 = 0;
            if (CellRef::parseA1Range(ref, &r1, &c1, &r2, &c2))
                ws->merges.append(MergeRange{r1, c1, r2, c2});
            continue;
        }
        if (r.name() != QLatin1String("c"))
            continue;
        const QString ref = r.attributes().value(QStringLiteral("r")).toString();
        const QString t = r.attributes().value(QStringLiteral("t")).toString();
        const int s = r.attributes().value(QStringLiteral("s")).toInt();
        QString formula;
        QString value;
        while (!(r.isEndElement() && r.name() == QLatin1String("c")) && !r.atEnd()) {
            r.readNext();
            if (r.isStartElement() && r.name() == QLatin1String("f"))
                formula = r.readElementText();
            else if (r.isStartElement() && r.name() == QLatin1String("v"))
                value = r.readElementText();
            else if (r.isStartElement() && r.name() == QLatin1String("t"))
                value = r.readElementText();
        }
        int row = 0, col = 0;
        if (!CellRef::parseA1(ref, &row, &col))
            continue;
        CellData d;
        if (s >= 0 && s < xfs.size())
            d = styleToCell(xfs.at(s));
        if (!formula.isEmpty())
            d.raw = QLatin1Char('=') + formula;
        else if (t == QLatin1String("s"))
            d.raw = shared.value(value.toInt());
        else if (t == QLatin1String("b"))
            d.raw = (value == QLatin1String("1") || value.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0)
                ? QStringLiteral("TRUE")
                : QStringLiteral("FALSE");
        else
            d.raw = value;
        if (!d.raw.isEmpty() || !d.styleIsDefault())
            ws->setCell(row, col, d);
    }
}

static QStringList parseSharedStrings(const QByteArray &xml)
{
    QStringList out;
    QXmlStreamReader r(xml);
    while (!r.atEnd()) {
        r.readNext();
        if (!r.isStartElement() || r.name() != QLatin1String("si"))
            continue;
        QString acc;
        while (!(r.isEndElement() && r.name() == QLatin1String("si")) && !r.atEnd()) {
            r.readNext();
            if (r.isStartElement() && r.name() == QLatin1String("t"))
                acc += r.readElementText();
        }
        out.append(acc);
    }
    return out;
}

} // namespace

bool FileIo::load(Workbook *wb, const QString &path, QString *error)
{
    if (path.endsWith(QLatin1String(".csv"), Qt::CaseInsensitive))
        return loadCsv(wb, path, error);
    return loadXlsx(wb, path, error);
}

bool FileIo::save(Workbook *wb, const QString &path, QString *error)
{
    if (path.endsWith(QLatin1String(".csv"), Qt::CaseInsensitive))
        return saveCsv(wb, path, error);
    return saveXlsx(wb, path, error);
}

bool FileIo::loadXlsx(Workbook *wb, const QString &path, QString *error)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("Не удалось открыть xlsx");
        return false;
    }
    const QHash<QString, QByteArray> files = unzipAll(f.readAll());
    const QByteArray wbXml = files.value(QStringLiteral("xl/workbook.xml"));
    if (wbXml.isEmpty()) {
        if (error)
            *error = QStringLiteral("Некорректный xlsx");
        return false;
    }
    const auto rels = parseRels(files.value(QStringLiteral("xl/_rels/workbook.xml.rels")));
    const QStringList shared = parseSharedStrings(files.value(QStringLiteral("xl/sharedStrings.xml")));
    const QVector<XfStyle> xfs = parseStylesXml(files.value(QStringLiteral("xl/styles.xml")));

    QVector<Worksheet> loaded;
    QXmlStreamReader reader(wbXml);
    while (!reader.atEnd()) {
        reader.readNext();
        if (!reader.isStartElement() || reader.name() != QLatin1String("sheet"))
            continue;
        QString rid;
        for (const QXmlStreamAttribute &a : reader.attributes()) {
            if (a.name() == QLatin1String("name"))
                /* name already read */;
            if (a.name() == QLatin1String("id"))
                rid = a.value().toString();
        }
        const QString name = reader.attributes().value(QStringLiteral("name")).toString();
        QString target = rels.value(rid);
        if (target.startsWith(QLatin1String("/")))
            target = target.mid(1);
        else if (!target.startsWith(QLatin1String("xl/")))
            target = QStringLiteral("xl/") + target;
        Worksheet ws;
        ws.name = name.isEmpty() ? QStringLiteral("Лист%1").arg(loaded.size() + 1) : name;
        parseSheetXml(files.value(target), &ws, shared, xfs);
        loaded.append(ws);
    }

    if (loaded.isEmpty()) {
        wb->resetToEmpty();
        return true;
    }
    wb->resetToEmpty();
    wb->setUndoEnabled(false);
    wb->blockSignals(true);
    wb->sheet(0) = loaded.first();
    for (int i = 1; i < loaded.size(); ++i) {
        const int idx = wb->addSheet(loaded.at(i).name);
        wb->sheet(idx) = loaded.at(i);
    }
    wb->blockSignals(false);
    wb->setUndoEnabled(true);
    if (wb->undoStack())
        wb->undoStack()->clear();
    wb->recalculate();
    emit wb->structureChanged();
    return true;
}

bool FileIo::saveXlsx(Workbook *wb, const QString &path, QString *error)
{
    StoreZip zip;

    QByteArray workbook = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
                          "<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
                          "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">"
                          "<sheets>";
    QByteArray wbRels = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
                        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">";
    QByteArray overrides;

    QVector<XfStyle> book;
    book.append(XfStyle());
    QVector<QHash<quint64, int>> xfMaps;
    xfMaps.resize(wb->sheetCount());
    for (int s = 0; s < wb->sheetCount(); ++s) {
        const Worksheet &ws = wb->sheet(s);
        for (auto it = ws.cells.cbegin(); it != ws.cells.cend(); ++it)
            xfMaps[s].insert(it.key(), xfIndex(&book, it.value()));
    }

    for (int s = 0; s < wb->sheetCount(); ++s) {
        const Worksheet &ws = wb->sheet(s);
        const int n = s + 1;
        const QByteArray name = xmlEscape(ws.name);
        workbook += "<sheet name=\"" + name + "\" sheetId=\"" + QByteArray::number(n)
                    + "\" r:id=\"rId" + QByteArray::number(n) + "\"/>";
        wbRels += "<Relationship Id=\"rId" + QByteArray::number(n)
                  + "\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" "
                    "Target=\"worksheets/sheet"
                  + QByteArray::number(n) + ".xml\"/>";
        overrides += "<Override PartName=\"/xl/worksheets/sheet" + QByteArray::number(n)
                     + ".xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>";
        zip.add(QStringLiteral("xl/worksheets/sheet%1.xml").arg(n), sheetXml(ws, {}, xfMaps.at(s)));
    }
    workbook += "</sheets></workbook>";
    wbRels += "<Relationship Id=\"rIdStyles\" "
              "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles\" "
              "Target=\"styles.xml\"/></Relationships>";

    QByteArray ctypes = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
                        "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
                        "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
                        "<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
                        "<Override PartName=\"/xl/workbook.xml\" "
                        "ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>"
                        "<Override PartName=\"/xl/styles.xml\" "
                        "ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml\"/>"
                        + overrides + "</Types>";
    zip.add(QStringLiteral("[Content_Types].xml"), ctypes);
    zip.add(QStringLiteral("xl/workbook.xml"), workbook);
    zip.add(QStringLiteral("xl/styles.xml"), stylesXml(book));
    zip.add(QStringLiteral("xl/_rels/workbook.xml.rels"), wbRels);
    zip.add(QStringLiteral("_rels/.rels"),
            QByteArray("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
                       "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
                       "<Relationship Id=\"rId1\" "
                       "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" "
                       "Target=\"xl/workbook.xml\"/></Relationships>"));

    const QByteArray packed = zip.finish();
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        if (error)
            *error = QStringLiteral("Не удалось сохранить xlsx");
        return false;
    }
    f.write(packed);
    return true;
}

static QStringList parseCsvRecord(QTextStream &in, QChar sep, bool *eof)
{
    QStringList fields;
    QString field;
    bool quoted = false;
    *eof = false;
    while (!in.atEnd()) {
        const QString chunk = in.readLine();
        if (quoted)
            field += QLatin1Char('\n');
        for (int i = 0; i < chunk.size(); ++i) {
            const QChar ch = chunk.at(i);
            if (quoted) {
                if (ch == QLatin1Char('"')) {
                    if (i + 1 < chunk.size() && chunk.at(i + 1) == QLatin1Char('"')) {
                        field += QLatin1Char('"');
                        ++i;
                    } else {
                        quoted = false;
                    }
                } else {
                    field += ch;
                }
            } else if (ch == QLatin1Char('"') && field.isEmpty()) {
                quoted = true;
            } else if (ch == sep) {
                fields.append(field);
                field.clear();
            } else {
                field += ch;
            }
        }
        if (!quoted)
            break;
    }
    fields.append(field);
    *eof = in.atEnd();
    return fields;
}

static QChar detectCsvSep(const QString &line)
{
    int commas = 0, semis = 0;
    bool quoted = false;
    for (int i = 0; i < line.size(); ++i) {
        const QChar ch = line.at(i);
        if (ch == QLatin1Char('"'))
            quoted = !quoted;
        else if (!quoted && ch == QLatin1Char(','))
            ++commas;
        else if (!quoted && ch == QLatin1Char(';'))
            ++semis;
    }
    return semis > commas ? QLatin1Char(';') : QLatin1Char(',');
}

static QString csvEscape(const QString &v, QChar sep)
{
    if (v.contains(sep) || v.contains(QLatin1Char('"')) || v.contains(QLatin1Char('\n'))) {
        QString e = v;
        e.replace(QLatin1Char('"'), QStringLiteral("\"\""));
        return QLatin1Char('"') + e + QLatin1Char('"');
    }
    return v;
}

bool FileIo::loadCsv(Workbook *wb, const QString &path, QString *error)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error)
            *error = QStringLiteral("Не удалось открыть csv");
        return false;
    }
    wb->resetToEmpty();
    wb->setUndoEnabled(false);
    QTextStream in(&f);
    const QString first = in.readLine();
    const QChar sep = detectCsvSep(first);
    in.seek(0);
    int r = 0;
    bool eof = false;
    while (!in.atEnd()) {
        const QStringList cols = parseCsvRecord(in, sep, &eof);
        for (int c = 0; c < cols.size(); ++c)
            wb->setRaw(0, r, c, cols.at(c));
        ++r;
        if (eof)
            break;
    }
    wb->setUndoEnabled(true);
    if (wb->undoStack())
        wb->undoStack()->clear();
    emit wb->structureChanged();
    return true;
}

bool FileIo::saveCsv(Workbook *wb, const QString &path, QString *error)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error)
            *error = QStringLiteral("Не удалось сохранить csv");
        return false;
    }
    QTextStream out(&f);
    const Worksheet &ws = wb->sheet(0);
    int maxR = 0, maxC = 0;
    for (auto it = ws.cells.cbegin(); it != ws.cells.cend(); ++it) {
        maxR = qMax(maxR, int(it.key() >> 32));
        maxC = qMax(maxC, int(it.key() & 0xffffffffu));
    }
    const QChar sep = QLatin1Char(',');
    for (int r = 0; r <= maxR; ++r) {
        QStringList row;
        for (int c = 0; c <= maxC; ++c)
            row.append(csvEscape(wb->displayText(0, r, c), sep));
        out << row.join(sep) << QLatin1Char('\n');
    }
    return true;
}
