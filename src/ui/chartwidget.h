#ifndef ARBUZ_CHARTWIDGET_H
#define ARBUZ_CHARTWIDGET_H

#include "chart.h"

#include <QWidget>

class Workbook;

class ChartWidget : public QWidget
{
    Q_OBJECT
public:
    ChartWidget(Workbook *wb, int sheetIndex, ChartObject chart, QWidget *parent = nullptr);

    ChartObject chart() const { return m_chart; }
    void setChart(const ChartObject &chart);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    Workbook *m_wb = nullptr;
    int m_sheet = 0;
    ChartObject m_chart;
};

#endif
