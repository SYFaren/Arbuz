#ifndef ARBUZ_ICON_H
#define ARBUZ_ICON_H

#include <QIcon>
#include <QString>

namespace ArbuzIcon {
QIcon app();
QIcon named(const QString &id);
QIcon fx();
QIcon addSheet();
}

#endif
