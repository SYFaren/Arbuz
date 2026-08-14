#include "sheetmodel.h"
#include "cellref.h"
#include "workbook.h"

#include <QBrush>
#include <QFont>

SheetModel::SheetModel(Workbook *workbook, int sheetIndex, QObject *parent)
    : QAbstractTableModel(parent)
    , m_wb(workbook)
    , m_sheet(sheetIndex)
{
    connect(m_wb, &Workbook::contentsChanged, this, &SheetModel::refresh);
    connect(m_wb, &Workbook::structureChanged, this, &SheetModel::refresh);
}

void SheetModel::setSheetIndex(int index)
{
    beginResetModel();
    m_sheet = index;
    endResetModel();
}

void SheetModel::refresh()
{
    beginResetModel();
    endResetModel();
}

int SheetModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid() || !m_wb || m_sheet >= m_wb->sheetCount())
        return 0;
    return m_wb->sheet(m_sheet).rowCount;
}

int SheetModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid() || !m_wb || m_sheet >= m_wb->sheetCount())
        return 0;
    return m_wb->sheet(m_sheet).colCount;
}

QVariant SheetModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || !m_wb || m_sheet >= m_wb->sheetCount())
        return {};
    const int r = index.row();
    const int c = index.column();
    const CellData cell = m_wb->sheet(m_sheet).cell(r, c);

    if (role == Qt::DisplayRole)
        return m_wb->displayText(m_sheet, r, c);
    if (role == Qt::EditRole || role == RawRole)
        return cell.raw;
    if (role == Qt::FontRole) {
        QFont f;
        f.setBold(cell.bold);
        f.setItalic(cell.italic);
        return f;
    }
    if (role == Qt::ForegroundRole && cell.foreground.isValid())
        return QBrush(cell.foreground);
    if (role == Qt::BackgroundRole && cell.background.isValid())
        return QBrush(cell.background);
    if (role == Qt::TextAlignmentRole) {
        Qt::Alignment h = Qt::AlignLeft;
        if (cell.hAlign == 2)
            h = Qt::AlignHCenter;
        else if (cell.hAlign == 3)
            h = Qt::AlignRight;
        else if (cell.hAlign == 0) {
            const QString d = m_wb->displayText(m_sheet, r, c);
            bool ok = false;
            d.toDouble(&ok);
            if (ok)
                h = Qt::AlignRight;
        }
        Qt::Alignment v = Qt::AlignVCenter;
        if (cell.vAlign == 1)
            v = Qt::AlignTop;
        else if (cell.vAlign == 2)
            v = Qt::AlignBottom;
        return int(h | v);
    }
    if (role == Qt::ToolTipRole && cell.raw.startsWith(QLatin1Char('=')))
        return cell.raw;
    return {};
}

bool SheetModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || role != Qt::EditRole || !m_wb)
        return false;
    m_wb->setRaw(m_sheet, index.row(), index.column(), value.toString());
    return true;
}

QVariant SheetModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
        return {};
    if (orientation == Qt::Horizontal)
        return CellRef::columnName(section);
    return section + 1;
}

Qt::ItemFlags SheetModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable;
}
