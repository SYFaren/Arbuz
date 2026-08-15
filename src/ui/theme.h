#ifndef ARBUZ_THEME_H
#define ARBUZ_THEME_H

#include <QColor>
#include <QHash>
#include <QString>
#include <QStringList>

class QApplication;
class QComboBox;

class Theme
{
public:
    static QStringList builtinIds();
    static QStringList presetIds();
    static QString presetTitle(const QString &id);
    static bool isBuiltin(const QString &id);
    static void fillPresetCombo(QComboBox *box, const QString &currentId);

    static QHash<QString, QColor> paletteFor(const QString &id);
    static QHash<QString, QColor> defaultPalette();
    static QHash<QString, QColor> currentPalette();
    static QStringList colorRoles();
    static QString roleTitle(const QString &role);
    static void apply(QApplication *app);
    static void applyPreset(const QString &id);
    static QString stylesheet(const QHash<QString, QColor> &p);

    static QString userThemesDir();
    static QString saveUserTheme(const QString &title, const QHash<QString, QColor> &palette);
    static bool deleteUserTheme(const QString &id);
    static QString idFromTitle(const QString &title);
};

#endif
