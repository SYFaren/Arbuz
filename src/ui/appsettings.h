#ifndef ARBUZ_APPSETTINGS_H
#define ARBUZ_APPSETTINGS_H

#include <QColor>
#include <QHash>
#include <QSettings>
#include <QString>

class AppSettings
{
public:
    static AppSettings &instance();

    bool firstRunDone() const;
    void setFirstRunDone(bool done);

    QString language() const;
    void setLanguage(const QString &code);

    QString documentsPath() const;
    void setDocumentsPath(const QString &path);

    int uiScalePercent() const;
    void setUiScalePercent(int percent);

    QString themePreset() const;
    void setThemePreset(const QString &id);

    QHash<QString, QColor> colorOverrides() const;
    void setColorOverride(const QString &role, const QColor &color);
    void clearColorOverrides();

    QString lastFile() const;
    void setLastFile(const QString &path);

private:
    AppSettings();
    mutable QSettings m_s;
};

#endif
