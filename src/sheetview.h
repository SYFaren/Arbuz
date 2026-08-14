#ifndef ARBUZ_SHEETVIEW_H
#define ARBUZ_SHEETVIEW_H

#include <QItemSelection>
#include <QModelIndex>
#include <QTableView>

class SheetView : public QTableView
{
    Q_OBJECT
public:
    explicit SheetView(QWidget *parent = nullptr);
    void setModel(QAbstractItemModel *model) override;
    QRect fillHandleRect() const;
    void setFrozen(int rows, int cols);

signals:
    void fillReleased(int sr1, int sc1, int sr2, int sc2, int er1, int ec1, int er2, int ec2);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void selectionChanged(const QItemSelection &selected, const QItemSelection &deselected) override;
    void resizeEvent(QResizeEvent *event) override;
    void scrollContentsBy(int dx, int dy) override;

private:
    bool isToggleModifier(Qt::KeyboardModifiers mods) const;
    bool wouldClearLastCell(const QModelIndex &idx) const;
    void ensureSelection();
    void collapseToCurrent();
    void updateFrozenGeometry();
    QPoint toViewport(const QPoint &widgetPos) const;
    QModelIndex fillHandleIndex() const;
    void selectionBounds(int *r1, int *c1, int *r2, int *c2) const;
    bool cellEmpty(int row, int col) const;
    QModelIndex jumpToEdge(int dRow, int dCol) const;
    QModelIndex lastUsedIndex() const;
    void selectFromAnchor(const QModelIndex &target, bool extend);

    bool m_ensuring = false;
    bool m_filling = false;
    QModelIndex m_fillFrom;
    QModelIndex m_selAnchor;
    int m_fillSr1 = 0;
    int m_fillSc1 = 0;
    int m_fillSr2 = 0;
    int m_fillSc2 = 0;
    int m_freezeRows = 0;
    int m_freezeCols = 0;
    QTableView *m_frozenCols = nullptr;
    QTableView *m_frozenRows = nullptr;
    QTableView *m_frozenCorner = nullptr;
};

#endif
