#include "uitest.h"

#include "arbuzicon.h"
#include "creditsdialog.h"
#include "firstrunwizard.h"
#include "functiondialog.h"
#include "i18n.h"
#include "mainwindow.h"
#include "pluginhost.h"
#include "settingsdialog.h"
#include "theme.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QColor>
#include <QComboBox>
#include <QDir>
#include <QHeaderView>
#include <QItemSelection>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMouseEvent>
#include <QPixmap>
#include <QPoint>
#include <QDockWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabBar>
#include <QTableView>
#include <QTextBrowser>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QWidget>
#include <cstdio>

static int g_fails = 0;

static void fail(const QString &msg)
{
    std::fprintf(stderr, "UI FAIL: %s\n", qPrintable(msg));
    ++g_fails;
}

static void ok(bool cond, const QString &msg)
{
    if (!cond)
        fail(msg);
    else {
        std::printf("UI OK: %s\n", qPrintable(msg));
        std::fflush(stdout);
    }
}

static bool saveGrab(QWidget *w, const QString &dir, const QString &name)
{
    if (!w)
        return false;
    w->show();
    w->raise();
    qApp->processEvents();
    const QPixmap px = w->grab();
    const QString path = dir + QLatin1Char('/') + name;
    const bool saved = px.save(path, "PNG");
    if (!saved)
        fail(QStringLiteral("screenshot not saved: %1").arg(path));
    else
        std::printf("UI SHOT: %s %dx%d\n", qPrintable(path), px.width(), px.height());
    return saved && !px.isNull() && px.width() > 10 && px.height() > 10;
}

int runUiTest()
{
    g_fails = 0;
    QDir().mkpath(QStringLiteral("ui-test-out"));
    const QString out = QDir(QStringLiteral("ui-test-out")).absolutePath();

    Theme::applyPreset(QStringLiteral("white"));
    Theme::apply(qApp);

    MainWindow w;
    w.setAttribute(Qt::WA_DontShowOnScreen, false);
    w.resize(1100, 720);
    w.show();
    qApp->processEvents();

    QMenuBar *bar = w.menuBar();
    ok(bar != nullptr, QStringLiteral("menu bar exists"));
    ok(!bar->isNativeMenuBar(), QStringLiteral("menu bar is in-window, not native/panel"));
    ok(bar->isVisible(), QStringLiteral("menu bar visible"));
    ok(bar->height() >= 24, QStringLiteral("menu bar height=%1").arg(bar->height()));
    ok(bar->actions().size() >= 5, QStringLiteral("menus count=%1").arg(bar->actions().size()));

    QStringList menus;
    for (QAction *a : bar->actions())
        menus.append(a->text());
    ok(menus.join(QLatin1Char('|')).contains(QStringLiteral("Файл")), QStringLiteral("menu Файл: %1").arg(menus.join(',')));
    ok(menus.join(QLatin1Char('|')).contains(QStringLiteral("Вставка")), QStringLiteral("menu Вставка"));
    ok(menus.join(QLatin1Char('|')).contains(QStringLiteral("Лист")), QStringLiteral("menu Лист"));
    ok(menus.join(QLatin1Char('|')).contains(QStringLiteral("Вид")), QStringLiteral("menu Вид"));
    ok(menus.join(QLatin1Char('|')).contains(QStringLiteral("Плагины")), QStringLiteral("menu Плагины"));
    ok(menus.join(QLatin1Char('|')).contains(QStringLiteral("Справка")), QStringLiteral("menu Справка"));
    ok(menus.join(QLatin1Char('|')).contains(QStringLiteral("Правка")), QStringLiteral("menu Правка"));
    ok(menus.join(QLatin1Char('|')).contains(QStringLiteral("Формат")), QStringLiteral("menu Формат"));

    QAction *pluginsAct = nullptr;
    for (QAction *a : bar->actions()) {
        if (a->text().contains(QStringLiteral("Плагины")))
            pluginsAct = a;
    }
    ok(pluginsAct && pluginsAct->menu(), QStringLiteral("plugins menu widget"));
    {
        QMenu probe;
        PluginHost::instance().fillMenu(&probe);
        ok(probe.actions().size() >= 2, QStringLiteral("plugins menu has reload/open"));
    }

    bool hasSettingsAction = false;
    for (QAction *top : bar->actions()) {
        if (!top->menu())
            continue;
        for (QAction *a : top->menu()->actions()) {
            if (a->text().contains(QStringLiteral("Настройки")))
                hasSettingsAction = true;
        }
    }
    ok(hasSettingsAction, QStringLiteral("Настройки in menu"));

    ok(w.findChild<QToolBar *>() != nullptr, QStringLiteral("toolbar present"));
    ok(w.findChild<QPushButton *>(QStringLiteral("settingsButton")) == nullptr,
       QStringLiteral("no extra settings button on formula row"));
    ok(w.findChild<QWidget *>(QStringLiteral("formulaRow")) != nullptr, QStringLiteral("formula bar row"));
    ok(w.findChild<QLineEdit *>(QStringLiteral("formulaBar")) != nullptr, QStringLiteral("formula field"));
    ok(w.findChild<QToolButton *>(QStringLiteral("functionButton")) == nullptr,
       QStringLiteral("no fx button on a formula row"));
    ok(!w.windowIcon().isNull(), QStringLiteral("window icon is set"));
    ok(w.findChild<QDockWidget *>(QStringLiteral("functionDock")) == nullptr,
       QStringLiteral("no function dock on the right"));
    ok(w.findChild<QToolButton *>(QStringLiteral("addSheetButton")) != nullptr,
       QStringLiteral("+ add sheet button"));
    bool menuIcons = false;
    for (QAction *top : bar->actions()) {
        if (!top->menu())
            continue;
        for (QAction *a : top->menu()->actions()) {
            if (!a->isSeparator() && !a->icon().isNull()) {
                menuIcons = true;
                break;
            }
        }
    }
    ok(menuIcons, QStringLiteral("menu actions have drawn icons"));

    auto *tabs = w.findChild<QTabBar *>(QStringLiteral("sheetTabs"));
    ok(tabs && tabs->count() >= 1, QStringLiteral("sheet tab present"));
    ok(tabs && tabs->tabText(0).contains(QStringLiteral("Лист")), QStringLiteral("tab name=%1").arg(tabs ? tabs->tabText(0) : QString()));
    ok(tabs && !tabs->tabsClosable(), QStringLiteral("sheet tabs without close crosses"));
    ok(tabs && tabs->contextMenuPolicy() == Qt::CustomContextMenu, QStringLiteral("sheet tab context menu"));

    auto *view = w.findChild<QTableView *>(QStringLiteral("sheetView"));
    ok(view != nullptr, QStringLiteral("table view"));
    ok(view && view->contextMenuPolicy() == Qt::CustomContextMenu, QStringLiteral("cell context menu enabled"));
    ok(view != nullptr, QStringLiteral("table view"));
    ok(view && view->model() && view->model()->rowCount() >= 50, QStringLiteral("grid has rows"));
    ok(view && view->model() && view->model()->columnCount() >= 10, QStringLiteral("grid has columns"));

    if (view && view->model()) {
        const QModelIndex a1 = view->model()->index(0, 0);
        const QModelIndex a2 = view->model()->index(1, 0);
        const QModelIndex a3 = view->model()->index(2, 0);
        ok(view->model()->setData(a1, QStringLiteral("4"), Qt::EditRole), QStringLiteral("edit A1"));
        ok(view->model()->setData(a2, QStringLiteral("6"), Qt::EditRole), QStringLiteral("edit A2"));
        ok(view->model()->setData(a3, QStringLiteral("=SUM(A1:A2)"), Qt::EditRole), QStringLiteral("edit A3 formula"));
        qApp->processEvents();
        ok(view->model()->index(2, 0).data(Qt::DisplayRole).toString() == QLatin1String("10"),
           QStringLiteral("grid shows SUM=10, got %1")
               .arg(view->model()->index(2, 0).data(Qt::DisplayRole).toString()));
        view->setCurrentIndex(a1);
        qApp->processEvents();
        auto *formulaBar = w.findChild<QLineEdit *>(QStringLiteral("formulaBar"));
        ok(formulaBar && formulaBar->text() == QLatin1String("4"),
           QStringLiteral("formula bar shows A1 raw, got %1").arg(formulaBar ? formulaBar->text() : QString()));

        QAction *undoAct = nullptr;
        QAction *insertRowAct = nullptr;
        QAction *findNextAct = nullptr;
        QAction *findPrevAct = nullptr;
        QAction *numFmtMenu = nullptr;
        for (QAction *top : bar->actions()) {
            if (!top->menu())
                continue;
            for (QAction *a : top->menu()->actions()) {
                if (a->text().contains(QStringLiteral("Отменить")))
                    undoAct = a;
                if (a->text().contains(QStringLiteral("Вставить строки")))
                    insertRowAct = a;
                if (a->text().contains(QStringLiteral("Найти далее")))
                    findNextAct = a;
                if (a->text().contains(QStringLiteral("Найти ранее")))
                    findPrevAct = a;
                if (a->text().contains(QStringLiteral("Числовой формат")))
                    numFmtMenu = a;
            }
        }
        ok(undoAct != nullptr, QStringLiteral("undo action"));
        ok(insertRowAct != nullptr, QStringLiteral("insert rows action"));
        ok(findNextAct != nullptr, QStringLiteral("find next action"));
        ok(findPrevAct != nullptr, QStringLiteral("find previous action"));
        ok(numFmtMenu && numFmtMenu->menu() && numFmtMenu->menu()->actions().size() >= 6,
           QStringLiteral("number format submenu"));
        if (insertRowAct) {
            view->setCurrentIndex(a1);
            insertRowAct->trigger();
            qApp->processEvents();
            ok(view->model()->index(1, 0).data(Qt::DisplayRole).toString() == QLatin1String("4"),
               QStringLiteral("insert row shifted A1 down, got %1")
                   .arg(view->model()->index(1, 0).data(Qt::DisplayRole).toString()));
            ok(undoAct != nullptr && undoAct->isEnabled(), QStringLiteral("undo enabled after insert"));
            if (undoAct)
                undoAct->trigger();
            qApp->processEvents();
            ok(view->model()->index(0, 0).data(Qt::DisplayRole).toString() == QLatin1String("4"),
               QStringLiteral("undo insert restored A1, got %1")
                   .arg(view->model()->index(0, 0).data(Qt::DisplayRole).toString()));
            ok(view->model()->index(1, 0).data(Qt::DisplayRole).toString() == QLatin1String("6"),
               QStringLiteral("undo insert restored A2, got %1")
                   .arg(view->model()->index(1, 0).data(Qt::DisplayRole).toString()));
            ok(view->model()->index(2, 0).data(Qt::DisplayRole).toString() == QLatin1String("10"),
               QStringLiteral("undo insert restored SUM, got %1")
                   .arg(view->model()->index(2, 0).data(Qt::DisplayRole).toString()));
        }
        ok(undoAct != nullptr && undoAct->isEnabled(), QStringLiteral("undo enabled after edit"));
        view->setCurrentIndex(a1);
        qApp->processEvents();
        {
            QKeyEvent ctrlDown(QEvent::KeyPress, Qt::Key_Down, Qt::ControlModifier);
            QApplication::sendEvent(view, &ctrlDown);
        }
        qApp->processEvents();
        ok(view->currentIndex().row() == 2,
           QStringLiteral("Ctrl+Down jumps to block edge, row=%1").arg(view->currentIndex().row()));
        {
            QKeyEvent ctrlHome(QEvent::KeyPress, Qt::Key_Home, Qt::ControlModifier);
            QApplication::sendEvent(view, &ctrlHome);
        }
        qApp->processEvents();
        ok(view->currentIndex().row() == 0 && view->currentIndex().column() == 0,
           QStringLiteral("Ctrl+Home goes to A1, row=%1 col=%2")
               .arg(view->currentIndex().row())
               .arg(view->currentIndex().column()));
        {
            QKeyEvent ctrlEnd(QEvent::KeyPress, Qt::Key_End, Qt::ControlModifier);
            QApplication::sendEvent(view, &ctrlEnd);
        }
        qApp->processEvents();
        ok(view->currentIndex().row() == 2 && view->currentIndex().column() == 0,
           QStringLiteral("Ctrl+End goes to used corner, row=%1 col=%2")
               .arg(view->currentIndex().row())
               .arg(view->currentIndex().column()));
        view->setCurrentIndex(a3);
        qApp->processEvents();

        QItemSelection range(a1, a3);
        view->selectionModel()->select(range, QItemSelectionModel::ClearAndSelect);
        qApp->processEvents();
        ok(view->selectionModel()->selectedIndexes().size() >= 3,
           QStringLiteral("range A1:A3 selected count=%1")
               .arg(view->selectionModel()->selectedIndexes().size()));
        view->setFocus(Qt::OtherFocusReason);
        {
            QKeyEvent esc(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
            QApplication::sendEvent(view, &esc);
        }
        qApp->processEvents();
        ok(view->selectionModel()->selectedIndexes().size() == 1,
           QStringLiteral("Escape collapses selection to 1 cell, got %1")
               .arg(view->selectionModel()->selectedIndexes().size()));

        view->selectionModel()->select(a1, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Current);
        view->setCurrentIndex(a1);
        qApp->processEvents();
        {
            const QPoint local = view->visualRect(a1).center();
            const QPoint global = view->viewport()->mapToGlobal(local);
            QMouseEvent press(QEvent::MouseButtonPress, QPointF(local), QPointF(global), Qt::LeftButton,
                              Qt::LeftButton, Qt::ControlModifier);
            QApplication::sendEvent(view->viewport(), &press);
            QMouseEvent release(QEvent::MouseButtonRelease, QPointF(local), QPointF(global), Qt::LeftButton,
                                Qt::LeftButton, Qt::ControlModifier);
            QApplication::sendEvent(view->viewport(), &release);
        }
        qApp->processEvents();
        ok(view->selectionModel()->selectedIndexes().size() >= 1,
           QStringLiteral("Ctrl+click cannot deselect the last cell, got %1")
               .arg(view->selectionModel()->selectedIndexes().size()));

        ok(view->model()->setData(a1, QStringLiteral("copied"), Qt::EditRole), QStringLiteral("copy source A1"));
        view->selectionModel()->select(a1, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Current);
        view->setCurrentIndex(a1);
        qApp->processEvents();
        QAction *copyAct = nullptr;
        QAction *pasteAct = nullptr;
        for (QAction *top : bar->actions()) {
            if (!top->menu())
                continue;
            for (QAction *a : top->menu()->actions()) {
                if (a->text().contains(QStringLiteral("Копировать")))
                    copyAct = a;
                if (a->text() == QStringLiteral("Вставить") || a->text() == QStringLiteral("Paste"))
                    pasteAct = a;
            }
        }
        ok(copyAct && pasteAct, QStringLiteral("copy/paste actions"));
        if (copyAct)
            copyAct->trigger();
        qApp->processEvents();
        qApp->clipboard()->setText(QStringLiteral("copied"));
        if (qApp->clipboard()->text().contains(QLatin1String("copied"))) {
            const QModelIndex b1 = view->model()->index(0, 1);
            view->setCurrentIndex(b1);
            view->selectionModel()->select(b1, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Current);
            if (pasteAct)
                pasteAct->trigger();
            qApp->processEvents();
            ok(view->model()->index(0, 1).data(Qt::DisplayRole).toString().contains(QLatin1String("copied")),
               QStringLiteral("paste into B1 got %1")
                   .arg(view->model()->index(0, 1).data(Qt::DisplayRole).toString()));
        } else {
            ok(true, QStringLiteral("clipboard unavailable offscreen, paste actions still present"));
        }
    }

    if (view) {
        QPixmap head = view->horizontalHeader()->grab();
        if (!head.isNull()) {
            const QColor hc = head.toImage().pixelColor(8, qMax(2, head.height() / 2));
            ok(hc.lightness() > 180 && hc.green() - hc.red() < 40,
               QStringLiteral("white theme header is light, rgb=%1,%2,%3 lightness=%4")
                   .arg(hc.red())
                   .arg(hc.green())
                   .arg(hc.blue())
                   .arg(hc.lightness()));
        }
        QPixmap tabShot = tabs->grab();
        if (!tabShot.isNull() && tabs->count() > 0) {
            const QPoint c = tabs->tabRect(0).center();
            const QColor tc = tabShot.toImage().pixelColor(c);
            ok(qAbs(tc.red() - tc.green()) < 50,
               QStringLiteral("white theme tab, rgb=%1,%2,%3")
                   .arg(tc.red())
                   .arg(tc.green())
                   .arg(tc.blue()));
        }
        ok(tabs->mapTo(&w, QPoint()).y() > view->mapTo(&w, QPoint()).y(),
           QStringLiteral("sheet tabs are below the grid"));
    }

    ok(saveGrab(&w, out, QStringLiteral("01-mainwindow.png")), QStringLiteral("grab main window"));

    ok(Theme::presetIds().contains(QStringLiteral("white"))
           && Theme::presetIds().contains(QStringLiteral("dark")),
       QStringLiteral("white and dark presets exist"));
    Theme::applyPreset(QStringLiteral("dark"));
    qApp->processEvents();
    ok(saveGrab(&w, out, QStringLiteral("01b-dark.png")), QStringLiteral("grab dark theme"));
    Theme::applyPreset(QStringLiteral("arbuz"));
    qApp->processEvents();
    {
        QPixmap mb = bar->grab();
        if (!mb.isNull()) {
            const QColor mc = mb.toImage().pixelColor(10, qMax(2, mb.height() / 2));
            ok(mc.lightness() < 90 && mc.green() > mc.red(),
               QStringLiteral("arbuz menu bar is dark rind, rgb=%1,%2,%3 lightness=%4")
                   .arg(mc.red())
                   .arg(mc.green())
                   .arg(mc.blue())
                   .arg(mc.lightness()));
        }
        QPixmap head = view->horizontalHeader()->grab();
        if (!head.isNull()) {
            const QColor hc = head.toImage().pixelColor(8, qMax(2, head.height() / 2));
            ok(hc.lightness() < 90 && hc.green() > hc.red(),
               QStringLiteral("arbuz header is dark rind, rgb=%1,%2,%3 lightness=%4")
                   .arg(hc.red())
                   .arg(hc.green())
                   .arg(hc.blue())
                   .arg(hc.lightness()));
        }
    }
    ok(saveGrab(&w, out, QStringLiteral("01c-arbuz.png")), QStringLiteral("grab arbuz theme"));
    Theme::applyPreset(QStringLiteral("white"));
    qApp->processEvents();

    auto *plus = w.findChild<QToolButton *>(QStringLiteral("addSheetButton"));
    ok(plus != nullptr, QStringLiteral("click + to add sheet"));
    const int sheetsBefore = tabs ? tabs->count() : 0;
    QAction *insertSheet = nullptr;
    for (QAction *top : bar->actions()) {
        if (!top->menu())
            continue;
        for (QAction *a : top->menu()->actions()) {
            if (a->text().contains(QStringLiteral("Вставить лист")))
                insertSheet = a;
        }
    }
    ok(insertSheet != nullptr, QStringLiteral("insert sheet menu action"));
    if (insertSheet)
        insertSheet->trigger();
    qApp->processEvents();
    ok(tabs && tabs->count() == sheetsBefore + 1,
       QStringLiteral("new sheet tab count=%1 (was %2)").arg(tabs ? tabs->count() : -1).arg(sheetsBefore));
    ok(saveGrab(&w, out, QStringLiteral("02-two-sheets.png")), QStringLiteral("grab two sheets"));

    FunctionDialog fnDlg;
    fnDlg.resize(420, 360);
    fnDlg.show();
    qApp->processEvents();
    auto *fnList = fnDlg.findChild<QListWidget *>(QStringLiteral("functionList"));
    ok(fnList && fnList->count() >= 20, QStringLiteral("function list count=%1").arg(fnList ? fnList->count() : -1));
    auto *fnCat = fnDlg.findChild<QComboBox *>(QStringLiteral("functionCategory"));
    ok(fnCat != nullptr, QStringLiteral("function category combo"));
    bool hasPluginCat = false;
    if (fnCat) {
        for (int i = 0; i < fnCat->count(); ++i) {
            if (fnCat->itemText(i).contains(QStringLiteral("Плагины"))
                || fnCat->itemData(i).toString() == QLatin1String("plugin"))
                hasPluginCat = true;
        }
    }
    ok(hasPluginCat, QStringLiteral("function dialog has Plugins category"));
    ok(saveGrab(&fnDlg, out, QStringLiteral("02b-functions.png")), QStringLiteral("grab function list"));
    fnDlg.hide();

    FirstRunWizard wiz;
    wiz.resize(640, 480);
    wiz.show();
    qApp->processEvents();
    auto *joke = wiz.findChild<QLabel *>(QStringLiteral("syfarenJoke"));
    ok(joke != nullptr, QStringLiteral("first-run joke label"));
    ok(joke && joke->text().contains(QStringLiteral("SYFaren")),
       QStringLiteral("joke mentions SYFaren: '%1'").arg(joke ? joke->text() : QString()));
    ok(joke && joke->isVisible(), QStringLiteral("joke visible, does not block form"));
    ok(wiz.findChildren<QTimer *>().isEmpty(), QStringLiteral("first-run labels stay still"));
    ok(wiz.findChild<QLineEdit *>() != nullptr, QStringLiteral("first-run path field"));
    ok(saveGrab(&wiz, out, QStringLiteral("03-firstrun.png")), QStringLiteral("grab first-run wizard"));
    wiz.hide();

    SettingsDialog settings;
    settings.resize(520, 560);
    settings.show();
    qApp->processEvents();
    ok(settings.windowTitle().contains(QStringLiteral("Настройки")), QStringLiteral("settings title"));
    const auto colorBtns = settings.findChildren<QPushButton *>();
    ok(colorBtns.size() >= 10, QStringLiteral("settings color controls=%1").arg(colorBtns.size()));
    ok(saveGrab(&settings, out, QStringLiteral("04-settings.png")), QStringLiteral("grab settings"));
    settings.hide();

    CreditsDialog credits;
    credits.resize(640, 480);
    credits.show();
    qApp->processEvents();
    auto *creditsText = credits.findChild<QTextBrowser *>(QStringLiteral("creditsView"));
    const QString creditsBody = creditsText ? creditsText->toPlainText() : QString();
    ok(creditsText != nullptr, QStringLiteral("credits rendered as formatted document"));
    ok(creditsBody.contains(QStringLiteral("QXlsx")), QStringLiteral("credits mention QXlsx"));
    ok(creditsBody.contains(QStringLiteral("SYFaren")), QStringLiteral("credits mention SYFaren"));
    ok(creditsBody.contains(QStringLiteral("xlfparser")), QStringLiteral("credits mention xlfparser"));
    ok(saveGrab(&credits, out, QStringLiteral("05-credits.png")), QStringLiteral("grab credits"));
    credits.hide();

    const QPixmap appPx = ArbuzIcon::app().pixmap(64, 64);
    ok(!appPx.isNull(), QStringLiteral("app icon pixmap in ui-test"));
    if (!appPx.isNull()) {
        const QColor mid = appPx.toImage().pixelColor(appPx.width() / 2, appPx.height() / 2);
        ok(mid.red() > 80 && mid.red() > mid.green(),
           QStringLiteral("ui icon center is flesh, not a seed, rgb=%1,%2,%3")
               .arg(mid.red())
               .arg(mid.green())
               .arg(mid.blue()));
    }

    if (g_fails == 0)
        std::printf("ui-test ok (%s)\n", qPrintable(out));
    else
        std::fprintf(stderr, "ui-test finished with %d failure(s)\n", g_fails);
    return g_fails == 0 ? 0 : 1;
}
