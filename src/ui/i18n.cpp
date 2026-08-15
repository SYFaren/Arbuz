#include "i18n.h"
#include "appsettings.h"
#include "portable.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

namespace I18n {
namespace {

QHash<QString, QHash<QString, QString>> g_catalogs;
QHash<QString, QString> g_names;
QHash<QString, QString> g_creditsIds;
bool g_loaded = false;

void mergeObject(const QString &code, const QJsonObject &obj)
{
    QHash<QString, QString> &cat = g_catalogs[code];
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        if (it.key() == QLatin1String("_meta"))
            continue;
        if (it.value().isString())
            cat.insert(it.key(), it.value().toString());
    }
    const QJsonObject meta = obj.value(QStringLiteral("_meta")).toObject();
    const QString name = meta.value(QStringLiteral("name")).toString();
    if (!name.isEmpty())
        g_names.insert(code, name);
    else if (!g_names.contains(code))
        g_names.insert(code, code);
    const QString credits = meta.value(QStringLiteral("credits")).toString();
    if (!credits.isEmpty())
        g_creditsIds.insert(code, credits);
}

bool loadFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject())
        return false;
    const QJsonObject obj = doc.object();
    QString code = obj.value(QStringLiteral("_meta")).toObject().value(QStringLiteral("code")).toString();
    if (code.isEmpty()) {
        const QFileInfo fi(path);
        code = fi.completeBaseName();
    }
    if (code.isEmpty())
        return false;
    mergeObject(code, obj);
    return true;
}

void loadDir(const QString &dirPath)
{
    QDir dir(dirPath);
    if (!dir.exists())
        return;
    const QStringList files = dir.entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
    for (const QString &name : files)
        loadFile(dir.filePath(name));
}

QStringList searchDirs()
{
    QStringList dirs;
    const QString root = Arbuz::portableRoot();
    if (!root.isEmpty()) {
        dirs.append(root + QStringLiteral("/languages"));
        dirs.append(root + QStringLiteral("/i18n"));
    }
    if (QCoreApplication::instance()) {
        const QString appDir = QCoreApplication::applicationDirPath();
        dirs.append(appDir + QStringLiteral("/languages"));
        dirs.append(appDir + QStringLiteral("/../languages"));
        dirs.append(appDir + QStringLiteral("/i18n"));
        dirs.append(appDir + QStringLiteral("/../i18n"));
    }
    dirs.removeDuplicates();
    return dirs;
}

void ensureLoaded()
{
    if (!g_loaded)
        reload();
}

} // namespace

QString lang()
{
    return AppSettings::instance().language();
}

void setLang(const QString &code)
{
    AppSettings::instance().setLanguage(code);
}

void reload()
{
    g_catalogs.clear();
    g_names.clear();
    g_creditsIds.clear();

    // Bundled defaults first.
    loadFile(QStringLiteral(":/arbuz/i18n/en.json"));
    loadFile(QStringLiteral(":/arbuz/i18n/ru.json"));

    // On-disk overrides / extra languages (portable root, next to binary, …).
    for (const QString &dir : searchDirs())
        loadDir(dir);

    if (!g_names.contains(QStringLiteral("en")))
        g_names.insert(QStringLiteral("en"), QStringLiteral("English"));
    if (!g_names.contains(QStringLiteral("ru")))
        g_names.insert(QStringLiteral("ru"), QStringLiteral("Русский"));

    g_loaded = true;
}

QString lookup(const QString &langCode, const QString &key)
{
    ensureLoaded();
    const auto catIt = g_catalogs.constFind(langCode);
    if (catIt == g_catalogs.cend())
        return {};
    return catIt->value(key);
}

QString t(const QString &key)
{
    ensureLoaded();
    const QString cur = lang();
    QString v = lookup(cur, key);
    if (!v.isEmpty())
        return v;
    if (cur != QLatin1String("en")) {
        v = lookup(QStringLiteral("en"), key);
        if (!v.isEmpty())
            return v;
    }
    return key;
}

QString t(const char *key)
{
    return t(QString::fromUtf8(key));
}

QVector<Language> availableLanguages()
{
    ensureLoaded();
    QVector<Language> out;
    QSet<QString> seen;
    auto add = [&](const QString &code) {
        if (seen.contains(code) || code.isEmpty())
            return;
        seen.insert(code);
        Language L;
        L.code = code;
        L.name = g_names.value(code, code);
        out.append(L);
    };
    // Prefer ru then en, then the rest alphabetically.
    add(QStringLiteral("ru"));
    add(QStringLiteral("en"));
    QStringList rest = g_catalogs.keys();
    rest.sort(Qt::CaseInsensitive);
    for (const QString &code : rest)
        add(code);
    return out;
}

QString creditsId()
{
    ensureLoaded();
    const QString code = lang();
    const QString id = g_creditsIds.value(code);
    return id.isEmpty() ? code : id;
}

static QString readUtf8(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QString::fromUtf8(f.readAll());
}

static QStringList creditsSearchDirs()
{
    QStringList dirs;
    const QString root = Arbuz::portableRoot();
    if (!root.isEmpty()) {
        dirs.append(root);
        dirs.append(root + QStringLiteral("/languages"));
    }
    if (QCoreApplication::instance()) {
        const QString appDir = QCoreApplication::applicationDirPath();
        dirs.append(appDir);
        dirs.append(appDir + QStringLiteral("/languages"));
        dirs.append(appDir + QStringLiteral("/../languages"));
        dirs.append(QFileInfo(appDir + QStringLiteral("/..")).absoluteFilePath());
    }
    dirs.removeDuplicates();
    return dirs;
}

static QString creditsFileFor(const QString &id)
{
    const QStringList names = {
        QStringLiteral("CREDITS.%1.md").arg(id),
        QStringLiteral("credits.%1.md").arg(id),
    };
    for (const QString &dir : creditsSearchDirs()) {
        for (const QString &name : names) {
            const QString md = readUtf8(dir + QLatin1Char('/') + name);
            if (!md.isEmpty())
                return md;
        }
        if (id == QLatin1String("en")) {
            const QString md = readUtf8(dir + QStringLiteral("/CREDITS.md"));
            if (!md.isEmpty())
                return md;
        }
        if (id == QLatin1String("ru")) {
            const QString md = readUtf8(dir + QStringLiteral("/CREDITS.ru.md"));
            if (!md.isEmpty())
                return md;
        }
    }
    QString md = readUtf8(QStringLiteral(":/arbuz/credits/%1.md").arg(id));
    if (!md.isEmpty())
        return md;
    if (id == QLatin1String("en"))
        md = readUtf8(QStringLiteral(":/arbuz/CREDITS.md"));
    return md;
}

QString creditsMarkdown()
{
    ensureLoaded();
    QString md = creditsFileFor(creditsId());
    if (md.isEmpty() && creditsId() != QLatin1String("en"))
        md = creditsFileFor(QStringLiteral("en"));
    return md;
}

}
