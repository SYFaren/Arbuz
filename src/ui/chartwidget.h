#ifndef ARBUZ_CHARTWIDGET_H
#define ARBUZ_CHARTWIDGET_H

#include "chart.h"

#include <QPoint>
#include <QWidget>

class Workbook;

class ChartWidget : public QWidget
{
    Q_OBJECT
public:
    ChartWidget(Workbook *wb, int sheetIndex, int chartIndex, ChartObject chart, QWidget *parent = nullptr);

    ChartObject chart() const { return m_chart; }
    void setChart(const ChartObject &chart);
    int chartIndex() const { return m_chartIndex; }

signals:
    void geometryChanged(int chartIndex);
    void chartEdited(int chartIndex);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    enum DragMode { None, Move, Resize };

    QRect titleBarRect() const;
    QRect resizeHandleRect() const;
    void emitGeometry();
    void clampToParent();
    void editChart();
    void drawLegend(QPainter &p, const QRect &rect, const ChartData &data, bool pieMode);
    int groupW(int n, int plotWidth) const;

    Workbook *m_wb = nullptr;
    int m_sheet = 0;
    int m_chartIndex = 0;
    ChartObject m_chart;
    DragMode m_drag = None;
    QPoint m_dragStart;
    QPoint m_originPos;
    QSize m_originSize;
};

#endif
