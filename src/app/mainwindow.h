#ifndef ARBUZ_MAINWINDOW_H
#define ARBUZ_MAINWINDOW_H

#include <QMainWindow>
#include <QModelIndex>

#include "chart.h"

class Workbook;
class SheetModel;
class SheetView;
class ChartWidget;
class QTabBar;
class QToolButton;
class QActionGroup;
class QMenu;
class QPoint;
class QLineEdit;
class QToolBar;

class QResizeEvent;
class QShowEvent;
class QEvent;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    bool openPath(const QString &path);
    void openDemo();

protected:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

private slots:
    void newFile();
    void openFile();
    bool saveFile();
    bool saveFileAs();
    void printSheet();
    void copy();
    void paste();
    void cut();
    void clearContents();
    void findCell();
    void findNextCell();
    void findPrevCell();
    void replaceCell();
    void goToCell();
    void toggleBold();
    void toggleItalic();
    void setFillColor();
    void setTextColor();
    void addSheet();
    void removeSheet();
    void renameSheet();
    void showSettings();
    void showFirstRun();
    void showCredits();
    void showAbout();
    void showFunctionDialog();
    void onSheetChanged(int index);
    void onSelectionChanged();
    void cellContextMenu(const QPoint &pos);
    void sheetTabContextMenu(const QPoint &pos);
    void insertRows();
    void insertColumns();
    void removeRows();
    void removeColumns();
    void freezePanes();
    void unfreezePanes();
    void sortAsc();
    void sortDesc();
    void mergeSelection();
    void unmergeSelection();
    void alignLeft();
    void alignCenter();
    void alignRight();
    void toggleWrap();
    void setNumFmtGeneral();
    void setNumFmtNumber();
    void setNumFmtThousands();
    void setNumFmtPercent();
    void setNumFmtPercent2();
    void setNumFmtScientific();
    void setNumFmtDate();
    void setNumFmtDateTime();
    void setNumFmtTime();
    void setNumFmtCurrencyRub();
    void setNumFmtCurrencyUsd();
    void setNumFmtCurrencyEur();
    void toggleBorder();
    void formulaBarCommit();
    void cycleFormulaReference();
    void toggleAutoFilter();
    void filterColumn();
    void clearAutoFilter();
    void insertColumnChart();
    void insertBarChart();
    void insertLineChart();
    void insertPieChart();
    void nameBoxCommit();
    void fillDown();
    void fillRight();
    void onFillReleased(int sr1, int sc1, int sr2, int sc2, int er1, int ec1, int er2, int ec2);
    void onTabMoved(int from, int to);

private:
    void rebuildSheetTabs();
    void switchToSheet(int index);
    void saveColumnWidths();
    void restoreColumnWidths();
    void saveRowHeights();
    void restoreRowHeights();
    void setupSheetView();
    void applyMerges();
    void applyFilterVisibility();
    void rebuildCharts();
    void repositionCharts();
    void syncChartGeometry(int chartIndex);
    void insertChart(ChartObject::Type type);
    QMenu *functionMenu(QWidget *parent);
    void insertFunction(const QString &name);
    QString selectedRangeA1() const;
    void selectedBounds(int *r1, int *c1, int *r2, int *c2) const;
    void updateStatus();
    void updateFormulaBar();
    int currentSheet() const;
    bool confirmSave();
    void setCurrentPath(const QString &path);
    void applyScale();
    void syncThemeMenu();
    void rebuildThemeMenu();
    void applyStyleToSelection(bool toggleBold, bool toggleItalic, bool setFg, const QColor &fg,
                               bool setBg, const QColor &bg);
    void applyAlign(int hAlign);
    void applyNumFmt(int fmt);
    void revealCell(int row, int col);

    Workbook *m_wb = nullptr;
    SheetModel *m_model = nullptr;
    SheetView *m_view = nullptr;
    QTabBar *m_tabs = nullptr;
    QToolButton *m_addSheetBtn = nullptr;
    QActionGroup *m_themeGroup = nullptr;
    QMenu *m_themeMenu = nullptr;
    QLineEdit *m_formulaBar = nullptr;
    QLineEdit *m_nameBox = nullptr;
    QToolBar *m_toolbar = nullptr;
    QString m_path;
    bool m_dirty = false;
    bool m_syncingFormula = false;
    int m_copyRow = 0;
    int m_copyCol = 0;
    bool m_copyHasOrigin = false;
    QString m_findNeedle;
    int m_findSheet = 0;
    QVector<ChartWidget *> m_chartWidgets;
};

#endif
