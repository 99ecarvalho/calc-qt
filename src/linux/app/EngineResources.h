// Engine string provider for the Linux build.
// Strings are read from the upstream CEngineStrings.resw files, embedded as Qt
// resources. Locale-specific files only carry a few entries, so every lookup
// falls back to en-US. Number separators come from the system locale.
#pragma once

#include <QHash>
#include <QLocale>
#include <QString>

#include "CalculatorResource.h"

class EngineResources final : public CalculationManager::IResourceProvider
{
public:
    explicit EngineResources(const QLocale& locale = QLocale::system());

    std::wstring GetCEngineString(std::wstring_view id) override;

    QChar decimalSeparator() const { return m_decimal; }
    QChar groupSeparator() const { return m_group; }

private:
    static QHash<QString, QString> loadResw(const QString& path);

    QHash<QString, QString> m_strings;
    QChar m_decimal;
    QChar m_group;
};
