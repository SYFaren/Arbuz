#include "portable.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

namespace Arbuz {

static bool looksLikeUserRoot(const QDir &d)
{
    return QDir(d.filePath(QStringLiteral("plugins"))).exists()
        || QDir(d.filePath(QStringLiteral("python-plugins"))).exists()
        || QDir(d.filePath(QStringLiteral("languages"))).exists()
        || QDir(d.filePath(QStringLiteral("i18n"))).exists()
        || QDir(d.filePath(QStringLiteral("themes"))).exists();
}

QString portableRoot()
{
    const QString env = QString::fromLocal8Bit(qgetenv("ARBUZ_PORTABLE_ROOT"));
    if (!env.isEmpty())
        return env;
    if (!QCoreApplication::instance())
        return {};
    QDir app(QCoreApplication::applicationDirPath());
    if (looksLikeUserRoot(app))
        return app.absolutePath();
    QDir parent(app.filePath(QStringLiteral("..")));
    if (looksLikeUserRoot(parent))
        return QFileInfo(app.filePath(QStringLiteral(".."))).absoluteFilePath();
    return app.absolutePath();
}

}
