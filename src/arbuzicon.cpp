#include "arbuzicon.h"

QIcon ArbuzIcon::app()
{
    QIcon icon;
    icon.addFile(QStringLiteral(":/arbuz/icons/arbuz-16.png"));
    icon.addFile(QStringLiteral(":/arbuz/icons/arbuz-24.png"));
    icon.addFile(QStringLiteral(":/arbuz/icons/arbuz-32.png"));
    icon.addFile(QStringLiteral(":/arbuz/icons/arbuz-48.png"));
    icon.addFile(QStringLiteral(":/arbuz/icons/arbuz-64.png"));
    icon.addFile(QStringLiteral(":/arbuz/icons/arbuz-128.png"));
    icon.addFile(QStringLiteral(":/arbuz/icons/arbuz-256.png"));
    return icon;
}

QIcon ArbuzIcon::named(const QString &id)
{
    QIcon icon;
    const QString base = QStringLiteral(":/arbuz/icons/") + id;
    icon.addFile(base + QStringLiteral("-16.png"));
    icon.addFile(base + QStringLiteral("-32.png"));
    icon.addFile(base + QStringLiteral("-64.png"));
    return icon;
}

QIcon ArbuzIcon::fx()
{
    return named(QStringLiteral("fx"));
}

QIcon ArbuzIcon::addSheet()
{
    return named(QStringLiteral("add-sheet"));
}
