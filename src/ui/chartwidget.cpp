#include "chartwidget.h"
#include "numformat.h"
#include "workbook.h"

#include <QPainter>
#include <QPainterPath>
#include <QtMath>
#include <numeric>

ChartWidget::ChartWidget(Workbook *wb, int sheetIndex, ChartObject chart, QWidget *parent)
    : QWidget(parent)
    , m_wb(wb)
    , m_sheet(sheetIndex)
    , m_chart(chart)
{
    setMinimumSize(120, 80);
    resize(m_chart.widthPx, m_chart.heightPx);
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(255, 255, 255, 245));
    setPalette(pal);
}

void ChartWidget::setChart(const ChartObject &chart)
{
    m_chart = chart;
    resize(m_chart.widthPx, m_chart.heightPx);
    update();
}

void ChartWidget::paintEvent(QPaintEvent *)
{
    if (!m_wb || m_sheet < 0 || m_sheet >= m_wb->sheetCount())
        return;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), QColor(255, 255, 255, 245));
    p.setPen(QColor(180, 180, 180));
    p.drawRect(rect().adjusted(0, 0, -1, -1));

    const int r1 = qMin(m_chart.srcR1, m_chart.srcR2);
    const int r2 = qMax(m_chart.srcR1, m_chart.srcR2);
    const int c1 = qMin(m_chart.srcC1, m_chart.srcC2);
    const int c2 = qMax(m_chart.srcC1, m_chart.srcC2);
    if (r2 <= r1 || c2 <= c1) {
        p.drawText(rect(), Qt::AlignCenter, QStringLiteral("—"));
        return;
    }

    QStringList labels;
    QVector<double> values;
    const int labelCol = c1;
    const int valueCol = c1 + 1;
    if (valueCol > c2) {
        p.drawText(rect(), Qt::AlignCenter, QStringLiteral("—"));
        return;
    }
    for (int r = r1 + 1; r <= r2; ++r) {
        const QString label = m_wb->displayText(m_sheet, r, labelCol);
        const QString valText = m_wb->displayText(m_sheet, r, valueCol);
        double n = 0;
        if (!NumFormat::parse(valText, &n))
            continue;
        labels.append(label.isEmpty() ? QStringLiteral("#%1").arg(r) : label);
        values.append(n);
    }
    if (values.isEmpty()) {
        p.drawText(rect(), Qt::AlignCenter, QStringLiteral("—"));
        return;
    }

    const QRect area = rect().adjusted(8, 24, -8, -8);
    if (!m_chart.title.isEmpty()) {
        p.setPen(Qt::black);
        p.drawText(QRect(8, 4, width() - 16, 18), Qt::AlignCenter, m_chart.title);
    }

    static const QColor palette[] = {
        QColor(176, 34, 46), QColor(18, 64, 28), QColor(52, 108, 58), QColor(245, 180, 50),
        QColor(80, 120, 200), QColor(160, 80, 180), QColor(200, 100, 80), QColor(100, 160, 160),
    };

    if (m_chart.type == ChartObject::Pie) {
        const double sum = std::accumulate(values.cbegin(), values.cend(), 0.0);
        if (sum <= 0)
            return;
        const QPointF center(area.center());
        const int radius = qMin(area.width(), area.height()) / 2 - 4;
        double start = 90.0 * 16.0;
        for (int i = 0; i < values.size(); ++i) {
            const double span = values.at(i) / sum * 360.0 * 16.0;
            p.setBrush(palette[i % 8]);
            p.setPen(Qt::NoPen);
            p.drawPie(QRectF(center.x() - radius, center.y() - radius, radius * 2, radius * 2),
                      int(start), int(-span));
            start -= span;
        }
        p.setPen(Qt::black);
        int ly = area.bottom() - labels.size() * 12;
        for (int i = 0; i < labels.size(); ++i) {
            p.setBrush(palette[i % 8]);
            p.drawRect(area.left(), ly + i * 12, 10, 10);
            p.drawText(area.left() + 14, ly + i * 12 + 10, labels.at(i));
        }
        return;
    }

    const double vmax = *std::max_element(values.cbegin(), values.cend());
    const double scale = vmax > 0 ? (area.height() - 20) / vmax : 1.0;
    const int n = values.size();
    const int gap = 4;
    const bool horizontal = m_chart.type == ChartObject::Bar;

    if (m_chart.type == ChartObject::Line) {
        QPainterPath path;
        const double step = n > 1 ? double(area.width()) / (n - 1) : 0;
        for (int i = 0; i < n; ++i) {
            const double x = area.left() + i * step;
            const double y = area.bottom() - values.at(i) * scale;
            if (i == 0)
                path.moveTo(x, y);
            else
                path.lineTo(x, y);
        }
        p.setPen(QPen(palette[0], 2));
        p.drawPath(path);
        p.setBrush(palette[0]);
        for (int i = 0; i < n; ++i) {
            const double x = area.left() + (n > 1 ? i * double(area.width()) / (n - 1) : area.width() / 2.0);
            const double y = area.bottom() - values.at(i) * scale;
            p.drawEllipse(QPointF(x, y), 3, 3);
        }
    } else if (horizontal) {
        const int barH = qMax(4, (area.height() - gap * (n + 1)) / qMax(1, n));
        for (int i = 0; i < n; ++i) {
            const int y = area.top() + gap + i * (barH + gap);
            const int w = int(values.at(i) * scale);
            p.fillRect(area.left(), y, w, barH, palette[i % 8]);
        }
    } else {
        const int barW = qMax(4, (area.width() - gap * (n + 1)) / qMax(1, n));
        for (int i = 0; i < n; ++i) {
            const int x = area.left() + gap + i * (barW + gap);
            const int h = int(values.at(i) * scale);
            p.fillRect(x, area.bottom() - h, barW, h, palette[i % 8]);
        }
    }

    p.setPen(Qt::black);
    if (horizontal) {
        const int barH = qMax(4, (area.height() - gap * (n + 1)) / qMax(1, n));
        for (int i = 0; i < n; ++i) {
            const int y = area.top() + gap + i * (barH + gap);
            p.drawText(area.left() + int(values.at(i) * scale) + 4, y + barH - 2, labels.at(i));
        }
    } else if (m_chart.type != ChartObject::Line) {
        const int barW = qMax(4, (area.width() - gap * (n + 1)) / qMax(1, n));
        for (int i = 0; i < n; ++i) {
            const int x = area.left() + gap + i * (barW + gap);
            p.drawText(x, area.bottom() + 12, barW, 12, Qt::AlignHCenter, labels.at(i));
        }
    }
}
