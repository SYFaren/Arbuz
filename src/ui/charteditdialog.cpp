#include "charteditdialog.h"
#include "cellref.h"
#include "i18n.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QVBoxLayout>

ChartEditDialog::ChartEditDialog(const ChartObject &chart, QWidget *parent)
    : QDialog(parent)
    , m_chart(chart)
{
    setWindowTitle(I18n::t("ui.edit_chart"));
    auto *form = new QFormLayout;

    m_title = new QLineEdit(chart.title, this);
    form->addRow(I18n::t("ui.chart_title"), m_title);

    m_range = new QLineEdit(this);
    m_range->setText(rangeText(chart));
    form->addRow(I18n::t("ui.chart_range"), m_range);

    m_type = new QComboBox(this);
    m_type->addItem(I18n::t("ui.chart_column"), ChartObject::Column);
    m_type->addItem(I18n::t("ui.chart_bar"), ChartObject::Bar);
    m_type->addItem(I18n::t("ui.chart_line"), ChartObject::Line);
    m_type->addItem(I18n::t("ui.chart_pie"), ChartObject::Pie);
    for (int i = 0; i < m_type->count(); ++i) {
        if (m_type->itemData(i).toInt() == int(chart.type)) {
            m_type->setCurrentIndex(i);
            break;
        }
    }
    form->addRow(I18n::t("ui.chart_type"), m_type);

    m_header = new QCheckBox(I18n::t("ui.chart_header_row"), this);
    m_header->setChecked(chart.hasHeaderRow);
    form->addRow(QString(), m_header);

    m_legend = new QCheckBox(I18n::t("ui.chart_show_legend"), this);
    m_legend->setChecked(chart.showLegend);
    form->addRow(QString(), m_legend);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);
}

QString ChartEditDialog::rangeText(const ChartObject &chart)
{
    const int r1 = qMin(chart.srcR1, chart.srcR2);
    const int r2 = qMax(chart.srcR1, chart.srcR2);
    const int c1 = qMin(chart.srcC1, chart.srcC2);
    const int c2 = qMax(chart.srcC1, chart.srcC2);
    return CellRef::a1(r1, c1) + QLatin1Char(':') + CellRef::a1(r2, c2);
}

ChartObject ChartEditDialog::resultChart() const
{
    ChartObject out = m_chart;
    out.title = m_title->text().trimmed();
    out.type = ChartObject::Type(m_type->currentData().toInt());
    out.hasHeaderRow = m_header->isChecked();
    out.showLegend = m_legend->isChecked();

    const QString range = m_range->text().trimmed();
    int r1 = 0, c1 = 0, r2 = 0, c2 = 0;
    if (CellRef::parseA1Range(range, &r1, &c1, &r2, &c2)) {
        out.srcR1 = r1;
        out.srcC1 = c1;
        out.srcR2 = r2;
        out.srcC2 = c2;
    }
    return out;
}
