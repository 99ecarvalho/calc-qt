#include "EngineResources.h"

#include "AppStrings.h"

#include <QDir>
#include <QFile>
#include <QXmlStreamReader>

EngineResources::EngineResources(const QLocale& locale)
{
    m_strings = loadResw(QStringLiteral(":/engine/en-US/CEngineStrings.resw"));

    // Messages follow the UI languages (LANGUAGE, then LC_MESSAGES/LANG);
    // separators follow the numeric locale.
    const QString chosen = AppStrings::pickLocale(QDir(QStringLiteral(":/engine")).entryList(QDir::Dirs), locale);
    if (!chosen.isEmpty() && chosen.compare(QLatin1String("en-US"), Qt::CaseInsensitive) != 0)
    {
        const auto local = loadResw(QStringLiteral(":/engine/%1/CEngineStrings.resw").arg(chosen));
        for (auto it = local.cbegin(); it != local.cend(); ++it)
            m_strings.insert(it.key(), it.value());
    }

    const QString dec = locale.decimalPoint();
    const QString grp = locale.groupSeparator();
    m_decimal = dec.isEmpty() ? QChar('.') : dec.at(0);
    m_group = grp.isEmpty() ? QChar(',') : grp.at(0);
}

std::wstring EngineResources::GetCEngineString(std::wstring_view id)
{
    if (id == L"sDecimal")
        return std::wstring(1, static_cast<wchar_t>(m_decimal.unicode()));
    if (id == L"sThousand")
        return std::wstring(1, static_cast<wchar_t>(m_group.unicode()));
    if (id == L"sGrouping")
        return L"3;0"; // Win32 grouping format: groups of three digits.

    const QString key = QString::fromWCharArray(id.data(), static_cast<qsizetype>(id.size()));
    return m_strings.value(key).toStdWString();
}

QHash<QString, QString> EngineResources::loadResw(const QString& path)
{
    QHash<QString, QString> out;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return out;

    QXmlStreamReader xml(&file);
    QString name;
    while (!xml.atEnd())
    {
        xml.readNext();
        if (!xml.isStartElement())
            continue;
        if (xml.name() == QLatin1String("data"))
            name = xml.attributes().value(QLatin1String("name")).toString();
        else if (xml.name() == QLatin1String("value") && !name.isEmpty())
        {
            out.insert(name, xml.readElementText());
            name.clear();
        }
    }
    return out;
}
