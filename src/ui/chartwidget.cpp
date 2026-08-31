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
#include <QResizeEvent>
#include <QtMath>
#include <numeric>

namespace {
constexpr int kPad = 10;
constexpr int kTitleH = 26;
constexpr int kGrip = 12;
constexpr int kGap = 6;
constexpr int kAxisBottom = 22;
constexpr int kMinLegendW = 88;

const QColor kTitleBg(18, 64, 28);
const QColor kTitleText(255, 255, 255);
const QColor kFrameBorder(196, 206, 196);
const QColor kPlotBg(252, 253, 251);
const QColor kPlotBorder(216, 224, 216);
const QColor kGridLine(232, 236, 232);
const QColor kAxisText(72, 84, 72);

const QColor kPalette[] = {
    QColor(176, 34, 46), QColor(34, 108, 58), QColor(52, 120, 180), QColor(230, 162, 40),
    QColor(120, 90, 180), QColor(200, 90, 70), QColor(60, 160, 160), QColor(140, 140, 140),
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

QString formatTick(double v)
{
    if (qAbs(v - qRound(v)) < 1e-6 && qAbs(v) < 1e12)
        return QString::number(qint64(qRound(v)));
    return QString::number(v, 'g', 4);
}

struct ValueAxis {
    double min = 0;
    double max = 1;
    QVector<double> ticks;
};

ValueAxis computeValueAxis(double dataMin, double dataMax, int targetTicks = 5)
{
    ValueAxis ax;
    ax.min = qMin(0.0, dataMin);
    const double hi = qMax(dataMax, ax.min + 1e-9);
    const double span = hi - ax.min;
    const double raw = span / qMax(1, targetTicks - 1);
    double step = 1.0;
    if (raw > 1e-12) {
        const double mag = qPow(10.0, qFloor(qLn(raw) / qLn(10.0)));
        step = qMax(mag, qCeil(raw / mag) * mag);
    }
    ax.max = qCeil(hi / step) * step;
    ax.min = qFloor(ax.min / step) * step;
    for (double v = ax.min; v <= ax.max + step * 1e-4; v += step)
        ax.ticks.append(v);
    if (ax.ticks.isEmpty())
        ax.ticks << ax.min << ax.max;
    return ax;
}

void fillBar(QPainter &p, const QRect &r, const QColor &base)
{
    if (r.width() <= 0 || r.height() <= 0)
        return;
    const int rad = qMin(4, qMin(r.width(), r.height()) / 2);
    QPainterPath path;
    path.addRoundedRect(r, rad, rad);
    p.setPen(base.darker(120));
    p.setBrush(base);
    p.drawPath(path);
}

struct PlotLayout {
    QRect body;
    QRect plot;
    QRect legend;
    int axisLeft = 40;
    int legendW = 0;
    int groupW = 0;
    int groupH = 0;
};

int legendWidth(bool showLegend, bool categoryLegend, ChartObject::Type type, const ChartData &data,
                const QFontMetrics &lfm, int bodyWidth)
{
    if (!showLegend || data.series.isEmpty())
        return 0;
    int w = kMinLegendW;
    const int n = categoryLegend || type == ChartObject::Pie ? data.categories.size() : data.series.size();
    for (int i = 0; i < n; ++i) {
        const QString label = categoryLegend || type == ChartObject::Pie ? data.categories.at(i)
                                                                           : data.series.at(i).name;
        w = qMax(w, lfm.horizontalAdvance(label) + 28);
    }
    return qMin(w + 4, qMax(kMinLegendW, bodyWidth / 2));
}

PlotLayout computeLayout(const QRect &widget, bool showLegend, bool categoryLegend, const ChartData &data,
                         ChartObject::Type type, const ValueAxis &axis, const QFontMetrics &afm,
                         const QFont &axisFont)
{
    PlotLayout lay;
    lay.body = widget.adjusted(kPad, kTitleH + kPad, -kPad, -kPad);

    QFont lf = axisFont;
    lf.setPointSize(qMax(8, lf.pointSize() - 1));
    const QFontMetrics lfm(lf);
    lay.legendW = legendWidth(showLegend, categoryLegend, type, data, lfm, lay.body.width());

    if (type == ChartObject::Pie) {
        lay.axisLeft = 0;
        lay.plot = lay.body.adjusted(0, 0, -lay.legendW - (lay.legendW > 0 ? 6 : 0), 0);
        lay.legend = QRect(lay.plot.right() + 6, lay.body.top(), qMax(0, lay.legendW - 6), lay.body.height());
        return lay;
    }

    if (type == ChartObject::Bar) {
        lay.axisLeft = 10;
        for (const QString &cat : data.categories)
            lay.axisLeft = qMax(lay.axisLeft, afm.horizontalAdvance(cat) + 12);
        lay.plot = lay.body.adjusted(lay.axisLeft, 4, -lay.legendW - 4, -kAxisBottom);
        lay.legend = QRect(lay.plot.right() + 6, lay.body.top() + 4, qMax(0, lay.legendW - 6), lay.plot.height());
        const int n = data.categories.size();
        if (n > 0)
            lay.groupH = qMax(12, (lay.plot.height() - kGap * (n + 1)) / qMax(1, n));
        return lay;
    }

    lay.axisLeft = 10;
    for (double tv : axis.ticks)
        lay.axisLeft = qMax(lay.axisLeft, afm.horizontalAdvance(formatTick(tv)) + 12);
    lay.plot = lay.body.adjusted(lay.axisLeft, 4, -lay.legendW - 4, -kAxisBottom);
    lay.legend = QRect(lay.plot.right() + 6, lay.body.top() + 4, qMax(0, lay.legendW - 6), lay.plot.height());

    const int n = data.categories.size();
    if (n > 0)
        lay.groupW = qMax(12, (lay.plot.width() - kGap * (n + 1)) / qMax(1, n));
    return lay;
}

void drawVerticalValueAxis(QPainter &p, const PlotLayout &lay, const ValueAxis &axis)
{
    const double span = qMax(1e-9, axis.max - axis.min);
    p.setPen(kGridLine);
    for (double tv : axis.ticks) {
        const int y = lay.plot.bottom() - int((tv - axis.min) / span * lay.plot.height());
        p.drawLine(lay.plot.left(), y, lay.plot.right(), y);
        p.setPen(kAxisText);
        p.drawText(lay.body.left(), y - 7, lay.axisLeft - 6, 14, Qt::AlignRight | Qt::AlignVCenter, formatTick(tv));
        p.setPen(kGridLine);
    }
    p.setPen(kAxisText);
    p.drawLine(lay.plot.left(), lay.plot.top(), lay.plot.left(), lay.plot.bottom());
    p.drawLine(lay.plot.left(), lay.plot.bottom(), lay.plot.right(), lay.plot.bottom());
}

void drawHorizontalValueAxis(QPainter &p, const PlotLayout &lay, const ValueAxis &axis)
{
    const double span = qMax(1e-9, axis.max - axis.min);
    p.setPen(kGridLine);
    for (double tv : axis.ticks) {
        const int x = lay.plot.left() + int((tv - axis.min) / span * lay.plot.width());
        p.drawLine(x, lay.plot.top(), x, lay.plot.bottom());
        p.setPen(kAxisText);
        p.drawText(x - 24, lay.plot.bottom() + 3, 48, kAxisBottom - 6, Qt::AlignHCenter | Qt::AlignTop, formatTick(tv));
        p.setPen(kGridLine);
    }
    p.setPen(kAxisText);
    p.drawLine(lay.plot.left(), lay.plot.top(), lay.plot.left(), lay.plot.bottom());
    p.drawLine(lay.plot.left(), lay.plot.bottom(), lay.plot.right(), lay.plot.bottom());
}

} // namespace

ChartWidget::ChartWidget(Workbook *wb, int sheetIndex, int chartIndex, ChartObject chart, QWidget *parent)
    : QWidget(parent)
    , m_wb(wb)
    , m_sheet(sheetIndex)
    , m_chartIndex(chartIndex)
    , m_chart(chart)
{
    setMinimumSize(220, 170);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_NoSystemBackground, true);
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
    return QRect(0, 0, width(), kTitleH);
}

QRect ChartWidget::resizeHandleRect() const
{
    return QRect(width() - kGrip - 2, height() - kGrip - 2, kGrip, kGrip);
}

void ChartWidget::emitGeometry()
{
    m_chart.widthPx = width();
    m_chart.heightPx = height();
    emit geometryChanged(m_chartIndex);
}

void ChartWidget::clampToParent()
{
    QWidget *p = parentWidget();
    if (!p)
        return;
    const QRect clamped = clampChartGeometry(geometry(), p->size(), minimumSize());
    if (geometry() != clamped)
        setGeometry(clamped);
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
        clampToParent();
        emitGeometry();
        event->accept();
        return;
    }
    if (m_drag == Resize) {
        const QPoint delta = event->globalPosition().toPoint() - m_dragStart;
        const int w = qMax(minimumWidth(), m_originSize.width() + delta.x());
        const int h = qMax(minimumHeight(), m_originSize.height() + delta.y());
        resize(w, h);
        clampToParent();
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
        clampToParent();
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

void ChartWidget::drawLegend(QPainter &p, const QRect &rect, const ChartData &data, bool categoryMode)
{
    if (rect.width() < 20)
        return;
    QFont f = p.font();
    f.setPointSize(qMax(8, f.pointSize() - 1));
    p.setFont(f);
    const QFontMetrics fm(f);
    int y = rect.top() + 2;
    const int box = 10;
    const int n = categoryMode ? data.categories.size() : data.series.size();
    for (int i = 0; i < n; ++i) {
        const QString label = categoryMode ? data.categories.at(i) : data.series.at(i).name;
        p.setBrush(kPalette[i % 8]);
        p.setPen(kPalette[i % 8].darker(115));
        p.drawRoundedRect(rect.left(), y, box, box, 2, 2);
        p.setPen(kAxisText);
        p.drawText(rect.left() + box + 6, y, rect.width() - box - 8, box + 2, Qt::AlignVCenter | Qt::AlignLeft,
                   shortLabel(label, rect.width() - box - 10, fm));
        y += box + 6;
        if (y > rect.bottom() - box)
            break;
    }
}

void ChartWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    clampToParent();
}

void ChartWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    p.fillRect(rect(), Qt::white);
    p.setPen(kFrameBorder);
    p.drawRect(rect().adjusted(0, 0, -1, -1));

    p.fillRect(titleBarRect(), kTitleBg);
    const QString title = m_chart.title.isEmpty() ? I18n::t("ui.chart") : m_chart.title;
    p.setPen(kTitleText);
    QFont titleFont = p.font();
    titleFont.setBold(true);
    p.setFont(titleFont);
    p.drawText(titleBarRect().adjusted(10, 0, -10, 0), Qt::AlignVCenter | Qt::AlignLeft,
               shortLabel(title, titleBarRect().width() - 20, p.fontMetrics()));

    p.setPen(QColor(160, 168, 160));
    const QRect grip = resizeHandleRect().adjusted(2, 2, -2, -2);
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            p.fillRect(grip.right() - 3 - i * 4, grip.bottom() - 3 - j * 4, 2, 2, QColor(140, 150, 140));

    if (!m_wb || m_sheet < 0 || m_sheet >= m_wb->sheetCount())
        return;

    const ChartData data = extractChartData(m_wb, m_sheet, m_chart);
    QFont axisFont = p.font();
    axisFont.setBold(false);
    axisFont.setPointSize(qMax(8, axisFont.pointSize() - 1));
    p.setFont(axisFont);
    const QFontMetrics afm(axisFont);

    const int n = data.categories.size();
    const int seriesCount = data.series.size();
    const bool categoryLegend = m_chart.showLegend && seriesCount == 1
        && (m_chart.type == ChartObject::Column || m_chart.type == ChartObject::Bar);
    const bool showLegend = m_chart.showLegend && !data.series.isEmpty()
        && (seriesCount > 1 || m_chart.type == ChartObject::Line || m_chart.type == ChartObject::Pie
            || categoryLegend);

    const ValueAxis axis = m_chart.type == ChartObject::Pie ? ValueAxis{} : computeValueAxis(data.yMin, data.yMax);
    const PlotLayout lay =
        computeLayout(rect(), showLegend, categoryLegend, data, m_chart.type, axis, afm, axisFont);

    if (!data.valid || lay.plot.width() < 32 || lay.plot.height() < 32) {
        p.setPen(kAxisText);
        p.drawText(lay.body, Qt::AlignCenter, I18n::t("ui.chart_no_data"));
        return;
    }

    if (m_chart.type == ChartObject::Pie) {
        p.fillRect(lay.plot, kPlotBg);
        p.setPen(kPlotBorder);
        p.drawRect(lay.plot);

        const ChartSeries &s = data.series.first();
        const double sum = std::accumulate(s.values.cbegin(), s.values.cend(), 0.0);
        if (sum <= 0) {
            p.setPen(kAxisText);
            p.drawText(lay.plot, Qt::AlignCenter, I18n::t("ui.chart_no_data"));
            return;
        }
        const QPointF center(lay.plot.center());
        const int radius = qMin(lay.plot.width(), lay.plot.height()) / 2 - 12;
        p.setClipRect(lay.plot);
        double start = 90.0 * 16.0;
        for (int i = 0; i < s.values.size(); ++i) {
            const double span = s.values.at(i) / sum * 360.0 * 16.0;
            p.setBrush(kPalette[i % 8]);
            p.setPen(QPen(Qt::white, 2));
            p.drawPie(QRectF(center.x() - radius, center.y() - radius, radius * 2, radius * 2), int(start),
                      int(-span));
            start -= span;
        }
        p.setClipping(false);
        if (showLegend)
            drawLegend(p, lay.legend, data, true);
        return;
    }

    p.fillRect(lay.plot, kPlotBg);
    p.setPen(kPlotBorder);
    p.drawRect(lay.plot);

    const double span = qMax(1e-9, axis.max - axis.min);

    if (m_chart.type == ChartObject::Bar)
        drawHorizontalValueAxis(p, lay, axis);
    else
        drawVerticalValueAxis(p, lay, axis);

    p.save();
    p.setClipRect(lay.plot);

    if (m_chart.type == ChartObject::Line) {
        for (int si = 0; si < seriesCount; ++si) {
            const ChartSeries &s = data.series.at(si);
            QPainterPath path;
            for (int i = 0; i < n; ++i) {
                const double x =
                    lay.plot.left() + (n > 1 ? (i + 0.5) * lay.plot.width() / n : lay.plot.width() / 2.0);
                const double y = lay.plot.bottom() - (s.values.at(i) - axis.min) / span * lay.plot.height();
                if (i == 0)
                    path.moveTo(x, y);
                else
                    path.lineTo(x, y);
            }
            p.setPen(QPen(kPalette[si % 8], 2.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.setBrush(Qt::NoBrush);
            p.drawPath(path);
            p.setBrush(kPalette[si % 8]);
            p.setPen(QPen(Qt::white, 1.5));
            for (int i = 0; i < n; ++i) {
                const double x =
                    lay.plot.left() + (n > 1 ? (i + 0.5) * lay.plot.width() / n : lay.plot.width() / 2.0);
                const double y = lay.plot.bottom() - (s.values.at(i) - axis.min) / span * lay.plot.height();
                p.drawEllipse(QPointF(x, y), 4, 4);
            }
        }
    } else if (m_chart.type == ChartObject::Bar) {
        for (int i = 0; i < n; ++i) {
            const int gy = lay.plot.top() + kGap + i * (lay.groupH + kGap);
            const int barH = qMax(4, lay.groupH - 4);
            const int y = gy + (lay.groupH - barH) / 2;
            for (int si = 0; si < seriesCount; ++si) {
                const double v = data.series.at(si).values.at(i);
                int w = int((v - axis.min) / span * lay.plot.width());
                if (v > axis.min + 1e-9 && w < 2)
                    w = 2;
                w = qBound(0, w, lay.plot.width());
                const QColor color = seriesCount == 1 ? kPalette[i % 8] : kPalette[si % 8];
                fillBar(p, QRect(lay.plot.left(), y, w, barH), color);
            }
        }
    } else {
        for (int i = 0; i < n; ++i) {
            const int gx = lay.plot.left() + kGap + i * (lay.groupW + kGap);
            const int barW = qMax(4, (lay.groupW - 2 * (seriesCount - 1)) / qMax(1, seriesCount));
            for (int si = 0; si < seriesCount; ++si) {
                const double v = data.series.at(si).values.at(i);
                int h = int((v - axis.min) / span * lay.plot.height());
                if (v > axis.min + 1e-9 && h < 2)
                    h = 2;
                h = qBound(0, h, lay.plot.height());
                const int x = gx + si * (barW + 2);
                const QColor color = seriesCount == 1 ? kPalette[i % 8] : kPalette[si % 8];
                fillBar(p, QRect(x, lay.plot.bottom() - h, barW, h), color);
            }
        }
    }

    p.restore();

    p.setPen(kAxisText);
    if (m_chart.type == ChartObject::Line || m_chart.type == ChartObject::Column) {
        for (int i = 0; i < n; ++i) {
            const int labelW = m_chart.type == ChartObject::Column ? lay.groupW
                : qMax(24, lay.plot.width() / qMax(1, n));
            const int cx = m_chart.type == ChartObject::Column
                ? lay.plot.left() + kGap + i * (lay.groupW + kGap) + lay.groupW / 2
                : int(lay.plot.left() + (n > 1 ? (i + 0.5) * lay.plot.width() / n : lay.plot.width() / 2.0));
            const QString lab = shortLabel(data.categories.at(i), labelW - 4, afm);
            p.drawText(cx - labelW / 2, lay.plot.bottom() + 4, labelW, kAxisBottom - 6, Qt::AlignHCenter | Qt::AlignTop,
                       lab);
        }
    } else if (m_chart.type == ChartObject::Bar) {
        for (int i = 0; i < n; ++i) {
            const int gy = lay.plot.top() + kGap + i * (lay.groupH + kGap);
            const QString lab = shortLabel(data.categories.at(i), lay.axisLeft - 8, afm);
            p.drawText(lay.body.left(), gy, lay.axisLeft - 6, lay.groupH, Qt::AlignRight | Qt::AlignVCenter, lab);
        }
    }

    if (showLegend)
        drawLegend(p, lay.legend, data, categoryLegend || m_chart.type == ChartObject::Pie);
}

int ChartWidget::groupW(int n, int plotWidth) const
{
    if (n <= 0)
        return 40;
    return qMax(12, (plotWidth - kGap * (n + 1)) / n);
}
