#include "i18n.h"
#include "appsettings.h"

namespace I18n {

QString lang()
{
    return AppSettings::instance().language();
}

void setLang(const QString &code)
{
    AppSettings::instance().setLanguage(code);
}

QString t(const char *ru, const char *en)
{
    if (lang() == QLatin1String("en"))
        return QString::fromUtf8(en);
    return QString::fromUtf8(ru);
}

}
