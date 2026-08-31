#ifndef ARBUZ_CHART_H
#define ARBUZ_CHART_H

#include <QString>

struct ChartObject {
    enum Type { Column, Bar, Line, Pie };

    Type type = Column;
    int srcR1 = 0;
    int srcC1 = 0;
    int srcR2 = 0;
    int srcC2 = 0;
    int anchorRow = 1;
    int anchorCol = 3;
    int widthPx = 360;
    int heightPx = 240;
    QString title;
};

#endif
