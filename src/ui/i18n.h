#ifndef ARBUZ_I18N_H
#define ARBUZ_I18N_H

#include <QString>
#include <QVector>

namespace I18n {

struct Language {
    QString code;
    QString name;
};

QString lang();
void setLang(const QString &code);

/** Reload bundled + on-disk JSON catalogs (qrc and portable languages/). */
void reload();

/** Translate by key; falls back to English, then to the key itself. */
QString t(const QString &key);
QString t(const char *key);

/** Look up a key in a specific language catalog (no current-lang switch). */
QString lookup(const QString &langCode, const QString &key);

QVector<Language> availableLanguages();

/** Credits document id from language `_meta.credits` (falls back to the language code). */
QString creditsId();

/** Markdown for Help → Credits: disk override, then bundled, then English. */
QString creditsMarkdown();

}

#endif
