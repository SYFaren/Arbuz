#ifndef ARBUZ_I18N_H
#define ARBUZ_I18N_H

#include <QString>

namespace I18n {
QString lang();
void setLang(const QString &code);
QString t(const char *ru, const char *en);
}

#endif
