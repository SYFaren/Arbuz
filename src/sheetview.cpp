#include "sheetview.h"
#include "sheetmodel.h"
#include "workbook.h"

#include <QFocusEvent>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QScrollBar>

SheetView::SheetView(QWidget *parent)
    : QTableView(parent)
{
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setSelectionBehavior(QAbstractItemView::SelectItems);
}

void SheetView::setModel(QAbstractItemModel *model)
{
    QTableView::setModel(model);
    auto cloneView = [this](QTableView **slot) {
        if (*slot)
            return;
        auto *v = new QTableView(this);
        v->setModel(this->model());
        v->setFocusPolicy(Qt::NoFocus);
        v->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        v->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        v->setSelectionMode(NoSelection);
        v->horizontalHeader()->hide();
        v->verticalHeader()->hide();
        v->setFrameShape(QFrame::NoFrame);
        *slot = v;
    };
    cloneView(&m_frozenCols);
    cloneView(&m_frozenRows);
    cloneView(&m_frozenCorner);
    if (model && model->rowCount() > 0 && model->columnCount() > 0) {
        const QModelIndex a1 = model->index(0, 0);
        setCurrentIndex(a1);
        ensureSelection();
    }
    updateFrozenGeometry();
}

QRect SheetView::fillHandleRect() const
{
    const QModelIndex cur = fillHandleIndex();
    if (!cur.isValid())
        return {};
    const QRect r = visualRect(cur);
    return QRect(r.right() - 6, r.bottom() - 6, 8, 8);
}

QModelIndex SheetView::fillHandleIndex() const
{
    if (!model())
        return currentIndex();
    int r1 = 0, c1 = 0, r2 = -1, c2 = -1;
    selectionBounds(&r1, &c1, &r2, &c2);
    if (r2 < 0)
        return currentIndex();
    return model()->index(r2, c2);
}

void SheetView::selectionBounds(int *r1, int *c1, int *r2, int *c2) const
{
    *r1 = *c1 = 0;
    *r2 = *c2 = -1;
    if (!selectionModel()) {
        const QModelIndex cur = currentIndex();
        if (cur.isValid()) {
            *r1 = *r2 = cur.row();
            *c1 = *c2 = cur.column();
        }
        return;
    }
    const QItemSelection sel = selectionModel()->selection();
    if (sel.isEmpty()) {
        const QModelIndex cur = currentIndex();
        if (cur.isValid()) {
            *r1 = *r2 = cur.row();
            *c1 = *c2 = cur.column();
        }
        return;
    }
    *r1 = sel.first().top();
    *c1 = sel.first().left();
    *r2 = sel.first().bottom();
    *c2 = sel.first().right();
    for (const QItemSelectionRange &rng : sel) {
        *r1 = qMin(*r1, rng.top());
        *c1 = qMin(*c1, rng.left());
        *r2 = qMax(*r2, rng.bottom());
        *c2 = qMax(*c2, rng.right());
    }
}

void SheetView::setFrozen(int rows, int cols)
{
    m_freezeRows = qMax(0, rows);
    m_freezeCols = qMax(0, cols);
    updateFrozenGeometry();
}

QPoint SheetView::toViewport(const QPoint &widgetPos) const
{
    return widgetPos - QPoint(viewport()->x(), viewport()->y());
}

void SheetView::updateFrozenGeometry()
{
    if (!model() || !m_frozenCols)
        return;
    const bool any = m_freezeRows > 0 || m_freezeCols > 0;
    m_frozenCols->setVisible(any && m_freezeCols > 0);
    m_frozenRows->setVisible(any && m_freezeRows > 0);
    m_frozenCorner->setVisible(m_freezeRows > 0 && m_freezeCols > 0);
    if (!any)
        return;

    const int x = verticalHeader()->width();
    const int y = horizontalHeader()->height();
    int w = 0;
    for (int c = 0; c < m_freezeCols && c < model()->columnCount(); ++c)
        w += columnWidth(c);
    int h = 0;
    for (int r = 0; r < m_freezeRows && r < model()->rowCount(); ++r)
        h += rowHeight(r);

    if (m_freezeCols > 0) {
        m_frozenCols->move(x, y + h);
        m_frozenCols->resize(w, viewport()->height() - h);
        m_frozenCols->verticalScrollBar()->setValue(verticalScrollBar()->value());
        for (int c = 0; c < model()->columnCount(); ++c)
            m_frozenCols->setColumnHidden(c, c >= m_freezeCols);
        for (int r = 0; r < model()->rowCount(); ++r)
            m_frozenCols->setRowHidden(r, r < m_freezeRows);
    }
    if (m_freezeRows > 0) {
        m_frozenRows->move(x + w, y);
        m_frozenRows->resize(viewport()->width() - w, h);
        m_frozenRows->horizontalScrollBar()->setValue(horizontalScrollBar()->value());
        for (int r = 0; r < model()->rowCount(); ++r)
            m_frozenRows->setRowHidden(r, r >= m_freezeRows);
        for (int c = 0; c < model()->columnCount(); ++c)
            m_frozenRows->setColumnHidden(c, c < m_freezeCols);
    }
    if (m_freezeRows > 0 && m_freezeCols > 0) {
        m_frozenCorner->move(x, y);
        m_frozenCorner->resize(w, h);
        for (int r = 0; r < model()->rowCount(); ++r)
            m_frozenCorner->setRowHidden(r, r >= m_freezeRows);
        for (int c = 0; c < model()->columnCount(); ++c)
            m_frozenCorner->setColumnHidden(c, c >= m_freezeCols);
    }
}

void SheetView::resizeEvent(QResizeEvent *event)
{
    QTableView::resizeEvent(event);
    updateFrozenGeometry();
}

void SheetView::scrollContentsBy(int dx, int dy)
{
    QTableView::scrollContentsBy(dx, dy);
    if (m_frozenCols)
        m_frozenCols->verticalScrollBar()->setValue(verticalScrollBar()->value());
    if (m_frozenRows)
        m_frozenRows->horizontalScrollBar()->setValue(horizontalScrollBar()->value());
    updateFrozenGeometry();
}

bool SheetView::isToggleModifier(Qt::KeyboardModifiers mods) const
{
    return mods & (Qt::ControlModifier | Qt::MetaModifier);
}

bool SheetView::wouldClearLastCell(const QModelIndex &idx) const
{
    if (!idx.isValid() || !selectionModel())
        return false;
    return selectionModel()->isSelected(idx) && selectionModel()->selectedIndexes().size() <= 1;
}

void SheetView::ensureSelection()
{
    if (m_ensuring || !selectionModel() || !model())
        return;
    if (selectionModel()->hasSelection())
        return;
    QModelIndex cur = currentIndex();
    if (!cur.isValid())
        cur = model()->index(0, 0);
    if (!cur.isValid())
        return;
    m_ensuring = true;
    selectionModel()->select(cur, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Current);
    setCurrentIndex(cur);
    m_ensuring = false;
}

void SheetView::collapseToCurrent()
{
    if (!selectionModel())
        return;
    QModelIndex cur = currentIndex();
    if (!cur.isValid() && model())
        cur = model()->index(0, 0);
    if (!cur.isValid())
        return;
    selectionModel()->select(cur, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Current);
    setCurrentIndex(cur);
}

void SheetView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && fillHandleRect().contains(toViewport(event->pos()))) {
        m_filling = true;
        m_fillFrom = fillHandleIndex();
        selectionBounds(&m_fillSr1, &m_fillSc1, &m_fillSr2, &m_fillSc2);
        if (m_fillSr2 < 0 && currentIndex().isValid()) {
            m_fillSr1 = m_fillSr2 = currentIndex().row();
            m_fillSc1 = m_fillSc2 = currentIndex().column();
        }
        event->accept();
        return;
    }
    if (isToggleModifier(event->modifiers())) {
        const QModelIndex idx = indexAt(event->pos());
        if (wouldClearLastCell(idx)) {
            event->accept();
            return;
        }
    }
    QTableView::mousePressEvent(event);
    ensureSelection();
    if (!(event->modifiers() & Qt::ShiftModifier))
        m_selAnchor = currentIndex();
}

void SheetView::mouseMoveEvent(QMouseEvent *event)
{
    if (m_filling && model()) {
        const QModelIndex idx = indexAt(event->pos());
        if (idx.isValid() && selectionModel()) {
            const int er1 = qMin(m_fillSr1, idx.row());
            const int er2 = qMax(m_fillSr2, idx.row());
            const int ec1 = qMin(m_fillSc1, idx.column());
            const int ec2 = qMax(m_fillSc2, idx.column());
            QItemSelection sel(model()->index(er1, ec1), model()->index(er2, ec2));
            selectionModel()->select(sel, QItemSelectionModel::ClearAndSelect);
        }
        event->accept();
        return;
    }
    QTableView::mouseMoveEvent(event);
}

void SheetView::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_filling) {
        m_filling = false;
        const QModelIndex to = indexAt(event->pos());
        if (m_fillSr2 >= 0 && to.isValid()) {
            const int er1 = qMin(m_fillSr1, to.row());
            const int er2 = qMax(m_fillSr2, to.row());
            const int ec1 = qMin(m_fillSc1, to.column());
            const int ec2 = qMax(m_fillSc2, to.column());
            emit fillReleased(m_fillSr1, m_fillSc1, m_fillSr2, m_fillSc2, er1, ec1, er2, ec2);
        }
        event->accept();
        ensureSelection();
        return;
    }
    if (isToggleModifier(event->modifiers())) {
        const QModelIndex idx = indexAt(event->pos());
        if (wouldClearLastCell(idx)) {
            event->accept();
            ensureSelection();
            return;
        }
    }
    QTableView::mouseReleaseEvent(event);
    ensureSelection();
}

void SheetView::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape && state() != EditingState) {
        collapseToCurrent();
        event->accept();
        return;
    }
    const bool ctrl = event->modifiers() & (Qt::ControlModifier | Qt::MetaModifier);
    if (ctrl && state() != EditingState) {
        int dRow = 0;
        int dCol = 0;
        switch (event->key()) {
        case Qt::Key_Left:
            dCol = -1;
            break;
        case Qt::Key_Right:
            dCol = 1;
            break;
        case Qt::Key_Up:
            dRow = -1;
            break;
        case Qt::Key_Down:
            dRow = 1;
            break;
        default:
            break;
        }
        if (dRow != 0 || dCol != 0) {
            const QModelIndex target = jumpToEdge(dRow, dCol);
            selectFromAnchor(target, event->modifiers() & Qt::ShiftModifier);
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_Home) {
            selectFromAnchor(model() ? model()->index(0, 0) : QModelIndex(),
                             event->modifiers() & Qt::ShiftModifier);
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_End) {
            selectFromAnchor(lastUsedIndex(), event->modifiers() & Qt::ShiftModifier);
            event->accept();
            return;
        }
    }
    QTableView::keyPressEvent(event);
    ensureSelection();
    if (!(event->modifiers() & Qt::ShiftModifier))
        m_selAnchor = currentIndex();
}

bool SheetView::cellEmpty(int row, int col) const
{
    if (!model())
        return true;
    const QModelIndex idx = model()->index(row, col);
    if (!idx.isValid())
        return true;
    return idx.data(Qt::EditRole).toString().trimmed().isEmpty();
}

QModelIndex SheetView::jumpToEdge(int dRow, int dCol) const
{
    if (!model())
        return currentIndex();
    const int maxR = model()->rowCount() - 1;
    const int maxC = model()->columnCount() - 1;
    QModelIndex cur = currentIndex();
    if (!cur.isValid())
        cur = model()->index(0, 0);
    int r = cur.row();
    int c = cur.column();
    auto inb = [&](int rr, int cc) { return rr >= 0 && cc >= 0 && rr <= maxR && cc <= maxC; };
    const int nr = r + dRow;
    const int nc = c + dCol;
    if (!inb(nr, nc))
        return cur;

    if (cellEmpty(r, c) || cellEmpty(nr, nc)) {
        r = nr;
        c = nc;
        while (inb(r + dRow, c + dCol) && cellEmpty(r, c)) {
            r += dRow;
            c += dCol;
        }
    } else {
        while (inb(r + dRow, c + dCol) && !cellEmpty(r + dRow, c + dCol)) {
            r += dRow;
            c += dCol;
        }
    }
    return model()->index(r, c);
}

QModelIndex SheetView::lastUsedIndex() const
{
    if (!model())
        return currentIndex();
    int r = 0;
    int c = 0;
    if (auto *m = qobject_cast<SheetModel *>(model())) {
        if (m->workbook() && m->workbook()->usedCorner(m->sheetIndex(), &r, &c))
            return model()->index(r, c);
    }
    return model()->index(0, 0);
}

void SheetView::selectFromAnchor(const QModelIndex &target, bool extend)
{
    if (!target.isValid() || !selectionModel())
        return;
    if (!extend || !m_selAnchor.isValid()) {
        m_selAnchor = target;
        selectionModel()->select(target, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Current);
        setCurrentIndex(target);
        scrollTo(target);
        return;
    }
    QItemSelection sel(m_selAnchor, target);
    selectionModel()->select(sel, QItemSelectionModel::ClearAndSelect);
    setCurrentIndex(target);
    scrollTo(target);
}

void SheetView::focusInEvent(QFocusEvent *event)
{
    QTableView::focusInEvent(event);
    ensureSelection();
}

void SheetView::selectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    QTableView::selectionChanged(selected, deselected);
    ensureSelection();
}
