#ifndef ARBUZ_SHEETMODEL_H
#define ARBUZ_SHEETMODEL_H

#include <QAbstractTableModel>
#include <QColor>

class Workbook;

class SheetModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum Roles { RawRole = Qt::UserRole + 1 };

    explicit SheetModel(Workbook *workbook, int sheetIndex, QObject *parent = nullptr);

    void setSheetIndex(int index);
    int sheetIndex() const { return m_sheet; }
    Workbook *workbook() const { return m_wb; }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    void refresh();

private:
    Workbook *m_wb = nullptr;
    int m_sheet = 0;
};

#endif
