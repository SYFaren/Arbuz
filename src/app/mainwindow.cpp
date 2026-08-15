#include "mainwindow.h"
#include "appsettings.h"
#include "arbuzicon.h"
#include "cellref.h"
#include "creditsdialog.h"
#include "fileio.h"
#include "firstrunwizard.h"
#include "formulaengine.h"
#include "functiondialog.h"
#include "griddelegate.h"
#include "i18n.h"
#include "numformat.h"
#include "pluginhost.h"
#include "settingsdialog.h"
#include "sheetmodel.h"
#include "sheetview.h"
#include "theme.h"
#include "workbook.h"

#include <QAction>
#include <QJsonObject>
#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QColorDialog>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QItemSelection>
#include <QItemSelectionModel>
#include <QKeySequence>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSize>
#include <QStatusBar>
#include <QTabBar>
#include <QTableView>
#include <QToolBar>
#include <QToolButton>
#include <QUndoStack>
#include <QVBoxLayout>
#include <QPainter>
#include <QPrintDialog>
#include <QPrinter>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_wb(new Workbook(this))
{
    setWindowTitle(QStringLiteral("Arbuz"));
    setWindowIcon(ArbuzIcon::app());
    resize(1100, 720);
    applyScale();

    menuBar()->setNativeMenuBar(false);
    menuBar()->setVisible(true);
    menuBar()->setObjectName(QStringLiteral("mainMenuBar"));

    auto *central = new QWidget(this);
    auto *lay = new QVBoxLayout(central);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    auto *formulaRow = new QWidget(central);
    formulaRow->setObjectName(QStringLiteral("formulaRow"));
    formulaRow->setAttribute(Qt::WA_StyledBackground, true);
    auto *fl = new QHBoxLayout(formulaRow);
    fl->setContentsMargins(6, 3, 6, 3);
    fl->setSpacing(6);
    m_nameBox = new QLineEdit(formulaRow);
    m_nameBox->setObjectName(QStringLiteral("nameBox"));
    m_nameBox->setMaximumWidth(88);
    m_nameBox->setText(QStringLiteral("A1"));
    connect(m_nameBox, &QLineEdit::returnPressed, this, &MainWindow::nameBoxCommit);
    m_formulaBar = new QLineEdit(formulaRow);
    m_formulaBar->setObjectName(QStringLiteral("formulaBar"));
    connect(m_formulaBar, &QLineEdit::returnPressed, this, &MainWindow::formulaBarCommit);
    fl->addWidget(m_nameBox);
    fl->addWidget(m_formulaBar, 1);
    lay->addWidget(formulaRow);

    m_view = new SheetView(central);
    m_model = new SheetModel(m_wb, 0, m_view);
    setupSheetView();
    lay->addWidget(m_view, 1);

    auto *sheetBar = new QWidget(central);
    sheetBar->setObjectName(QStringLiteral("sheetBar"));
    sheetBar->setAttribute(Qt::WA_StyledBackground, true);
    auto *sl = new QHBoxLayout(sheetBar);
    sl->setContentsMargins(6, 4, 6, 4);
    sl->setSpacing(4);

    m_addSheetBtn = new QToolButton(sheetBar);
    m_addSheetBtn->setObjectName(QStringLiteral("addSheetButton"));
    m_addSheetBtn->setIcon(ArbuzIcon::addSheet());
    m_addSheetBtn->setIconSize(QSize(14, 14));
    m_addSheetBtn->setText(QString());
    m_addSheetBtn->setAccessibleName(QStringLiteral("+"));
    m_addSheetBtn->setToolTip(I18n::t("ui.add_sheet"));
    m_addSheetBtn->setAutoRaise(true);
    m_addSheetBtn->setFocusPolicy(Qt::NoFocus);
    m_addSheetBtn->setFixedSize(24, 22);
    connect(m_addSheetBtn, &QToolButton::clicked, this, &MainWindow::addSheet);

    m_tabs = new QTabBar(sheetBar);
    m_tabs->setObjectName(QStringLiteral("sheetTabs"));
    m_tabs->setDocumentMode(true);
    m_tabs->setExpanding(false);
    m_tabs->setMovable(true);
    m_tabs->setDrawBase(false);
    m_tabs->setUsesScrollButtons(true);
    m_tabs->setTabsClosable(false);
    m_tabs->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tabs->setElideMode(Qt::ElideNone);
    connect(m_tabs, &QTabBar::currentChanged, this, &MainWindow::onSheetChanged);
    connect(m_tabs, &QTabBar::tabBarDoubleClicked, this, [this](int index) {
        if (index >= 0)
            m_tabs->setCurrentIndex(index);
        renameSheet();
    });
    connect(m_tabs, &QTabBar::customContextMenuRequested, this, &MainWindow::sheetTabContextMenu);
    connect(m_tabs, &QTabBar::tabMoved, this, &MainWindow::onTabMoved);

    sl->addWidget(m_addSheetBtn);
    sl->addWidget(m_tabs, 1);
    lay->addWidget(sheetBar);
    setCentralWidget(central);

    auto iconize = [](QAction *act, const QString &id) {
        act->setIcon(ArbuzIcon::named(id));
        act->setIconVisibleInMenu(true);
        return act;
    };

    auto *fileMenu = menuBar()->addMenu(I18n::t("ui.file"));
    iconize(fileMenu->addAction(I18n::t("ui.new"), QKeySequence::New, this, &MainWindow::newFile),
            QStringLiteral("new"));
    iconize(fileMenu->addAction(I18n::t("ui.open"), QKeySequence::Open, this, &MainWindow::openFile),
            QStringLiteral("open"));
    iconize(fileMenu->addAction(I18n::t("ui.save"), QKeySequence::Save, this, &MainWindow::saveFile),
            QStringLiteral("save"));
    iconize(fileMenu->addAction(I18n::t("ui.save_as"), QKeySequence::SaveAs, this, &MainWindow::saveFileAs),
            QStringLiteral("save-as"));
    iconize(fileMenu->addAction(I18n::t("ui.print"), QKeySequence::Print, this, &MainWindow::printSheet),
            QStringLiteral("save"));
    fileMenu->addSeparator();
    iconize(fileMenu->addAction(I18n::t("ui.quit"), QKeySequence::Quit, this, &QWidget::close),
            QStringLiteral("quit"));

    auto *editMenu = menuBar()->addMenu(I18n::t("ui.edit"));
    QAction *undoAct = m_wb->undoStack()->createUndoAction(this, I18n::t("ui.undo"));
    undoAct->setShortcut(QKeySequence::Undo);
    undoAct->setIcon(ArbuzIcon::named(QStringLiteral("clear")));
    undoAct->setIconVisibleInMenu(true);
    editMenu->addAction(undoAct);
    QAction *redoAct = m_wb->undoStack()->createRedoAction(this, I18n::t("ui.redo"));
    redoAct->setShortcut(QKeySequence::Redo);
    redoAct->setIcon(ArbuzIcon::named(QStringLiteral("clear")));
    redoAct->setIconVisibleInMenu(true);
    editMenu->addAction(redoAct);
    editMenu->addSeparator();
    iconize(editMenu->addAction(I18n::t("ui.cut"), QKeySequence::Cut, this, &MainWindow::cut),
            QStringLiteral("cut"));
    iconize(editMenu->addAction(I18n::t("ui.copy"), QKeySequence::Copy, this, &MainWindow::copy),
            QStringLiteral("copy"));
    iconize(editMenu->addAction(I18n::t("ui.paste"), QKeySequence::Paste, this, &MainWindow::paste),
            QStringLiteral("paste"));
    iconize(editMenu->addAction(I18n::t("ui.clear"), QKeySequence::Delete, this, &MainWindow::clearContents),
            QStringLiteral("clear"));
    editMenu->addSeparator();
    iconize(editMenu->addAction(I18n::t("ui.fill_down"), QKeySequence(QStringLiteral("Ctrl+D")), this,
                               &MainWindow::fillDown),
            QStringLiteral("goto"));
    iconize(editMenu->addAction(I18n::t("ui.fill_right"), QKeySequence(QStringLiteral("Ctrl+R")), this,
                               &MainWindow::fillRight),
            QStringLiteral("goto"));
    editMenu->addSeparator();
    iconize(editMenu->addAction(I18n::t("ui.find"), QKeySequence::Find, this, &MainWindow::findCell),
            QStringLiteral("find"));
    iconize(editMenu->addAction(I18n::t("ui.find_next"), QKeySequence(QStringLiteral("F3")), this,
                               &MainWindow::findNextCell),
            QStringLiteral("find"));
    iconize(editMenu->addAction(I18n::t("ui.find_previous"), QKeySequence(QStringLiteral("Shift+F3")), this,
                               &MainWindow::findPrevCell),
            QStringLiteral("find"));
    iconize(editMenu->addAction(I18n::t("ui.replace"), QKeySequence::Replace, this, &MainWindow::replaceCell),
            QStringLiteral("find"));
    iconize(editMenu->addAction(I18n::t("ui.go_to_cell"), QKeySequence(QStringLiteral("Ctrl+G")),
                               this, &MainWindow::goToCell),
            QStringLiteral("goto"));
    editMenu->addSeparator();
    iconize(editMenu->addAction(I18n::t("ui.insert_rows"), this, &MainWindow::insertRows),
            QStringLiteral("add-sheet"));
    iconize(editMenu->addAction(I18n::t("ui.insert_columns"), this, &MainWindow::insertColumns),
            QStringLiteral("add-sheet"));
    iconize(editMenu->addAction(I18n::t("ui.delete_rows"), this, &MainWindow::removeRows),
            QStringLiteral("delete-sheet"));
    iconize(editMenu->addAction(I18n::t("ui.delete_columns"), this, &MainWindow::removeColumns),
            QStringLiteral("delete-sheet"));

    auto *insertMenu = menuBar()->addMenu(I18n::t("ui.insert"));
    iconize(insertMenu->addAction(I18n::t("ui.function"), this, &MainWindow::showFunctionDialog),
            QStringLiteral("fx"));
    insertMenu->addSeparator();
    for (const QString &name : QStringList{QStringLiteral("SUM"), QStringLiteral("AVERAGE"),
                                           QStringLiteral("IF"), QStringLiteral("IFERROR"),
                                           QStringLiteral("COUNTIF"), QStringLiteral("SUMIF"),
                                           QStringLiteral("VLOOKUP"), QStringLiteral("CONCAT"),
                                           QStringLiteral("TODAY")}) {
        auto *act = insertMenu->addAction(ArbuzIcon::named(QStringLiteral("fx")), name);
        act->setIconVisibleInMenu(true);
        connect(act, &QAction::triggered, this, [this, name]() { insertFunction(name); });
    }

    auto *fmtMenu = menuBar()->addMenu(I18n::t("ui.format"));
    iconize(fmtMenu->addAction(I18n::t("ui.bold"), QKeySequence::Bold, this, &MainWindow::toggleBold),
            QStringLiteral("bold"));
    iconize(fmtMenu->addAction(I18n::t("ui.italic"), QKeySequence::Italic, this, &MainWindow::toggleItalic),
            QStringLiteral("italic"));
    iconize(fmtMenu->addAction(I18n::t("ui.text_color"), this, &MainWindow::setTextColor),
            QStringLiteral("text-color"));
    iconize(fmtMenu->addAction(I18n::t("ui.fill_color"), this, &MainWindow::setFillColor),
            QStringLiteral("fill-color"));
    fmtMenu->addSeparator();
    iconize(fmtMenu->addAction(I18n::t("ui.align_left"), this, &MainWindow::alignLeft),
            QStringLiteral("goto"));
    iconize(fmtMenu->addAction(I18n::t("ui.align_center"), this, &MainWindow::alignCenter),
            QStringLiteral("goto"));
    iconize(fmtMenu->addAction(I18n::t("ui.align_right"), this, &MainWindow::alignRight),
            QStringLiteral("goto"));
    iconize(fmtMenu->addAction(I18n::t("ui.wrap_text"), this, &MainWindow::toggleWrap),
            QStringLiteral("italic"));
    iconize(fmtMenu->addAction(I18n::t("ui.borders"), this, &MainWindow::toggleBorder),
            QStringLiteral("fill-color"));
    fmtMenu->addSeparator();
    auto *numMenu = fmtMenu->addMenu(I18n::t("ui.number_format"));
    iconize(numMenu->addAction(I18n::t("ui.general"), this, &MainWindow::setNumFmtGeneral),
            QStringLiteral("fx"));
    iconize(numMenu->addAction(I18n::t("ui.number_0_00"), this, &MainWindow::setNumFmtNumber),
            QStringLiteral("fx"));
    iconize(numMenu->addAction(I18n::t("ui.thousands_0_00"), this,
                               &MainWindow::setNumFmtThousands),
            QStringLiteral("fx"));
    iconize(numMenu->addAction(I18n::t("ui.percent"), this, &MainWindow::setNumFmtPercent),
            QStringLiteral("fx"));
    iconize(numMenu->addAction(I18n::t("ui.percent_0_00"), this, &MainWindow::setNumFmtPercent2),
            QStringLiteral("fx"));
    iconize(numMenu->addAction(I18n::t("ui.scientific"), this, &MainWindow::setNumFmtScientific),
            QStringLiteral("fx"));
    iconize(numMenu->addAction(I18n::t("ui.date"), this, &MainWindow::setNumFmtDate), QStringLiteral("fx"));
    iconize(numMenu->addAction(I18n::t("ui.date_and_time"), this, &MainWindow::setNumFmtDateTime),
            QStringLiteral("fx"));
    iconize(numMenu->addAction(I18n::t("ui.time"), this, &MainWindow::setNumFmtTime), QStringLiteral("fx"));
    fmtMenu->addSeparator();
    iconize(fmtMenu->addAction(I18n::t("ui.merge_cells"), this, &MainWindow::mergeSelection),
            QStringLiteral("add-sheet"));
    iconize(fmtMenu->addAction(I18n::t("ui.unmerge"), this, &MainWindow::unmergeSelection),
            QStringLiteral("delete-sheet"));

    auto *sheetMenu = menuBar()->addMenu(I18n::t("ui.sheet"));
    iconize(sheetMenu->addAction(I18n::t("ui.insert_sheet"), this, &MainWindow::addSheet),
            QStringLiteral("add-sheet"));
    iconize(sheetMenu->addAction(I18n::t("ui.delete_sheet"), this, &MainWindow::removeSheet),
            QStringLiteral("delete-sheet"));
    iconize(sheetMenu->addAction(I18n::t("ui.rename_sheet"), this, &MainWindow::renameSheet),
            QStringLiteral("rename-sheet"));

    auto *viewMenu = menuBar()->addMenu(I18n::t("ui.view"));
    m_themeMenu = viewMenu->addMenu(ArbuzIcon::named(QStringLiteral("theme")), I18n::t("ui.theme"));
    m_themeGroup = new QActionGroup(this);
    m_themeGroup->setExclusive(true);
    rebuildThemeMenu();
    viewMenu->addSeparator();
    iconize(viewMenu->addAction(I18n::t("ui.freeze_panes"), this, &MainWindow::freezePanes),
            QStringLiteral("goto"));
    iconize(viewMenu->addAction(I18n::t("ui.unfreeze_panes"), this, &MainWindow::unfreezePanes),
            QStringLiteral("goto"));
    iconize(viewMenu->addAction(I18n::t("ui.sort_a_z"), this, &MainWindow::sortAsc),
            QStringLiteral("goto"));
    iconize(viewMenu->addAction(I18n::t("ui.sort_z_a"), this, &MainWindow::sortDesc),
            QStringLiteral("goto"));
    viewMenu->addSeparator();
    iconize(viewMenu->addAction(I18n::t("ui.settings"), this, &MainWindow::showSettings),
            QStringLiteral("settings"));

    auto *plugMenu = menuBar()->addMenu(I18n::t("ui.plugins"));
    plugMenu->setObjectName(QStringLiteral("pluginsMenu"));
    connect(plugMenu, &QMenu::aboutToShow, this, [plugMenu]() { PluginHost::instance().fillMenu(plugMenu); });

    auto *helpMenu = menuBar()->addMenu(I18n::t("ui.help"));
    iconize(helpMenu->addAction(I18n::t("ui.credits"), this, &MainWindow::showCredits),
            QStringLiteral("credits"));
    iconize(helpMenu->addAction(I18n::t("ui.first_run_setup"), this, &MainWindow::showFirstRun),
            QStringLiteral("wizard"));
    iconize(helpMenu->addAction(I18n::t("ui.about"), this, &MainWindow::showAbout),
            QStringLiteral("about"));

    m_toolbar = addToolBar(I18n::t("ui.main"));
    m_toolbar->setObjectName(QStringLiteral("mainToolBar"));
    m_toolbar->setMovable(false);
    m_toolbar->setIconSize(QSize(20, 20));
    m_toolbar->addAction(ArbuzIcon::named(QStringLiteral("new")), I18n::t("ui.new"), this, &MainWindow::newFile);
    m_toolbar->addAction(ArbuzIcon::named(QStringLiteral("open")), I18n::t("ui.open_2"), this, &MainWindow::openFile);
    m_toolbar->addAction(ArbuzIcon::named(QStringLiteral("save")), I18n::t("ui.save"), this, &MainWindow::saveFile);
    m_toolbar->addSeparator();
    m_toolbar->addAction(undoAct);
    m_toolbar->addAction(redoAct);
    m_toolbar->addSeparator();
    m_toolbar->addAction(ArbuzIcon::named(QStringLiteral("cut")), I18n::t("ui.cut"), this, &MainWindow::cut);
    m_toolbar->addAction(ArbuzIcon::named(QStringLiteral("copy")), I18n::t("ui.copy"), this, &MainWindow::copy);
    m_toolbar->addAction(ArbuzIcon::named(QStringLiteral("paste")), I18n::t("ui.paste"), this, &MainWindow::paste);
    m_toolbar->addSeparator();
    m_toolbar->addAction(ArbuzIcon::named(QStringLiteral("bold")), I18n::t("ui.bold"), this, &MainWindow::toggleBold);
    m_toolbar->addAction(ArbuzIcon::named(QStringLiteral("italic")), I18n::t("ui.italic"), this, &MainWindow::toggleItalic);
    m_toolbar->addSeparator();
    m_toolbar->addAction(ArbuzIcon::named(QStringLiteral("fx")), QStringLiteral("SUM"), this, [this]() {
        insertFunction(QStringLiteral("SUM"));
    });

    statusBar()->setSizeGripEnabled(true);
    updateStatus();

    connect(m_wb, &Workbook::contentsChanged, this, [this]() {
        m_dirty = true;
        updateStatus();
    });
    connect(m_wb, &Workbook::structureChanged, this, &MainWindow::rebuildSheetTabs);
    connect(m_wb, &Workbook::cellEdited, this, [](int sh, int r, int c) {
        PluginHost::instance().notifyCellEdited(sh, r, c);
    });

    connect(&PluginHost::instance(), &PluginHost::requestSheetChange, this, [this](int index) {
        if (m_tabs && index >= 0 && index < m_tabs->count())
            m_tabs->setCurrentIndex(index);
    });
    PluginHost::instance().start(m_wb);
    rebuildSheetTabs();
    PluginHost::instance().setCurrentSheet(currentSheet());
}

void MainWindow::setupSheetView()
{
    m_view->setModel(m_model);
    m_view->setObjectName(QStringLiteral("sheetView"));
    m_view->setAlternatingRowColors(false);
    m_view->setShowGrid(true);
    m_view->setWordWrap(false);
    m_view->setCornerButtonEnabled(true);
    m_view->setFrameShape(QFrame::NoFrame);
    m_view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_view->setSelectionBehavior(QAbstractItemView::SelectItems);
    m_view->setContextMenuPolicy(Qt::CustomContextMenu);
    m_view->setTabKeyNavigation(true);
    m_view->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_view->horizontalHeader()->setHighlightSections(false);
    m_view->horizontalHeader()->setStretchLastSection(false);
    m_view->horizontalHeader()->setDefaultSectionSize(80);
    m_view->verticalHeader()->setDefaultSectionSize(24);
    m_view->verticalHeader()->setHighlightSections(false);
    m_view->verticalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_view->setItemDelegate(new GridDelegate(m_view));
    connect(m_view->selectionModel(), &QItemSelectionModel::currentChanged,
            this, &MainWindow::onSelectionChanged);
    connect(m_view->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, [this](const QItemSelection &, const QItemSelection &) { onSelectionChanged(); });
    connect(m_view, &QTableView::customContextMenuRequested, this, &MainWindow::cellContextMenu);
    connect(m_view, &SheetView::fillReleased, this, &MainWindow::onFillReleased);
    connect(m_view->horizontalHeader(), &QHeaderView::sectionDoubleClicked, this, [this](int section) {
        m_view->resizeColumnToContents(section);
    });
}

void MainWindow::applyScale()
{
    const int p = AppSettings::instance().uiScalePercent();
    QFont f = font();
    f.setPointSizeF(10.0 * p / 100.0);
    setFont(f);
}

void MainWindow::syncThemeMenu()
{
    if (!m_themeGroup)
        return;
    const QString cur = AppSettings::instance().themePreset();
    for (QAction *act : m_themeGroup->actions())
        act->setChecked(act->data().toString() == cur);
}

void MainWindow::rebuildThemeMenu()
{
    if (!m_themeMenu || !m_themeGroup)
        return;
    m_themeMenu->clear();
    for (QAction *a : m_themeGroup->actions())
        m_themeGroup->removeAction(a);
    for (const QString &id : Theme::presetIds()) {
        const QString presetId = id;
        auto *act = m_themeMenu->addAction(ArbuzIcon::named(QStringLiteral("theme")), Theme::presetTitle(id));
        act->setCheckable(true);
        act->setIconVisibleInMenu(true);
        act->setData(id);
        m_themeGroup->addAction(act);
        connect(act, &QAction::triggered, this, [this, presetId]() {
            Theme::applyPreset(presetId);
            applyScale();
        });
    }
    syncThemeMenu();
}

QMenu *MainWindow::functionMenu(QWidget *parent)
{
    auto *menu = new QMenu(parent);
    for (const QString &name : QStringList{QStringLiteral("SUM"), QStringLiteral("AVERAGE"),
                                           QStringLiteral("IF"), QStringLiteral("VLOOKUP"),
                                           QStringLiteral("COUNTIF"), QStringLiteral("CONCAT")}) {
        auto *act = menu->addAction(ArbuzIcon::named(QStringLiteral("fx")), name);
        act->setIconVisibleInMenu(true);
        connect(act, &QAction::triggered, this, [this, name]() { insertFunction(name); });
    }
    menu->addSeparator();
    auto *all = menu->addAction(ArbuzIcon::named(QStringLiteral("fx")),
                                I18n::t("ui.all_functions"), this, &MainWindow::showFunctionDialog);
    all->setIconVisibleInMenu(true);
    return menu;
}

void MainWindow::rebuildSheetTabs()
{
    if (!m_tabs)
        return;
    int keep = m_tabs->currentIndex();
    m_tabs->blockSignals(true);
    while (m_tabs->count() > 0)
        m_tabs->removeTab(0);
    for (int i = 0; i < m_wb->sheetCount(); ++i)
        m_tabs->addTab(m_wb->sheet(i).name);
    if (keep < 0 || keep >= m_tabs->count())
        keep = m_tabs->count() - 1;
    if (keep >= 0)
        m_tabs->setCurrentIndex(keep);
    m_tabs->blockSignals(false);
    if (keep >= 0)
        switchToSheet(keep);
    if (m_view && currentSheet() >= 0) {
        applyMerges();
        m_view->setFrozen(m_wb->sheet(currentSheet()).freezeRows, m_wb->sheet(currentSheet()).freezeCols);
    }
}

void MainWindow::switchToSheet(int index)
{
    if (!m_model || index < 0 || index >= m_wb->sheetCount())
        return;
    if (m_model->sheetIndex() != index) {
        saveColumnWidths();
        m_model->setSheetIndex(index);
        restoreColumnWidths();
        applyMerges();
        m_view->setFrozen(m_wb->sheet(index).freezeRows, m_wb->sheet(index).freezeCols);
    }
    onSelectionChanged();
    updateStatus();
}

void MainWindow::saveColumnWidths()
{
    if (!m_view || !m_model)
        return;
    const int sh = m_model->sheetIndex();
    if (sh < 0 || sh >= m_wb->sheetCount())
        return;
    Worksheet &ws = m_wb->sheet(sh);
    for (int c = 0; c < m_model->columnCount(); ++c)
        ws.columnWidths.insert(c, m_view->columnWidth(c));
}

void MainWindow::restoreColumnWidths()
{
    if (!m_view || !m_model)
        return;
    const int sh = m_model->sheetIndex();
    if (sh < 0 || sh >= m_wb->sheetCount())
        return;
    const Worksheet &ws = m_wb->sheet(sh);
    for (auto it = ws.columnWidths.cbegin(); it != ws.columnWidths.cend(); ++it)
        m_view->setColumnWidth(it.key(), it.value());
}

int MainWindow::currentSheet() const
{
    return m_tabs ? m_tabs->currentIndex() : 0;
}

void MainWindow::onSheetChanged(int index)
{
    switchToSheet(index);
    PluginHost::instance().setCurrentSheet(index);
}

void MainWindow::onSelectionChanged()
{
    if (!m_view || !m_view->currentIndex().isValid())
        return;
    const QModelIndex cur = m_view->currentIndex();
    PluginHost::instance().setSelection(CellRef::a1(cur.row(), cur.column()), selectedRangeA1());
    updateFormulaBar();
    updateStatus();
}

QString MainWindow::selectedRangeA1() const
{
    if (!m_view || !m_view->selectionModel())
        return {};
    const QModelIndexList idxs = m_view->selectionModel()->selectedIndexes();
    if (idxs.isEmpty())
        return {};
    int minR = idxs.first().row(), maxR = minR, minC = idxs.first().column(), maxC = minC;
    for (const QModelIndex &i : idxs) {
        minR = qMin(minR, i.row());
        maxR = qMax(maxR, i.row());
        minC = qMin(minC, i.column());
        maxC = qMax(maxC, i.column());
    }
    if (minR == maxR && minC == maxC)
        return CellRef::a1(minR, minC);
    return CellRef::a1(minR, minC) + QLatin1Char(':') + CellRef::a1(maxR, maxC);
}

void MainWindow::insertFunction(const QString &name)
{
    if (name.isEmpty() || !m_view)
        return;
    QString inner;
    if (name == QLatin1String("IF"))
        inner = QStringLiteral(",");
    else {
        const QString range = selectedRangeA1();
        const QModelIndex cur = m_view->currentIndex();
        const QString curA1 = cur.isValid() ? CellRef::a1(cur.row(), cur.column()) : QString();
        if (!range.isEmpty() && range != curA1)
            inner = range;
    }
    const QString formula = QStringLiteral("=%1(%2)").arg(name, inner);
    const QModelIndex cur = m_view->currentIndex();
    if (!cur.isValid())
        return;
    m_view->model()->setData(cur, formula, Qt::EditRole);
    m_view->setCurrentIndex(cur);
    m_view->edit(cur);
}

void MainWindow::showFunctionDialog()
{
    FunctionDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted)
        insertFunction(dlg.selectedName());
}

void MainWindow::cellContextMenu(const QPoint &pos)
{
    const QModelIndex idx = m_view->indexAt(pos);
    if (idx.isValid())
        m_view->setCurrentIndex(idx);

    QMenu menu(this);
    auto add = [&](const QString &id, const QString &text, void (MainWindow::*slot)()) {
        QAction *act = menu.addAction(ArbuzIcon::named(id), text, this, slot);
        act->setIconVisibleInMenu(true);
        return act;
    };
    add(QStringLiteral("cut"), I18n::t("ui.cut"), &MainWindow::cut);
    add(QStringLiteral("copy"), I18n::t("ui.copy"), &MainWindow::copy);
    add(QStringLiteral("paste"), I18n::t("ui.paste"), &MainWindow::paste);
    add(QStringLiteral("clear"), I18n::t("ui.clear"), &MainWindow::clearContents);
    menu.addSeparator();
    QMenu *fnMenu = functionMenu(&menu);
    fnMenu->setTitle(I18n::t("ui.function_2"));
    fnMenu->setIcon(ArbuzIcon::named(QStringLiteral("fx")));
    menu.addMenu(fnMenu);
    menu.addSeparator();
    add(QStringLiteral("bold"), I18n::t("ui.bold"), &MainWindow::toggleBold);
    add(QStringLiteral("italic"), I18n::t("ui.italic"), &MainWindow::toggleItalic);
    add(QStringLiteral("text-color"), I18n::t("ui.text_color"), &MainWindow::setTextColor);
    add(QStringLiteral("fill-color"), I18n::t("ui.fill_color"), &MainWindow::setFillColor);
    menu.exec(m_view->viewport()->mapToGlobal(pos));
}

void MainWindow::sheetTabContextMenu(const QPoint &pos)
{
    const int index = m_tabs->tabAt(pos);
    if (index >= 0)
        m_tabs->setCurrentIndex(index);

    QMenu menu(this);
    auto *ins = menu.addAction(ArbuzIcon::named(QStringLiteral("add-sheet")),
                               I18n::t("ui.insert_sheet"), this, &MainWindow::addSheet);
    ins->setIconVisibleInMenu(true);
    auto *del = menu.addAction(ArbuzIcon::named(QStringLiteral("delete-sheet")),
                               I18n::t("ui.delete_sheet"), this, &MainWindow::removeSheet);
    del->setIconVisibleInMenu(true);
    del->setEnabled(m_wb->sheetCount() > 1);
    auto *ren = menu.addAction(ArbuzIcon::named(QStringLiteral("rename-sheet")),
                               I18n::t("ui.rename"), this, &MainWindow::renameSheet);
    ren->setIconVisibleInMenu(true);
    menu.exec(m_tabs->mapToGlobal(pos));
}

void MainWindow::updateStatus()
{
    QString range = selectedRangeA1();
    if (range.isEmpty())
        range = QStringLiteral("A1");

    QString extra;
    if (m_view && m_view->selectionModel()) {
        const QModelIndexList idxs = m_view->selectionModel()->selectedIndexes();
        if (idxs.size() > 1) {
            double sum = 0;
            int n = 0;
            const int sh = currentSheet();
            for (const QModelIndex &i : idxs) {
                double v = 0;
                const QString shown = m_wb ? m_wb->displayText(sh, i.row(), i.column())
                                           : i.data(Qt::DisplayRole).toString();
                if (NumFormat::parse(shown, &v)) {
                    sum += v;
                    ++n;
                }
            }
            if (n > 0) {
                extra = I18n::t("ui.1_avg_2_n_3")
                            .arg(QString::number(sum, 'g', 12), QString::number(sum / n, 'g', 12),
                                 QString::number(n));
            }
        }
    }

    const QString sheet = (currentSheet() >= 0 && currentSheet() < m_wb->sheetCount())
        ? m_wb->sheet(currentSheet()).name
        : QString();
    statusBar()->showMessage(QStringLiteral("%1    %2%3").arg(sheet, range, extra));
}

void MainWindow::newFile()
{
    if (!confirmSave())
        return;
    m_wb->resetToEmpty();
    m_path.clear();
    m_dirty = false;
    setCurrentPath({});
    PluginHost::instance().notifyEvent(QStringLiteral("workbook_new"));
}

void MainWindow::openFile()
{
    if (!confirmSave())
        return;
    // GTK/portal on Linux mishandles "(*.xlsx *.csv)"; one pattern per filter.
    const QString path = QFileDialog::getOpenFileName(
        this, I18n::t("ui.open_2"), AppSettings::instance().documentsPath(),
        I18n::t("ui.arbuz_xlsx_csv_csv_all_files"));
    if (path.isEmpty())
        return;
    openPath(path);
}

bool MainWindow::openPath(const QString &path)
{
    QString err;
    if (!FileIo::load(m_wb, path, &err)) {
        QMessageBox::warning(this, QStringLiteral("Arbuz"), err);
        return false;
    }
    rebuildSheetTabs();
    setCurrentPath(path);
    m_dirty = false;
    PluginHost::instance().notifyEvent(QStringLiteral("workbook_opened"),
                                       QJsonObject{{QStringLiteral("path"), path}});
    return true;
}

bool MainWindow::saveFile()
{
    if (m_path.isEmpty())
        return saveFileAs();
    saveColumnWidths();
    PluginHost::instance().notifyEvent(QStringLiteral("before_save"),
                                       QJsonObject{{QStringLiteral("path"), m_path}});
    QString err;
    if (!FileIo::save(m_wb, m_path, &err)) {
        QMessageBox::warning(this, QStringLiteral("Arbuz"), err);
        return false;
    }
    m_dirty = false;
    PluginHost::instance().notifyEvent(QStringLiteral("after_save"),
                                       QJsonObject{{QStringLiteral("path"), m_path}});
    return true;
}

bool MainWindow::saveFileAs()
{
    const QString path = QFileDialog::getSaveFileName(
        this, I18n::t("ui.save_as_2"), AppSettings::instance().documentsPath(),
        I18n::t("ui.arbuz_xlsx_csv_csv"));
    if (path.isEmpty())
        return false;
    m_path = path;
    return saveFile();
}

void MainWindow::setCurrentPath(const QString &path)
{
    m_path = path;
    AppSettings::instance().setLastFile(path);
    QString title = QStringLiteral("Arbuz");
    if (!path.isEmpty())
        title += QStringLiteral(" — ") + path;
    setWindowTitle(title);
}

bool MainWindow::confirmSave()
{
    if (!m_dirty)
        return true;
    QMessageBox box(this);
    box.setWindowTitle(QStringLiteral("Arbuz"));
    box.setText(I18n::t("ui.save_changes"));
    auto *yes = box.addButton(I18n::t("ui.yes"), QMessageBox::YesRole);
    box.addButton(I18n::t("ui.no"), QMessageBox::NoRole);
    auto *cancel = box.addButton(I18n::t("ui.cancel"), QMessageBox::RejectRole);
    box.setDefaultButton(yes);
    box.exec();
    if (box.clickedButton() == cancel)
        return false;
    if (box.clickedButton() == yes)
        return saveFile();
    return true;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!confirmSave()) {
        event->ignore();
        return;
    }
    PluginHost::instance().notifyEvent(QStringLiteral("app_quit"));
    PluginHost::instance().stop();
    event->accept();
}

void MainWindow::copy()
{
    if (!m_view)
        return;
    const QModelIndexList idxs = m_view->selectionModel()->selectedIndexes();
    if (idxs.isEmpty())
        return;
    int minR = idxs.first().row(), maxR = minR, minC = idxs.first().column(), maxC = minC;
    for (const QModelIndex &i : idxs) {
        minR = qMin(minR, i.row());
        maxR = qMax(maxR, i.row());
        minC = qMin(minC, i.column());
        maxC = qMax(maxC, i.column());
    }
    QStringList rows;
    for (int r = minR; r <= maxR; ++r) {
        QStringList cols;
        for (int c = minC; c <= maxC; ++c)
            cols.append(m_view->model()->index(r, c).data(Qt::EditRole).toString());
        rows.append(cols.join(QLatin1Char('\t')));
    }
    QApplication::clipboard()->setText(rows.join(QLatin1Char('\n')));
    m_copyRow = minR;
    m_copyCol = minC;
    m_copyHasOrigin = true;
}

void MainWindow::paste()
{
    if (!m_view || !m_view->currentIndex().isValid())
        return;
    const QString text = QApplication::clipboard()->text();
    const int r0 = m_view->currentIndex().row();
    const int c0 = m_view->currentIndex().column();
    const int dRow = m_copyHasOrigin ? r0 - m_copyRow : 0;
    const int dCol = m_copyHasOrigin ? c0 - m_copyCol : 0;
    const QStringList rows = text.split(QLatin1Char('\n'));
    m_wb->beginUndoMacro(I18n::t("ui.paste_2"));
    for (int i = 0; i < rows.size(); ++i) {
        if (rows.at(i).isEmpty() && i == rows.size() - 1)
            continue;
        const QStringList cols = rows.at(i).split(QLatin1Char('\t'));
        for (int j = 0; j < cols.size(); ++j) {
            QString v = cols.at(j);
            if (m_copyHasOrigin && v.startsWith(QLatin1Char('=')))
                v = CellRef::adjustFormula(v, dRow, dCol);
            m_view->model()->setData(m_view->model()->index(r0 + i, c0 + j), v, Qt::EditRole);
        }
    }
    m_wb->endUndoMacro();
}

void MainWindow::cut()
{
    copy();
    clearContents();
}

void MainWindow::clearContents()
{
    if (!m_view)
        return;
    const QModelIndexList idxs = m_view->selectionModel()->selectedIndexes();
    for (const QModelIndex &i : idxs)
        m_view->model()->setData(i, QString(), Qt::EditRole);
}

void MainWindow::findCell()
{
    bool ok = false;
    const QString needle = QInputDialog::getText(this, I18n::t("ui.find_2"),
                                                 I18n::t("ui.text"), QLineEdit::Normal, m_findNeedle, &ok);
    if (!ok || needle.isEmpty() || currentSheet() < 0)
        return;
    m_findNeedle = needle;
    int r = 0;
    int c = 0;
    if (m_wb->findNext(currentSheet(), m_findNeedle, 0, -1, &r, &c, false))
        revealCell(r, c);
    else
        statusBar()->showMessage(I18n::t("ui.not_found"), 3000);
}

void MainWindow::findNextCell()
{
    if (m_findNeedle.isEmpty()) {
        findCell();
        return;
    }
    if (currentSheet() < 0)
        return;
    int fr = 0;
    int fc = -1;
    if (m_view && m_view->currentIndex().isValid()) {
        fr = m_view->currentIndex().row();
        fc = m_view->currentIndex().column();
    }
    int r = 0;
    int c = 0;
    if (m_wb->findNext(currentSheet(), m_findNeedle, fr, fc, &r, &c, true))
        revealCell(r, c);
    else
        statusBar()->showMessage(I18n::t("ui.not_found"), 3000);
}

void MainWindow::findPrevCell()
{
    if (m_findNeedle.isEmpty()) {
        findCell();
        return;
    }
    if (currentSheet() < 0)
        return;
    int fr = 0;
    int fc = 0;
    if (m_view && m_view->currentIndex().isValid()) {
        fr = m_view->currentIndex().row();
        fc = m_view->currentIndex().column();
    }
    int r = 0;
    int c = 0;
    if (m_wb->findPrev(currentSheet(), m_findNeedle, fr, fc, &r, &c, true))
        revealCell(r, c);
    else
        statusBar()->showMessage(I18n::t("ui.not_found"), 3000);
}

void MainWindow::revealCell(int row, int col)
{
    if (!m_view)
        return;
    const QModelIndex idx = m_view->model()->index(row, col);
    if (!idx.isValid())
        return;
    m_view->setCurrentIndex(idx);
    m_view->scrollTo(idx);
}

void MainWindow::goToCell()
{
    bool ok = false;
    const QString ref = QInputDialog::getText(this, I18n::t("ui.go_to"),
                                              QStringLiteral("A1"), QLineEdit::Normal, {}, &ok);
    if (!ok || ref.trimmed().isEmpty() || !m_view)
        return;
    int r = 0, c = 0;
    if (!CellRef::parseA1(ref.trimmed(), &r, &c))
        return;
    const QModelIndex idx = m_view->model()->index(r, c);
    if (!idx.isValid())
        return;
    m_view->setCurrentIndex(idx);
    m_view->scrollTo(idx);
}

void MainWindow::applyStyleToSelection(bool doBold, bool doItalic, bool setFg, const QColor &fg,
                                       bool setBg, const QColor &bg)
{
    if (!m_view)
        return;
    const int sh = currentSheet();
    m_wb->beginUndoMacro(I18n::t("ui.format"));
    for (const QModelIndex &i : m_view->selectionModel()->selectedIndexes()) {
        CellData d = m_wb->sheet(sh).cell(i.row(), i.column());
        const bool bold = doBold ? !d.bold : d.bold;
        const bool italic = doItalic ? !d.italic : d.italic;
        const QColor foreground = setFg ? fg : d.foreground;
        const QColor background = setBg ? bg : d.background;
        m_wb->setStyle(sh, i.row(), i.column(), bold, italic, foreground, background);
    }
    m_wb->endUndoMacro();
}

void MainWindow::toggleBold()
{
    applyStyleToSelection(true, false, false, {}, false, {});
}

void MainWindow::toggleItalic()
{
    applyStyleToSelection(false, true, false, {}, false, {});
}

void MainWindow::setFillColor()
{
    const QColor c = QColorDialog::getColor(Qt::white, this);
    if (!c.isValid())
        return;
    applyStyleToSelection(false, false, false, {}, true, c);
}

void MainWindow::setTextColor()
{
    const QColor c = QColorDialog::getColor(Qt::black, this);
    if (!c.isValid())
        return;
    applyStyleToSelection(false, false, true, c, false, {});
}

void MainWindow::addSheet()
{
    const int i = m_wb->addSheet();
    m_tabs->setCurrentIndex(i);
}

void MainWindow::removeSheet()
{
    const int sh = currentSheet();
    if (sh < 0)
        return;
    if (m_wb->sheetCount() <= 1) {
        statusBar()->showMessage(I18n::t("ui.cannot_delete_the_last_sheet"), 3000);
        return;
    }
    QMessageBox box(this);
    box.setWindowTitle(QStringLiteral("Arbuz"));
    box.setText(I18n::t("ui.delete_sheet_1").arg(m_wb->sheet(sh).name));
    auto *yes = box.addButton(I18n::t("ui.yes"), QMessageBox::YesRole);
    box.addButton(I18n::t("ui.no"), QMessageBox::NoRole);
    box.setDefaultButton(yes);
    box.exec();
    if (box.clickedButton() != yes)
        return;
    m_wb->removeSheet(sh);
}

void MainWindow::renameSheet()
{
    const int sh = currentSheet();
    if (sh < 0)
        return;
    bool ok = false;
    const QString n = QInputDialog::getText(this, I18n::t("ui.sheet"),
                                            I18n::t("ui.name"), QLineEdit::Normal,
                                            m_wb->sheet(sh).name, &ok);
    if (ok)
        m_wb->renameSheet(sh, n);
}

void MainWindow::showSettings()
{
    SettingsDialog dlg(this);
    connect(&dlg, &SettingsDialog::themeChanged, this, [this]() {
        applyScale();
        rebuildThemeMenu();
    });
    dlg.exec();
}

void MainWindow::showFirstRun()
{
    FirstRunWizard wiz(this);
    if (wiz.exec() == QDialog::Accepted) {
        applyScale();
        rebuildThemeMenu();
    }
}

void MainWindow::showCredits()
{
    CreditsDialog dlg(this);
    dlg.exec();
}

void MainWindow::showAbout()
{
    QMessageBox::about(
        this, I18n::t("ui.about"),
        I18n::t("ui.about_text"));
}

void MainWindow::selectedBounds(int *r1, int *c1, int *r2, int *c2) const
{
    int a = 0, b = 0, c = 0, d = 0;
    if (m_view && m_view->selectionModel() && m_view->selectionModel()->hasSelection()) {
        const QModelIndexList idxs = m_view->selectionModel()->selectedIndexes();
        a = b = idxs.first().row();
        c = d = idxs.first().column();
        for (const QModelIndex &i : idxs) {
            a = qMin(a, i.row());
            b = qMax(b, i.row());
            c = qMin(c, i.column());
            d = qMax(d, i.column());
        }
    } else if (m_view && m_view->currentIndex().isValid()) {
        a = b = m_view->currentIndex().row();
        c = d = m_view->currentIndex().column();
    }
    if (r1)
        *r1 = a;
    if (r2)
        *r2 = b;
    if (c1)
        *c1 = c;
    if (c2)
        *c2 = d;
}

void MainWindow::updateFormulaBar()
{
    if (!m_formulaBar || !m_nameBox || !m_view || m_syncingFormula)
        return;
    const QModelIndex cur = m_view->currentIndex();
    m_syncingFormula = true;
    if (cur.isValid()) {
        m_nameBox->setText(CellRef::a1(cur.row(), cur.column()));
        m_formulaBar->setText(cur.data(Qt::EditRole).toString());
    } else {
        m_nameBox->setText(QStringLiteral("A1"));
        m_formulaBar->clear();
    }
    m_syncingFormula = false;
}

void MainWindow::formulaBarCommit()
{
    if (!m_view || !m_formulaBar)
        return;
    const QModelIndex cur = m_view->currentIndex();
    if (!cur.isValid())
        return;
    m_view->model()->setData(cur, m_formulaBar->text(), Qt::EditRole);
    m_view->setFocus();
}

void MainWindow::nameBoxCommit()
{
    if (!m_nameBox || !m_view)
        return;
    int r = 0, c = 0;
    if (!CellRef::parseA1(m_nameBox->text().trimmed(), &r, &c))
        return;
    const QModelIndex idx = m_view->model()->index(r, c);
    if (!idx.isValid())
        return;
    m_view->setCurrentIndex(idx);
    m_view->scrollTo(idx);
}

void MainWindow::applyMerges()
{
    if (!m_view || currentSheet() < 0)
        return;
    m_view->clearSpans();
    for (const MergeRange &m : m_wb->sheet(currentSheet()).merges)
        m_view->setSpan(m.r1, m.c1, m.r2 - m.r1 + 1, m.c2 - m.c1 + 1);
}

void MainWindow::insertRows()
{
    int r1, c1, r2, c2;
    selectedBounds(&r1, &c1, &r2, &c2);
    m_wb->insertRows(currentSheet(), r1, r2 - r1 + 1);
}

void MainWindow::insertColumns()
{
    int r1, c1, r2, c2;
    selectedBounds(&r1, &c1, &r2, &c2);
    m_wb->insertColumns(currentSheet(), c1, c2 - c1 + 1);
}

void MainWindow::removeRows()
{
    int r1, c1, r2, c2;
    selectedBounds(&r1, &c1, &r2, &c2);
    m_wb->removeRows(currentSheet(), r1, r2 - r1 + 1);
}

void MainWindow::removeColumns()
{
    int r1, c1, r2, c2;
    selectedBounds(&r1, &c1, &r2, &c2);
    m_wb->removeColumns(currentSheet(), c1, c2 - c1 + 1);
}

void MainWindow::freezePanes()
{
    if (!m_view || !m_view->currentIndex().isValid())
        return;
    const QModelIndex cur = m_view->currentIndex();
    m_wb->setFreeze(currentSheet(), cur.row(), cur.column());
    m_view->setFrozen(cur.row(), cur.column());
}

void MainWindow::unfreezePanes()
{
    m_wb->setFreeze(currentSheet(), 0, 0);
    m_view->setFrozen(0, 0);
}

void MainWindow::sortAsc()
{
    int r1, c1, r2, c2;
    selectedBounds(&r1, &c1, &r2, &c2);
    const int key = m_view && m_view->currentIndex().isValid() ? m_view->currentIndex().column() : c1;
    m_wb->sortRange(currentSheet(), r1, c1, r2, c2, key, true);
}

void MainWindow::sortDesc()
{
    int r1, c1, r2, c2;
    selectedBounds(&r1, &c1, &r2, &c2);
    const int key = m_view && m_view->currentIndex().isValid() ? m_view->currentIndex().column() : c1;
    m_wb->sortRange(currentSheet(), r1, c1, r2, c2, key, false);
}

void MainWindow::mergeSelection()
{
    int r1, c1, r2, c2;
    selectedBounds(&r1, &c1, &r2, &c2);
    m_wb->mergeCells(currentSheet(), r1, c1, r2, c2);
    applyMerges();
}

void MainWindow::unmergeSelection()
{
    if (!m_view || !m_view->currentIndex().isValid())
        return;
    m_wb->unmergeAt(currentSheet(), m_view->currentIndex().row(), m_view->currentIndex().column());
    applyMerges();
}

void MainWindow::applyAlign(int hAlign)
{
    const int sh = currentSheet();
    m_wb->beginUndoMacro(I18n::t("ui.alignment"));
    for (const QModelIndex &i : m_view->selectionModel()->selectedIndexes()) {
        CellData d = m_wb->sheet(sh).cell(i.row(), i.column());
        d.hAlign = hAlign;
        m_wb->setCellData(sh, i.row(), i.column(), d);
    }
    m_wb->endUndoMacro();
}

void MainWindow::alignLeft()
{
    applyAlign(1);
}
void MainWindow::alignCenter()
{
    applyAlign(2);
}
void MainWindow::alignRight()
{
    applyAlign(3);
}

void MainWindow::toggleWrap()
{
    const int sh = currentSheet();
    m_wb->beginUndoMacro(I18n::t("ui.wrap"));
    for (const QModelIndex &i : m_view->selectionModel()->selectedIndexes()) {
        CellData d = m_wb->sheet(sh).cell(i.row(), i.column());
        d.wrap = !d.wrap;
        m_wb->setCellData(sh, i.row(), i.column(), d);
    }
    m_wb->endUndoMacro();
}

void MainWindow::applyNumFmt(int fmt)
{
    const int sh = currentSheet();
    m_wb->beginUndoMacro(I18n::t("ui.number_format"));
    for (const QModelIndex &i : m_view->selectionModel()->selectedIndexes()) {
        CellData d = m_wb->sheet(sh).cell(i.row(), i.column());
        d.numFmt = fmt;
        m_wb->setCellData(sh, i.row(), i.column(), d);
    }
    m_wb->endUndoMacro();
}

void MainWindow::setNumFmtGeneral()
{
    applyNumFmt(NumFormat::General);
}
void MainWindow::setNumFmtNumber()
{
    applyNumFmt(NumFormat::Number2);
}
void MainWindow::setNumFmtThousands()
{
    applyNumFmt(NumFormat::Thousands2);
}
void MainWindow::setNumFmtDate()
{
    applyNumFmt(NumFormat::Date);
}
void MainWindow::setNumFmtPercent()
{
    applyNumFmt(NumFormat::Percent);
}
void MainWindow::setNumFmtPercent2()
{
    applyNumFmt(NumFormat::Percent2);
}
void MainWindow::setNumFmtScientific()
{
    applyNumFmt(NumFormat::Scientific);
}
void MainWindow::setNumFmtDateTime()
{
    applyNumFmt(NumFormat::DateTime);
}
void MainWindow::setNumFmtTime()
{
    applyNumFmt(NumFormat::Time);
}

void MainWindow::toggleBorder()
{
    const int sh = currentSheet();
    m_wb->beginUndoMacro(I18n::t("ui.borders"));
    for (const QModelIndex &i : m_view->selectionModel()->selectedIndexes()) {
        CellData d = m_wb->sheet(sh).cell(i.row(), i.column());
        d.border = d.border ? 0 : 15;
        m_wb->setCellData(sh, i.row(), i.column(), d);
    }
    m_wb->endUndoMacro();
}

void MainWindow::onFillReleased(int sr1, int sc1, int sr2, int sc2, int er1, int ec1, int er2, int ec2)
{
    if (currentSheet() < 0)
        return;
    m_wb->fill(currentSheet(), sr1, sc1, sr2, sc2, er1, ec1, er2, ec2);
}

void MainWindow::fillDown()
{
    if (currentSheet() < 0 || !m_view)
        return;
    int r1 = 0, c1 = 0, r2 = 0, c2 = 0;
    selectedBounds(&r1, &c1, &r2, &c2);
    if (r2 <= r1)
        r2 = r1 + 1;
    m_wb->fill(currentSheet(), r1, c1, r1, c2, r1, c1, r2, c2);
}

void MainWindow::fillRight()
{
    if (currentSheet() < 0 || !m_view)
        return;
    int r1 = 0, c1 = 0, r2 = 0, c2 = 0;
    selectedBounds(&r1, &c1, &r2, &c2);
    if (c2 <= c1)
        c2 = c1 + 1;
    m_wb->fill(currentSheet(), r1, c1, r2, c1, r1, c1, r2, c2);
}

void MainWindow::onTabMoved(int from, int to)
{
    m_wb->moveSheet(from, to);
}

void MainWindow::replaceCell()
{
    bool ok = false;
    const QString needle = QInputDialog::getText(this, I18n::t("ui.replace_2"),
                                                 I18n::t("ui.find_2"), QLineEdit::Normal, {}, &ok);
    if (!ok || needle.isEmpty() || currentSheet() < 0)
        return;
    const QString repl = QInputDialog::getText(this, I18n::t("ui.replace_2"),
                                               I18n::t("ui.replace_with"), QLineEdit::Normal, {}, &ok);
    if (!ok)
        return;
    Worksheet &ws = m_wb->sheet(currentSheet());
    m_wb->beginUndoMacro(I18n::t("ui.replace_2"));
    int n = 0;
    for (int r = 0; r < ws.rowCount; ++r) {
        for (int c = 0; c < ws.colCount; ++c) {
            CellData d = ws.cell(r, c);
            if (d.raw.contains(needle, Qt::CaseInsensitive)) {
                d.raw.replace(needle, repl, Qt::CaseInsensitive);
                m_wb->setCellData(currentSheet(), r, c, d);
                ++n;
            }
        }
    }
    m_wb->endUndoMacro();
    statusBar()->showMessage(I18n::t("ui.replaced_1").arg(n), 3000);
}

void MainWindow::printSheet()
{
    QPrinter printer;
    QPrintDialog dlg(&printer, this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    QPainter painter(&printer);
    const int sh = currentSheet();
    const Worksheet &ws = m_wb->sheet(sh);
    const int rowH = 18;
    const int colW = 72;
    int y = 40;
    painter.drawText(40, 24, ws.name);
    const int rows = qMin(ws.rowCount, 60);
    const int cols = qMin(ws.colCount, 12);
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            const QRect box(40 + c * colW, y, colW - 2, rowH);
            painter.drawRect(box);
            painter.drawText(box.adjusted(2, 0, -2, 0), Qt::AlignVCenter | Qt::AlignLeft,
                             m_wb->displayText(sh, r, c));
        }
        y += rowH;
        if (y > printer.pageRect(QPrinter::DevicePixel).height() - 40) {
            printer.newPage();
            y = 40;
        }
    }
}
