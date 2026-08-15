#include "griddelegate.h"
#include "theme.h"

#include <QAbstractItemView>
#include <QItemSelection>
#include <QPainter>
#include <QPen>

GridDelegate::GridDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

void GridDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                         const QModelIndex &index) const
{
    QStyleOptionViewItem opt(option);
    opt.state &= ~QStyle::State_HasFocus;
    QStyledItemDelegate::paint(painter, opt, index);

    const auto *view = qobject_cast<const QAbstractItemView *>(opt.widget);
    if (!view)
        return;

    const auto pal = Theme::currentPalette();
    QColor border = pal.value(QStringLiteral("selectionBorder"));
    if (!border.isValid())
        border = pal.value(QStringLiteral("flesh"));

    if (view->currentIndex() == index) {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, false);
        QPen pen(border, 2);
        pen.setJoinStyle(Qt::MiterJoin);
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(opt.rect.adjusted(1, 1, -2, -2));
        painter->restore();
    }

    int br = -1;
    int bc = -1;
    if (view->selectionModel()) {
        for (const QItemSelectionRange &rng : view->selectionModel()->selection()) {
            br = qMax(br, rng.bottom());
            bc = qMax(bc, rng.right());
        }
    }
    if (br < 0) {
        const QModelIndex cur = view->currentIndex();
        br = cur.row();
        bc = cur.column();
    }
    if (index.row() == br && index.column() == bc) {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, false);
        const QRect handle(opt.rect.right() - 6, opt.rect.bottom() - 6, 9, 9);
        painter->fillRect(handle, border);
        QColor inner = pal.value(QStringLiteral("pith"));
        if (!inner.isValid())
            inner = QColor(Qt::white);
        painter->fillRect(handle.adjusted(2, 2, -2, -2), inner);
        painter->setPen(QPen(border, 1));
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(handle.adjusted(0, 0, -1, -1));
        painter->restore();
    }
}
