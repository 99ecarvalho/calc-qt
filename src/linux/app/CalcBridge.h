// Bridges the upstream CalculatorManager to Qt.
// The engine reports display changes through ICalcDisplay callbacks; this
// class turns them into Qt signals and offers a small command API to the UI.
#pragma once

#include <QObject>
#include <QStringList>
#include <memory>

#include "CalculatorManager.h"
#include "EngineResources.h"

using CalculationManager::Command;

enum class CalcMode
{
    Standard,
    Scientific,
    Programmer
};

struct HistoryEntry
{
    QString expression;
    QString result;
};

class CalcBridge final : public QObject, public ICalcDisplay
{
    Q_OBJECT

public:
    explicit CalcBridge(QObject* parent = nullptr);
    ~CalcBridge() override;

    void setMode(CalcMode mode);
    CalcMode mode() const { return m_mode; }

    void send(Command command);
    void send(int command) { send(static_cast<Command>(command)); }

    // Programmer mode
    void setRadix(RadixType radix);
    RadixType radix() const { return m_radix; }
    QString resultForRadix(uint32_t radix, bool grouped);

    // Memory
    void memoryStore();
    void memoryRecall(unsigned int index);
    void memoryAdd(unsigned int index);
    void memorySubtract(unsigned int index);
    void memoryClear(unsigned int index);
    void memoryClearAll();
    const QStringList& memory() const { return m_memory; }

    // History (standard and scientific only)
    QList<HistoryEntry> history() const;
    void loadHistoryItem(int index);
    void removeHistoryItem(int index);
    void clearHistory();

    QString primary() const { return m_primary; }
    QString expression() const { return m_expression; }
    bool isInError() const { return m_isError; }
    QChar decimalSeparator() const { return m_resources.decimalSeparator(); }
    QChar groupSeparator() const { return m_resources.groupSeparator(); }
    Command angleMode() { return m_manager->GetCurrentDegreeMode(); }

    // ICalcDisplay
    void SetPrimaryDisplay(const std::wstring& text, bool isError) override;
    void SetIsInError(bool isInError) override;
    void SetExpressionDisplay(
        std::shared_ptr<std::vector<std::pair<std::wstring, int>>> const& tokens,
        std::shared_ptr<std::vector<std::shared_ptr<IExpressionCommand>>> const& commands) override;
    void SetParenthesisNumber(unsigned int count) override;
    void OnNoRightParenAdded() override {}
    void MaxDigitsReached() override;
    void BinaryOperatorReceived() override {}
    void OnHistoryItemAdded(unsigned int addedItemIndex) override;
    void SetMemorizedNumbers(const std::vector<std::wstring>& numbers) override;
    void MemoryItemChanged(unsigned int) override {}
    void InputChanged() override {}

signals:
    void primaryChanged(const QString& text, bool isError);
    void expressionChanged(const QString& text);
    void parenthesisCountChanged(unsigned int count);
    void memoryChanged();
    void historyChanged();
    void maxDigitsReached();

private:
    static QString joinTokens(const std::vector<std::pair<std::wstring, int>>& tokens);
    static std::vector<int> replayCommands(const std::vector<std::shared_ptr<IExpressionCommand>>& commands);

    EngineResources m_resources;
    std::unique_ptr<CalculationManager::CalculatorManager> m_manager;
    CalcMode m_mode = CalcMode::Standard;
    RadixType m_radix = RadixType::Decimal;
    bool m_feChecked = false;
    QString m_primary = QStringLiteral("0");
    QString m_expression;
    bool m_isError = false;
    QStringList m_memory;
};
