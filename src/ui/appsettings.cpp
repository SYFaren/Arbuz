#include "appsettings.h"

#include <QDir>
#include <QLocale>
#include <QStandardPaths>

AppSettings &AppSettings::instance()
{
    static AppSettings s;
    return s;
}

AppSettings::AppSettings()
    : m_s(QStringLiteral("SYFaren"), QStringLiteral("Arbuz"))
{
}

bool AppSettings::firstRunDone() const
{
    return m_s.value(QStringLiteral("firstRunDone"), false).toBool();
}

void AppSettings::setFirstRunDone(bool done)
{
    m_s.setValue(QStringLiteral("firstRunDone"), done);
    m_s.sync();
}

QString AppSettings::language() const
{
    if (m_s.contains(QStringLiteral("language")))
        return m_s.value(QStringLiteral("language")).toString();
    // First launch / unset: follow the OS UI language (RU → ru, everything else → en).
    return QLocale::system().language() == QLocale::Russian ? QStringLiteral("ru")
                                                            : QStringLiteral("en");
}

void AppSettings::setLanguage(const QString &code)
{
    m_s.setValue(QStringLiteral("language"), code);
}

QString AppSettings::documentsPath() const
{
    const QString def = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    return m_s.value(QStringLiteral("documentsPath"), def).toString();
}

void AppSettings::setDocumentsPath(const QString &path)
{
    m_s.setValue(QStringLiteral("documentsPath"), path);
}

int AppSettings::uiScalePercent() const
{
    return m_s.value(QStringLiteral("uiScalePercent"), 100).toInt();
}

void AppSettings::setUiScalePercent(int percent)
{
    m_s.setValue(QStringLiteral("uiScalePercent"), percent);
}

QString AppSettings::themePreset() const
{
    if (m_s.contains(QStringLiteral("themePreset")))
        return m_s.value(QStringLiteral("themePreset")).toString();
    if (!colorOverrides().isEmpty())
        return QStringLiteral("arbuz");
    return QStringLiteral("white");
}

void AppSettings::setThemePreset(const QString &id)
{
    m_s.setValue(QStringLiteral("themePreset"), id);
}

QHash<QString, QColor> AppSettings::colorOverrides() const
{
    QHash<QString, QColor> out;
    m_s.beginGroup(QStringLiteral("colors"));
    const QStringList keys = m_s.childKeys();
    for (const QString &k : keys) {
        const QColor c(m_s.value(k).toString());
        if (c.isValid())
            out.insert(k, c);
    }
    m_s.endGroup();
    return out;
}

void AppSettings::setColorOverride(const QString &role, const QColor &color)
{
    m_s.setValue(QStringLiteral("colors/") + role, color.name(QColor::HexRgb));
}

void AppSettings::clearColorOverrides()
{
    m_s.remove(QStringLiteral("colors"));
}

QString AppSettings::lastFile() const
{
    return m_s.value(QStringLiteral("lastFile")).toString();
}

void AppSettings::setLastFile(const QString &path)
{
    m_s.setValue(QStringLiteral("lastFile"), path);
}
