// Localized UI strings taken from the upstream Resources.resw files.
// The build extracts the keys the app needs into one JSON resource
// (see tools/extract-strings.py). Lookups fall back to en-US.
#pragma once

#include <QHash>
#include <QLocale>
#include <QString>
#include <QStringList>

class AppStrings
{
public:
    static QString get(const QString& key);

    // Picks the best upstream locale folder ("pt-BR") for the user's UI
    // languages. Tries an exact match, then any folder of the same language.
    static QString pickLocale(const QStringList& available, const QLocale& locale = QLocale::system());

private:
    AppStrings();
    static const AppStrings& instance();

    QHash<QString, QString> m_strings;
};
