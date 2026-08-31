#include "chartwidget.h"
#include "charteditdialog.h"
#include "i18n.h"
#include "workbook.h"

#include <QContextMenuEvent>
#include <QFontMetrics>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>
#include <numeric>

namespace {
constexpr int kTitleH = 24;
constexpr int kHandle = 14;
constexpr int kFrame = 1;
constexpr int kAxisLeft = 44;
constexpr int kAxisBottom = 28;
constexpr int kLegendW = 96;

const QColor kPalette[] = {
    QColor(176, 34, 46), QColor(18, 64, 28), QColor(52, 108, 58), QColor(245, 180, 50),
    QColor(80, 120, 200), QColor(160, 80, 180), QColor(200, 100, 80), QColor(100, 160, 160),
};

QString shortLabel(const QString &text, int maxPx, const QFontMetrics &fm)
{
    if (fm.horizontalAdvance(text) <= maxPx)
        return text;
    QString s = text;
    while (!s.isEmpty() && fm.horizontalAdvance(s + QLatin1String("…")) > maxPx)
        s.chop(1);
    return s + QLatin1String("…");
}

QVector<double> yTicks(double ymin, double ymax, int count = 5)
{
    QVector<double> ticks;
    if (count < 2 || ymax <= ymin) {
        ticks << ymin << ymax;
        return ticks;
    }
    const double span = ymax - ymin;
    const double raw = span / (count - 1);
    const double mag = qPow(10.0, qFloor(qLn(raw) / qLn(10.0)));
    const double step = qCeil(raw / mag) * mag;
    double v = qFloor(ymin / step) * step;
    while (v <= ymax + step * 0.01) {
        if (v >= ymin - step * 0.01)
            ticks.append(v);
        v += step;
    }
    if (ticks.isEmpty())
        ticks << ymin << ymax;
    return ticks;
}

} // namespace

ChartWidget::ChartWidget(Workbook *wb, int sheetIndex, int chartIndex, ChartObject chart, QWidget *parent)
    : QWidget(parent)
    , m_wb(wb)
    , m_sheet(sheetIndex)
    , m_chartIndex(chartIndex)
    , m_chart(chart)
{
    setMinimumSize(180, 140);
    resize(m_chart.widthPx, m_chart.heightPx);
    setMouseTracking(true);
    setFocusPolicy(Qt::ClickFocus);
}

void ChartWidget::setChart(const ChartObject &chart)
{
    m_chart = chart;
    resize(m_chart.widthPx, m_chart.heightPx);
    update();
}

QRect ChartWidget::titleBarRect() const
{
    return QRect(kFrame, kFrame, width() - kFrame * 2, kTitleH);
}

QRect ChartWidget::resizeHandleRect() const
{
    return QRect(width() - kHandle - kFrame, height() - kHandle - kFrame, kHandle, kHandle);
}

void ChartWidget::emitGeometry()
{
    m_chart.widthPx = width();
    m_chart.heightPx = height();
    emit geometryChanged(m_chartIndex);
}

void ChartWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    raise();
    setFocus();
    const QPoint p = event->pos();
    if (resizeHandleRect().contains(p)) {
        m_drag = Resize;
        m_dragStart = event->globalPosition().toPoint();
        m_originSize = size();
    } else if (titleBarRect().contains(p)) {
        m_drag = Move;
        m_dragStart = event->globalPosition().toPoint();
        m_originPos = pos();
    }
    event->accept();
}

void ChartWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_drag == Move) {
        const QPoint delta = event->globalPosition().toPoint() - m_dragStart;
        move(m_originPos + delta);
        emitGeometry();
        event->accept();
        return;
    }
    if (m_drag == Resize) {
        const QPoint delta = event->globalPosition().toPoint() - m_dragStart;
        const int w = qMax(minimumWidth(), m_originSize.width() + delta.x());
        const int h = qMax(minimumHeight(), m_originSize.height() + delta.y());
        resize(w, h);
        emitGeometry();
        event->accept();
        return;
    }

    const QPoint p = event->pos();
    if (resizeHandleRect().contains(p))
        setCursor(Qt::SizeFDiagCursor);
    else if (titleBarRect().contains(p))
        setCursor(Qt::SizeAllCursor);
    else
        unsetCursor();
    QWidget::mouseMoveEvent(event);
}

void ChartWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_drag != None) {
        m_drag = None;
        unsetCursor();
        emitGeometry();
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void ChartWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (titleBarRect().contains(event->pos()) || (rect().contains(event->pos()) && !resizeHandleRect().contains(event->pos()))) {
        editChart();
        event->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

void ChartWidget::editChart()
{
    ChartEditDialog dlg(m_chart, this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    m_chart = dlg.resultChart();
    if (m_wb && m_sheet >= 0 && m_sheet < m_wb->sheetCount() && m_chartIndex >= 0
        && m_chartIndex < m_wb->sheet(m_sheet).charts.size()) {
        m_wb->sheet(m_sheet).charts[m_chartIndex] = m_chart;
    }
    update();
    emit chartEdited(m_chartIndex);
}

void ChartWidget::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    menu.addAction(I18n::t("ui.edit_chart"), this, &ChartWidget::editChart);
    menu.addAction(I18n::t("ui.delete_chart"), this, [this]() {
        if (!m_wb || m_sheet < 0 || m_sheet >= m_wb->sheetCount())
            return;
        Worksheet &ws = m_wb->sheet(m_sheet);
        if (m_chartIndex >= 0 && m_chartIndex < ws.charts.size()) {
            ws.charts.removeAt(m_chartIndex);
            hide();
            deleteLater();
            emit geometryChanged(-1);
        }
    });
    menu.exec(event->globalPos());
}

void ChartWidget::drawLegend(QPainter &p, const QRect &rect, const ChartData &data, bool pieMode)
{
    QFont f = p.font();
    f.setPointSize(qMax(7, f.pointSize() - 1));
    p.setFont(f);
    const QFontMetrics fm(f);
    int y = rect.top() + 4;
    const int box = 10;
    const int n = pieMode ? data.categories.size() : data.series.size();
    for (int i = 0; i < n; ++i) {
        const QString label = pieMode ? data.categories.at(i) : data.series.at(i).name;
        p.fillRect(rect.left(), y, box, box, kPalette[i % 8]);
        p.setPen(QColor(60, 60, 60));
        p.drawRect(rect.left(), y, box, box);
        p.setPen(Qt::black);
        p.drawText(rect.left() + box + 4, y, rect.width() - box - 6, box, Qt::AlignVCenter | Qt::AlignLeft,
                   shortLabel(label, rect.width() - box - 8, fm));
        y += box + 4;
        if (y > rect.bottom() - box)
            break;
    }
}

void ChartWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    p.fillRect(rect(), QColor(255, 255, 255, 252));
    p.setPen(QColor(120, 120, 120));
    p.drawRect(rect().adjusted(0, 0, -1, -1));

    p.fillRect(titleBarRect(), QColor(236, 240, 244));
    p.setPen(QColor(90, 90, 90));
    p.drawLine(kFrame, kTitleH + kFrame, width() - kFrame, kTitleH + kFrame);
    const QString title = m_chart.title.isEmpty() ? I18n::t("ui.chart") : m_chart.title;
    p.setPen(Qt::black);
    p.drawText(titleBarRect().adjusted(6, 0, -6, 0), Qt::AlignVCenter | Qt::AlignLeft,
               shortLabel(title, titleBarRect().width() - 12, p.fontMetrics()));

    p.fillRect(resizeHandleRect(), QColor(200, 200, 200));
    p.setPen(QColor(80, 80, 80));
    const QRect h = resizeHandleRect().adjusted(3, 3, -3, -3);
    p.drawLine(h.bottomLeft(), h.topRight());
    p.drawLine(h.left() + 4, h.bottom() - 1, h.right() - 1, h.top() + 4);

    if (!m_wb || m_sheet < 0 || m_sheet >= m_wb->sheetCount())
        return;

    const ChartData data = extractChartData(m_wb, m_sheet, m_chart);
    const int legendW = (m_chart.showLegend && !data.series.isEmpty()) ? kLegendW : 0;
    const QRect body = rect().adjusted(4, kTitleH + 6, -kHandle - 2, -kHandle - 2);
    const QRect plot = body.adjusted(kAxisLeft, 4, -legendW - 4, -kAxisBottom);
    const QRect legendRect(body.right() - legendW + 2, body.top() + 4, legendW - 4, body.height() - 4);

    if (!data.valid || plot.width() < 24 || plot.height() < 24) {
        p.drawText(body, Qt::AlignCenter, I18n::t("ui.chart_no_data"));
        return;
    }

    const QVector<double> ticks = yTicks(data.yMin, data.yMax);
    const double ymin = ticks.first();
    const double ymax = ticks.last();
    const double yspan = qMax(1e-9, ymax - ymin);

    p.setPen(QColor(180, 180, 180));
    QFont axisFont = p.font();
    axisFont.setPointSize(qMax(7, axisFont.pointSize() - 1));
    p.setFont(axisFont);
    const QFontMetrics afm(axisFont);

    for (double tv : ticks) {
        const int y = plot.bottom() - int((tv - ymin) / yspan * plot.height());
        p.drawLine(plot.left(), y, plot.right(), y);
        p.setPen(QColor(80, 80, 80));
        const QString label = QString::number(tv, 'g', 4);
        p.drawText(4, y - 6, kAxisLeft - 8, 12, Qt::AlignRight | Qt::AlignVCenter, label);
        p.setPen(QColor(180, 180, 180));
    }
    p.setPen(QColor(100, 100, 100));
    p.drawLine(plot.left(), plot.top(), plot.left(), plot.bottom());
    p.drawLine(plot.left(), plot.bottom(), plot.right(), plot.bottom());

    if (m_chart.type == ChartObject::Pie) {
        const ChartSeries &s = data.series.first();
        const double sum = std::accumulate(s.values.cbegin(), s.values.cend(), 0.0);
        if (sum <= 0) {
            p.drawText(plot, Qt::AlignCenter, I18n::t("ui.chart_no_data"));
            return;
        }
        const QPointF center(plot.center());
        const int radius = qMin(plot.width(), plot.height()) / 2 - 8;
        double start = 90.0 * 16.0;
        for (int i = 0; i < s.values.size(); ++i) {
            const double span = s.values.at(i) / sum * 360.0 * 16.0;
            p.setBrush(kPalette[i % 8]);
            p.setPen(Qt::white);
            p.drawPie(QRectF(center.x() - radius, center.y() - radius, radius * 2, radius * 2), int(start),
                      int(-span));
            start -= span;
        }
        if (m_chart.showLegend)
            drawLegend(p, legendRect, data, true);
        return;
    }

    const int n = data.categories.size();
    const int seriesCount = data.series.size();
    const int gap = 3;

    if (m_chart.type == ChartObject::Line) {
        for (int si = 0; si < seriesCount; ++si) {
            const ChartSeries &s = data.series.at(si);
            QPainterPath path;
            for (int i = 0; i < n; ++i) {
                const double x = plot.left() + (n > 1 ? i * double(plot.width()) / (n - 1) : plot.width() / 2.0);
                const double y = plot.bottom() - (s.values.at(i) - ymin) / yspan * plot.height();
                if (i == 0)
                    path.moveTo(x, y);
                else
                    path.lineTo(x, y);
            }
            p.setPen(QPen(kPalette[si % 8], 2));
            p.drawPath(path);
            p.setBrush(kPalette[si % 8]);
            for (int i = 0; i < n; ++i) {
                const double x = plot.left() + (n > 1 ? i * double(plot.width()) / (n - 1) : plot.width() / 2.0);
                const double y = plot.bottom() - (s.values.at(i) - ymin) / yspan * plot.height();
                p.drawEllipse(QPointF(x, y), 3, 3);
            }
        }
    } else if (m_chart.type == ChartObject::Bar) {
        const int groupH = qMax(6, (plot.height() - gap * (n + 1)) / qMax(1, n));
        for (int i = 0; i < n; ++i) {
            const int gy = plot.top() + gap + i * (groupH + gap);
            const int barH = qMax(2, (groupH - gap * (seriesCount - 1)) / qMax(1, seriesCount));
            for (int si = 0; si < seriesCount; ++si) {
                const double v = data.series.at(si).values.at(i);
                const int w = int((v - ymin) / yspan * plot.width());
                const int y = gy + si * (barH + 1);
                p.fillRect(plot.left(), y, qMax(0, w), barH, kPalette[si % 8]);
            }
        }
    } else {
        const int groupW = qMax(8, (plot.width() - gap * (n + 1)) / qMax(1, n));
        for (int i = 0; i < n; ++i) {
            const int gx = plot.left() + gap + i * (groupW + gap);
            const int barW = qMax(2, (groupW - gap * (seriesCount - 1)) / qMax(1, seriesCount));
            for (int si = 0; si < seriesCount; ++si) {
                const double v = data.series.at(si).values.at(i);
                const int h = int((v - ymin) / yspan * plot.height());
                const int x = gx + si * (barW + 1);
                p.fillRect(x, plot.bottom() - h, barW, h, kPalette[si % 8]);
            }
        }
    }

    p.setPen(QColor(60, 60, 60));
    const int catW = groupW(n, plot.width());
    for (int i = 0; i < n; ++i) {
        const int cx = (m_chart.type == ChartObject::Bar)
            ? plot.left()
            : int(plot.left() + (n > 1 ? i * double(plot.width()) / (n - 1) : plot.width() / 2.0));
        const QString lab = shortLabel(data.categories.at(i), catW, afm);
        if (m_chart.type == ChartObject::Bar) {
            const int groupH = qMax(6, (plot.height() - gap * (n + 1)) / qMax(1, n));
            const int gy = plot.top() + gap + i * (groupH + gap);
            p.drawText(plot.left() - kAxisLeft + 2, gy, kAxisLeft - 4, groupH, Qt::AlignRight | Qt::AlignVCenter, lab);
        } else {
            p.drawText(cx - 20, plot.bottom() + 2, 40, kAxisBottom - 4, Qt::AlignHCenter | Qt::AlignTop, lab);
        }
    }

    if (m_chart.showLegend && seriesCount > 1)
        drawLegend(p, legendRect, data, false);
    else if (m_chart.showLegend && m_chart.type == ChartObject::Line && seriesCount == 1)
        drawLegend(p, legendRect, data, false);
}

int ChartWidget::groupW(int n, int plotWidth) const
{
    if (n <= 0)
        return 40;
    const int gap = 3;
    return qMax(8, (plotWidth - gap * (n + 1)) / n);
}
