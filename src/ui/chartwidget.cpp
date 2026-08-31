#include "chartwidget.h"
#include "i18n.h"
#include "numformat.h"
#include "workbook.h"

#include <QContextMenuEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>
#include <numeric>

namespace {
constexpr int kTitleH = 22;
constexpr int kHandle = 14;
constexpr int kFrame = 1;
}

ChartWidget::ChartWidget(Workbook *wb, int sheetIndex, int chartIndex, ChartObject chart, QWidget *parent)
    : QWidget(parent)
    , m_wb(wb)
    , m_sheet(sheetIndex)
    , m_chartIndex(chartIndex)
    , m_chart(chart)
{
    setMinimumSize(140, 100);
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
    } else if (titleBarRect().contains(p) || rect().contains(p)) {
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

void ChartWidget::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
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
    p.drawText(titleBarRect().adjusted(6, 0, -6, 0), Qt::AlignVCenter | Qt::AlignLeft, title);

    p.fillRect(resizeHandleRect(), QColor(200, 200, 200));
    p.setPen(QColor(80, 80, 80));
    const QRect h = resizeHandleRect().adjusted(3, 3, -3, -3);
    p.drawLine(h.bottomLeft(), h.topRight());
    p.drawLine(h.left() + 4, h.bottom() - 1, h.right() - 1, h.top() + 4);

    if (!m_wb || m_sheet < 0 || m_sheet >= m_wb->sheetCount())
        return;

    const int r1 = qMin(m_chart.srcR1, m_chart.srcR2);
    const int r2 = qMax(m_chart.srcR1, m_chart.srcR2);
    const int c1 = qMin(m_chart.srcC1, m_chart.srcC2);
    const int c2 = qMax(m_chart.srcC1, m_chart.srcC2);
    const QRect area = rect().adjusted(8, kTitleH + 8, -10, -10);
    if (r2 <= r1 || c2 <= c1 || area.width() < 20 || area.height() < 20) {
        p.drawText(area, Qt::AlignCenter, QStringLiteral("—"));
        return;
    }

    QStringList labels;
    QVector<double> values;
    const int labelCol = c1;
    const int valueCol = c1 + 1;
    if (valueCol > c2) {
        p.drawText(area, Qt::AlignCenter, QStringLiteral("—"));
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
        p.drawText(area, Qt::AlignCenter, I18n::t("ui.chart_no_data"));
        return;
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
        return;
    }

    const double vmax = *std::max_element(values.cbegin(), values.cend());
    const double scale = vmax > 0 ? (area.height() - 16) / vmax : 1.0;
    const int n = values.size();
    const int gap = 4;

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
    } else if (m_chart.type == ChartObject::Bar) {
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
}
