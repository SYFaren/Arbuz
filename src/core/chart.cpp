#include "chart.h"

#include "numformat.h"
#include "workbook.h"

#include <QtMath>
#include <algorithm>
#include <numeric>

namespace {

bool cellAsNumber(Workbook *wb, int sh, int r, int c, double *out)
{
    const QString t = wb->displayText(sh, r, c);
    if (t.isEmpty() || t.startsWith(QLatin1Char('#')))
        return false;
    return NumFormat::parse(t, out);
}

} // namespace

ChartData extractChartData(Workbook *wb, int sheetIndex, const ChartObject &chart)
{
    ChartData data;
    if (!wb || sheetIndex < 0 || sheetIndex >= wb->sheetCount())
        return data;

    const int r1 = qMin(chart.srcR1, chart.srcR2);
    const int r2 = qMax(chart.srcR1, chart.srcR2);
    const int c1 = qMin(chart.srcC1, chart.srcC2);
    const int c2 = qMax(chart.srcC1, chart.srcC2);
    if (r2 <= r1 || c2 <= c1)
        return data;

    const int labelCol = c1;
    const int firstValueCol = c1 + 1;
    if (firstValueCol > c2)
        return data;

    const int dataStartRow = chart.hasHeaderRow ? r1 + 1 : r1;
    if (dataStartRow > r2)
        return data;

    for (int c = firstValueCol; c <= c2; ++c) {
        ChartSeries series;
        if (chart.hasHeaderRow) {
            const QString hdr = wb->displayText(sheetIndex, r1, c);
            series.name = hdr.isEmpty() ? QStringLiteral("#%1").arg(c + 1) : hdr;
        } else {
            series.name = QStringLiteral("#%1").arg(c + 1);
        }
        data.series.append(series);
    }

    for (int r = dataStartRow; r <= r2; ++r) {
        QString label = wb->displayText(sheetIndex, r, labelCol);
        if (label.isEmpty())
            label = QStringLiteral("#%1").arg(r + 1);
        data.categories.append(label);

        for (int si = 0; si < data.series.size(); ++si) {
            const int c = firstValueCol + si;
            double n = 0;
            if (!cellAsNumber(wb, sheetIndex, r, c, &n))
                n = 0;
            data.series[si].values.append(n);
        }
    }

    if (data.categories.isEmpty() || data.series.isEmpty())
        return data;

    double vmin = 0;
    double vmax = 0;
    bool any = false;
    for (const ChartSeries &s : data.series) {
        for (double v : s.values) {
            if (!any) {
                vmin = vmax = v;
                any = true;
            } else {
                vmin = qMin(vmin, v);
                vmax = qMax(vmax, v);
            }
        }
    }
    if (!any)
        return data;

    if (vmax <= vmin) {
        if (vmax < 0) {
            data.yMin = vmax * 1.1;
            data.yMax = 0;
        } else {
            data.yMin = 0;
            data.yMax = qMax(1.0, vmax * 1.1);
        }
    } else {
        data.yMin = qMin(0.0, vmin);
        data.yMax = vmax * 1.05;
    }
    data.valid = true;
    return data;
}

QString chartTypeToString(ChartObject::Type type)
{
    switch (type) {
    case ChartObject::Column:
        return QStringLiteral("column");
    case ChartObject::Bar:
        return QStringLiteral("bar");
    case ChartObject::Line:
        return QStringLiteral("line");
    case ChartObject::Pie:
        return QStringLiteral("pie");
    }
    return QStringLiteral("column");
}

bool chartTypeFromString(const QString &s, ChartObject::Type *out)
{
    const QString t = s.trimmed().toLower();
    if (t == QLatin1String("column") || t == QLatin1String("0")) {
        *out = ChartObject::Column;
        return true;
    }
    if (t == QLatin1String("bar") || t == QLatin1String("1")) {
        *out = ChartObject::Bar;
        return true;
    }
    if (t == QLatin1String("line") || t == QLatin1String("2")) {
        *out = ChartObject::Line;
        return true;
    }
    if (t == QLatin1String("pie") || t == QLatin1String("3")) {
        *out = ChartObject::Pie;
        return true;
    }
    return false;
}
