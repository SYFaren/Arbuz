#ifndef ARBUZ_CHARTEDITDIALOG_H
#define ARBUZ_CHARTEDITDIALOG_H

#include "chart.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLineEdit;

class ChartEditDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ChartEditDialog(const ChartObject &chart, QWidget *parent = nullptr);

    ChartObject resultChart() const;

private:
    static QString rangeText(const ChartObject &chart);

    ChartObject m_chart;
    QLineEdit *m_title = nullptr;
    QLineEdit *m_range = nullptr;
    QComboBox *m_type = nullptr;
    QCheckBox *m_header = nullptr;
    QCheckBox *m_legend = nullptr;
};

#endif
