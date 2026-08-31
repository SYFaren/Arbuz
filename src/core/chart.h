#ifndef ARBUZ_CHART_H
#define ARBUZ_CHART_H

#include <QRect>
#include <QString>
#include <QVector>

class Workbook;

struct ChartObject {
    enum Type { Column, Bar, Line, Pie };

    Type type = Column;
    int srcR1 = 0;
    int srcC1 = 0;
    int srcR2 = 0;
    int srcC2 = 0;
    int posX = 80;
    int posY = 80;
    int widthPx = 360;
    int heightPx = 260;
    QString title;
    bool hasHeaderRow = true;
    bool showLegend = true;
};

struct ChartSeries {
    QString name;
    QVector<double> values;
};

struct ChartData {
    QStringList categories;
    QVector<ChartSeries> series;
    double yMin = 0;
    double yMax = 0;
    bool valid = false;
};

ChartData extractChartData(Workbook *wb, int sheetIndex, const ChartObject &chart);
QString chartTypeToString(ChartObject::Type type);
bool chartTypeFromString(const QString &s, ChartObject::Type *out);
QRect clampChartGeometry(const QRect &geo, const QSize &bounds, const QSize &minSize, int margin = 8);

#endif
