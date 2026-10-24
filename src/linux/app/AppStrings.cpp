#include "AppStrings.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

AppStrings::AppStrings()
{
    QFile file(QStringLiteral(":/strings/ui_strings.json"));
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QJsonObject all = QJsonDocument::fromJson(file.readAll()).object();

    auto merge = [this](const QJsonObject& strings) {
        for (auto it = strings.begin(); it != strings.end(); ++it)
            m_strings.insert(it.key(), it.value().toString());
    };
    merge(all.value(QLatin1String("en-US")).toObject());
    const QString chosen = pickLocale(all.keys());
    if (!chosen.isEmpty() && chosen != QLatin1String("en-US"))
        merge(all.value(chosen).toObject());
}

const AppStrings& AppStrings::instance()
{
    static const AppStrings strings;
    return strings;
}

QString AppStrings::get(const QString& key)
{
    return instance().m_strings.value(key, key);
}

QString AppStrings::pickLocale(const QStringList& available, const QLocale& locale)
{
    for (const QString& ui : locale.uiLanguages())
    {
        for (const QString& dir : available)
            if (dir.compare(ui, Qt::CaseInsensitive) == 0)
                return dir;
        const QString lang = ui.section('-', 0, 0);
        for (const QString& dir : available)
            if (dir.section('-', 0, 0).compare(lang, Qt::CaseInsensitive) == 0)
                return dir;
    }
    return {};
}
