#include "theme.h"
#include "appsettings.h"
#include "i18n.h"
#include "portable.h"

#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPalette>
#include <QStandardPaths>
#include <QStyle>
#include <QStyleFactory>

static QColor jsonColor(const QJsonObject &o, const QString &key, const QColor &fallback)
{
    if (!o.contains(key))
        return fallback;
    const QColor c(o.value(key).toString());
    return c.isValid() ? c : fallback;
}

static QHash<QString, QColor> hardcoded(const QString &id)
{
    QHash<QString, QColor> p;
    if (id == QLatin1String("dark")) {
        p[QStringLiteral("flesh")] = QColor("#EDEDED");
        p[QStringLiteral("fleshHot")] = QColor("#CCCCCC");
        p[QStringLiteral("rind")] = QColor("#1A1A1A");
        p[QStringLiteral("rindStripe")] = QColor("#2A2A2A");
        p[QStringLiteral("seed")] = QColor("#EDEDED");
        p[QStringLiteral("pith")] = QColor("#141414");
        p[QStringLiteral("windowBg")] = QColor("#111111");
        p[QStringLiteral("headerText")] = QColor("#EDEDED");
        p[QStringLiteral("gridLine")] = QColor("#2A2A2A");
        p[QStringLiteral("selection")] = QColor("#1F4E79");
        p[QStringLiteral("selectionText")] = QColor("#F4F8FC");
        p[QStringLiteral("selectionBorder")] = QColor("#79B8F2");
        p[QStringLiteral("formulaBar")] = QColor("#1A1A1A");
        p[QStringLiteral("button")] = QColor("#1C1C1C");
        p[QStringLiteral("buttonText")] = QColor("#EDEDED");
        p[QStringLiteral("tabActive")] = QColor("#141414");
        p[QStringLiteral("tabInactive")] = QColor("#1A1A1A");
        p[QStringLiteral("menuBg")] = QColor("#161616");
        p[QStringLiteral("statusBg")] = QColor("#111111");
        return p;
    }
    if (id == QLatin1String("arbuz")) {
        p[QStringLiteral("flesh")] = QColor("#B83A45");
        p[QStringLiteral("fleshHot")] = QColor("#D45A62");
        p[QStringLiteral("rind")] = QColor("#2A3F32");
        p[QStringLiteral("rindStripe")] = QColor("#3D5645");
        p[QStringLiteral("seed")] = QColor("#1E1714");
        p[QStringLiteral("pith")] = QColor("#FFF8F2");
        p[QStringLiteral("windowBg")] = QColor("#F3EBE3");
        p[QStringLiteral("headerText")] = QColor("#F4EFE6");
        p[QStringLiteral("gridLine")] = QColor("#E4D4C6");
        p[QStringLiteral("selection")] = QColor("#F0D4D6");
        p[QStringLiteral("selectionText")] = QColor("#1E1714");
        p[QStringLiteral("selectionBorder")] = QColor("#B83A45");
        p[QStringLiteral("formulaBar")] = QColor("#FFFCF8");
        p[QStringLiteral("button")] = QColor("#2A3F32");
        p[QStringLiteral("buttonText")] = QColor("#F4EFE6");
        p[QStringLiteral("tabActive")] = QColor("#FFF8F2");
        p[QStringLiteral("tabInactive")] = QColor("#DCCFC2");
        p[QStringLiteral("menuBg")] = QColor("#2A3F32");
        p[QStringLiteral("statusBg")] = QColor("#24362C");
        return p;
    }
    p[QStringLiteral("flesh")] = QColor("#111111");
    p[QStringLiteral("fleshHot")] = QColor("#333333");
    p[QStringLiteral("rind")] = QColor("#F2F2F2");
    p[QStringLiteral("rindStripe")] = QColor("#D9D9D9");
    p[QStringLiteral("seed")] = QColor("#111111");
    p[QStringLiteral("pith")] = QColor("#FFFFFF");
    p[QStringLiteral("windowBg")] = QColor("#FFFFFF");
    p[QStringLiteral("headerText")] = QColor("#111111");
    p[QStringLiteral("gridLine")] = QColor("#D0D0D0");
    p[QStringLiteral("selection")] = QColor("#C5DDF8");
    p[QStringLiteral("selectionText")] = QColor("#111111");
    p[QStringLiteral("selectionBorder")] = QColor("#2B6CB0");
    p[QStringLiteral("formulaBar")] = QColor("#FFFFFF");
    p[QStringLiteral("button")] = QColor("#F5F5F5");
    p[QStringLiteral("buttonText")] = QColor("#111111");
    p[QStringLiteral("tabActive")] = QColor("#FFFFFF");
    p[QStringLiteral("tabInactive")] = QColor("#E8E8E8");
    p[QStringLiteral("menuBg")] = QColor("#FFFFFF");
    p[QStringLiteral("statusBg")] = QColor("#F0F0F0");
    return p;
}

QStringList Theme::builtinIds()
{
    return {QStringLiteral("white"), QStringLiteral("dark"), QStringLiteral("arbuz")};
}

QString Theme::userThemesDir()
{
    const QByteArray env = qgetenv("ARBUZ_PORTABLE_ROOT");
    if (!env.isEmpty()) {
        const QString d = QString::fromLocal8Bit(env) + QStringLiteral("/themes");
        QDir().mkpath(d);
        return d;
    }
    const QString root = Arbuz::portableRoot();
    const QString portable = root + QStringLiteral("/themes");
    if (QDir(portable).exists())
        return portable;
    const QString cfg = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
        + QStringLiteral("/themes");
    QDir().mkpath(cfg);
    return cfg;
}

QString Theme::idFromTitle(const QString &title)
{
    QString id;
    for (const QChar &ch : title.toLower()) {
        if (ch.isLetterOrNumber())
            id += ch;
        else if (!id.isEmpty() && !id.endsWith(QLatin1Char('-')))
            id += QLatin1Char('-');
    }
    while (id.endsWith(QLatin1Char('-')))
        id.chop(1);
    if (id.isEmpty())
        id = QStringLiteral("theme");
    if (builtinIds().contains(id))
        id = QStringLiteral("user-") + id;
    return id;
}

static QStringList userThemeIds()
{
    QStringList ids;
    QDir dir(Theme::userThemesDir());
    const QFileInfoList files = dir.entryInfoList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
    for (const QFileInfo &fi : files)
        ids.append(fi.completeBaseName());
    return ids;
}

QStringList Theme::presetIds()
{
    QStringList ids = builtinIds();
    for (const QString &id : userThemeIds()) {
        if (!ids.contains(id))
            ids.append(id);
    }
    return ids;
}

bool Theme::isBuiltin(const QString &id)
{
    return builtinIds().contains(id);
}

QString Theme::presetTitle(const QString &id)
{
    if (id == QLatin1String("dark"))
        return I18n::t("ui.dark");
    if (id == QLatin1String("arbuz"))
        return I18n::t("ui.watermelon");
    if (id == QLatin1String("white"))
        return I18n::t("ui.white");
    QFile f(userThemesDir() + QLatin1Char('/') + id + QStringLiteral(".json"));
    if (f.open(QIODevice::ReadOnly)) {
        const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
        const QString n = o.value(QStringLiteral("name")).toString();
        if (!n.isEmpty())
            return n;
    }
    return id;
}

void Theme::fillPresetCombo(QComboBox *box, const QString &currentId)
{
    if (!box)
        return;
    box->clear();
    for (const QString &id : presetIds()) {
        box->addItem(presetTitle(id), id);
        if (id == currentId)
            box->setCurrentIndex(box->count() - 1);
    }
}

QHash<QString, QColor> Theme::paletteFor(const QString &id)
{
    QHash<QString, QColor> p = hardcoded(isBuiltin(id) ? id : QStringLiteral("white"));
    QString path = QStringLiteral(":/arbuz/themes/%1.json").arg(isBuiltin(id) ? id : QStringLiteral("white"));
    QFile builtin(path);
    if (builtin.open(QIODevice::ReadOnly)) {
        const QJsonObject o = QJsonDocument::fromJson(builtin.readAll()).object();
        for (auto it = p.begin(); it != p.end(); ++it)
            it.value() = jsonColor(o, it.key(), it.value());
    }
    QFile disk(userThemesDir() + QLatin1Char('/') + id + QStringLiteral(".json"));
    if (disk.open(QIODevice::ReadOnly)) {
        const QJsonObject o = QJsonDocument::fromJson(disk.readAll()).object();
        for (auto it = p.begin(); it != p.end(); ++it)
            it.value() = jsonColor(o, it.key(), it.value());
    }
    if (!p.contains(QStringLiteral("selectionBorder")))
        p.insert(QStringLiteral("selectionBorder"), p.value(QStringLiteral("flesh")));
    return p;
}

QHash<QString, QColor> Theme::defaultPalette()
{
    return paletteFor(QStringLiteral("white"));
}

QStringList Theme::colorRoles()
{
    return defaultPalette().keys();
}

QString Theme::roleTitle(const QString &role)
{
    const QString key = QStringLiteral("theme.role.") + role;
    const QString title = I18n::t(key);
    return title == key ? role : title;
}

QHash<QString, QColor> Theme::currentPalette()
{
    QHash<QString, QColor> p = paletteFor(AppSettings::instance().themePreset());
    const auto ov = AppSettings::instance().colorOverrides();
    for (auto it = ov.cbegin(); it != ov.cend(); ++it)
        p.insert(it.key(), it.value());
    return p;
}

QString Theme::stylesheet(const QHash<QString, QColor> &p)
{
    QString css = QStringLiteral(
        "QMainWindow { background-color: @windowBg@; color: @seed@; }\n"
        "QDialog, QWizard { background-color: @windowBg@; }\n"
        "QMessageBox QLabel { min-width: 280px; padding: 2px; }\n"
        "QMenuBar { background-color: @menuBg@; color: @headerText@; border-bottom: 1px solid @rindStripe@; }\n"
        "QMenuBar::item { color: @headerText@; padding: 5px 10px; background: transparent; }\n"
        "QMenuBar::item:selected { background-color: @rindStripe@; color: @headerText@; }\n"
        "QMenu { background-color: @pith@; color: @seed@; border: 1px solid @gridLine@; padding: 4px; }\n"
        "QMenu::item { padding: 5px 24px 5px 32px; color: @seed@; }\n"
        "QMenu::item:selected { background-color: @selection@; color: @selectionText@; }\n"
        "QMenu::icon { width: 20px; }\n"
        "QMenu::separator { height: 1px; background: @gridLine@; margin: 3px 8px; }\n"
        "QTableView { background-color: @pith@; alternate-background-color: @pith@; "
        "gridline-color: @gridLine@; color: @seed@; selection-background-color: @selection@; "
        "selection-color: @selectionText@; outline: 0; }\n"
        "QTextBrowser { background-color: @pith@; color: @seed@; border: 1px solid @gridLine@; "
        "padding: 8px; }\n"
        "QDockWidget { color: @seed@; titlebar-close-icon: none; }\n"
        "QDockWidget::title { background: @rind@; color: @headerText@; padding: 4px; }\n"
        "QHeaderView { background-color: @rind@; color: @headerText@; }\n"
        "QHeaderView::section { background-color: @rind@; color: @headerText@; padding: 4px 6px; "
        "border: none; border-right: 1px solid @rindStripe@; border-bottom: 1px solid @rindStripe@; "
        "font-weight: normal; }\n"
        "QTableCornerButton::section { background: @rind@; border: none; "
        "border-right: 1px solid @rindStripe@; border-bottom: 1px solid @rindStripe@; }\n"
        "QLineEdit, QPlainTextEdit { background-color: @formulaBar@; "
        "color: @seed@; border: 1px solid @gridLine@; padding: 3px 6px; border-radius: 0; }\n"
        "QListWidget { background-color: @formulaBar@; color: @seed@; border: 1px solid @gridLine@; "
        "padding: 2px; border-radius: 0; }\n"
        "QComboBox, QSpinBox { background-color: @formulaBar@; color: @seed@; "
        "border: 1px solid @gridLine@; padding: 2px 6px; min-height: 1.25em; border-radius: 0; }\n"
        "QComboBox::drop-down { subcontrol-origin: border; subcontrol-position: center right; "
        "width: 18px; border: none; }\n"
        "QSpinBox::up-button, QSpinBox::down-button { width: 16px; }\n"
        "QComboBox QAbstractItemView { background-color: @pith@; color: @seed@; "
        "selection-background-color: @selection@; selection-color: @selectionText@; padding: 2px; }\n"
        "QWidget#sheetBar { background: @rind@; border-top: 1px solid @rindStripe@; }\n"
        "QPushButton { background-color: @button@; color: @buttonText@; border: 1px solid @rindStripe@; "
        "padding: 5px 12px; border-radius: 0; }\n"
        "QPushButton:hover { background-color: @rindStripe@; color: @buttonText@; }\n"
        "QPushButton:default { background-color: @selection@; color: @selectionText@; border: 1px solid @selectionBorder@; }\n"
        "QToolButton { background: transparent; color: @seed@; border: 1px solid transparent; "
        "padding: 3px 6px; border-radius: 0; }\n"
        "QToolButton:hover { background: @selection@; border: 1px solid @gridLine@; }\n"
        "QWidget#sheetBar QToolButton { color: @headerText@; }\n"
        "QWidget#sheetBar QToolButton:hover { background: @rindStripe@; border: 1px solid @rindStripe@; "
        "color: @headerText@; }\n"
        "QToolButton#addSheetButton { padding: 2px; }\n"
        "QTabBar::tab { background: @tabInactive@; color: @seed@; "
        "padding: 5px 14px; margin-right: 1px; border: 1px solid @gridLine@; "
        "border-bottom: none; min-width: 48px; min-height: 1.2em; }\n"
        "QTabBar::tab:selected { background: @tabActive@; color: @seed@; "
        "border-top: 2px solid @flesh@; }\n"
        "QTabBar::tab:!selected { background: @tabInactive@; color: @seed@; }\n"
        "QStatusBar { background: @statusBg@; color: @headerText@; border-top: 1px solid @rindStripe@; "
        "padding: 3px 10px 4px 8px; min-height: 1.3em; }\n"
        "QStatusBar::item { border: none; }\n"
        "QScrollBar:vertical { background: @windowBg@; width: 12px; margin: 0; border: none; }\n"
        "QScrollBar::handle:vertical { background: @rindStripe@; min-height: 24px; }\n"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: @windowBg@; }\n"
        "QScrollBar:horizontal { background: @windowBg@; height: 12px; margin: 0; border: none; }\n"
        "QScrollBar::handle:horizontal { background: @rindStripe@; min-width: 24px; }\n"
        "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: @windowBg@; }\n"
        "QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; border: none; background: none; }\n"
        "QAbstractScrollArea::corner { background: @windowBg@; }\n"
        "QToolTip { background: @pith@; color: @seed@; border: 1px solid @gridLine@; "
        "padding: 6px 10px; }\n"
        "QLabel#functionSyntax, QLabel#functionHelp { padding: 2px 2px 4px 2px; min-height: 1.2em; }\n"
        "QLabel#syfarenJoke { color: @flesh@; font-style: italic; padding-bottom: 4px; }\n");
    const QStringList keys = p.keys();
    for (const QString &k : keys)
        css.replace(QLatin1Char('@') + k + QLatin1Char('@'), p.value(k).name(QColor::HexRgb));
    return css;
}

void Theme::apply(QApplication *app)
{
    // setStyle() deletes the previous style while live widgets may still reference it.
    static bool fusionSet = false;
    if (!fusionSet) {
        if (QStyle *fusion = QStyleFactory::create(QStringLiteral("Fusion")))
            app->setStyle(fusion);
        fusionSet = true;
    }
    const auto p = currentPalette();
    QPalette pal = app->palette();
    pal.setColor(QPalette::Window, p.value(QStringLiteral("windowBg")));
    pal.setColor(QPalette::WindowText, p.value(QStringLiteral("seed")));
    pal.setColor(QPalette::Base, p.value(QStringLiteral("pith")));
    pal.setColor(QPalette::Text, p.value(QStringLiteral("seed")));
    pal.setColor(QPalette::Button, p.value(QStringLiteral("button")));
    pal.setColor(QPalette::ButtonText, p.value(QStringLiteral("buttonText")));
    pal.setColor(QPalette::Highlight, p.value(QStringLiteral("selection")));
    pal.setColor(QPalette::HighlightedText, p.value(QStringLiteral("selectionText")));
    app->setPalette(pal);
    app->setStyleSheet(stylesheet(p));
}

void Theme::applyPreset(const QString &id)
{
    AppSettings::instance().setThemePreset(id);
    AppSettings::instance().clearColorOverrides();
    if (qApp)
        apply(qApp);
}

QString Theme::saveUserTheme(const QString &title, const QHash<QString, QColor> &palette)
{
    const QString dir = userThemesDir();
    QDir().mkpath(dir);
    QString id = idFromTitle(title);
    const QString path = dir + QLatin1Char('/') + id + QStringLiteral(".json");
    QJsonObject o;
    o.insert(QStringLiteral("name"), title);
    for (auto it = palette.cbegin(); it != palette.cend(); ++it)
        o.insert(it.key(), it.value().name(QColor::HexRgb));
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return {};
    f.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
    return id;
}

bool Theme::deleteUserTheme(const QString &id)
{
    if (isBuiltin(id))
        return false;
    return QFile::remove(userThemesDir() + QLatin1Char('/') + id + QStringLiteral(".json"));
}
