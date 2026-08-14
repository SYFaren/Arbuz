#ifndef ARBUZ_GRIDDELEGATE_H
#define ARBUZ_GRIDDELEGATE_H

#include <QStyledItemDelegate>

class GridDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit GridDelegate(QObject *parent = nullptr);
    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
};

#endif
