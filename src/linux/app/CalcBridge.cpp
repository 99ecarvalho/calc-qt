#include "CalcBridge.h"

using namespace CalculationManager;

CalcBridge::CalcBridge(QObject* parent)
    : QObject(parent)
    , m_manager(std::make_unique<CalculatorManager>(this, &m_resources))
{
    setMode(CalcMode::Standard);
}

CalcBridge::~CalcBridge() = default;

void CalcBridge::setMode(CalcMode mode)
{
    // Same sequence as StandardCalculatorViewModel.SetCalculatorType upstream.
    m_mode = mode;
    m_isError = false;
    switch (mode)
    {
    case CalcMode::Standard:
        m_manager->SetStandardMode();
        m_radix = RadixType::Decimal;
        m_manager->SetRadix(RadixType::Decimal);
        m_manager->SetPrecision(static_cast<int>(CalculatorPrecision::StandardModePrecision));
        m_manager->UpdateMaxIntDigits();
        break;
    case CalcMode::Scientific:
        m_manager->SetScientificMode();
        m_radix = RadixType::Decimal;
        m_manager->SetRadix(RadixType::Decimal);
        m_manager->SetPrecision(static_cast<int>(CalculatorPrecision::ScientificModePrecision));
        break;
    case CalcMode::Programmer:
        m_manager->SetProgrammerMode();
        m_radix = RadixType::Decimal;
        m_manager->SetMemorizedNumbersString();
        m_manager->SetPrecision(static_cast<int>(CalculatorPrecision::ProgrammerModePrecision));
        break;
    }
    emit historyChanged();
}

void CalcBridge::send(Command command)
{
    if (command == Command::CommandFE)
        m_feChecked = !m_feChecked;
    m_manager->SendCommand(command);
}

void CalcBridge::setRadix(RadixType radix)
{
    if (m_isError)
        m_manager->SendCommand(Command::CommandCLEAR);
    m_radix = radix;
    m_manager->SetRadix(radix);
}

QString CalcBridge::resultForRadix(uint32_t radix, bool grouped)
{
    if (m_isError)
        return {};
    return QString::fromStdWString(m_manager->GetResultForRadix(radix, 64, grouped));
}

void CalcBridge::memoryStore() { m_manager->MemorizeNumber(); }
void CalcBridge::memoryRecall(unsigned int i) { m_manager->MemorizedNumberLoad(i); }
void CalcBridge::memoryAdd(unsigned int i) { m_manager->MemorizedNumberAdd(i); }
void CalcBridge::memorySubtract(unsigned int i) { m_manager->MemorizedNumberSubtract(i); }

void CalcBridge::memoryClear(unsigned int i)
{
    m_manager->MemorizedNumberClear(i);
    if (i < static_cast<unsigned int>(m_memory.size()))
        m_memory.removeAt(i);
    emit memoryChanged();
}

void CalcBridge::memoryClearAll()
{
    m_manager->MemorizedNumberClearAll();
    m_memory.clear();
    emit memoryChanged();
}

QList<HistoryEntry> CalcBridge::history() const
{
    QList<HistoryEntry> out;
    if (m_mode == CalcMode::Programmer)
        return out;
    for (const auto& item : m_manager->GetHistoryItems())
    {
        out.append({QString::fromStdWString(item->historyItemVector.expression),
                    QString::fromStdWString(item->historyItemVector.result)});
    }
    return out;
}

void CalcBridge::loadHistoryItem(int index)
{
    const auto& items = m_manager->GetHistoryItems();
    if (index < 0 || index >= static_cast<int>(items.size()))
        return;
    const auto item = items[index]; // keep it alive across the replay
    const auto& vec = item->historyItemVector;

    // Mirrors StandardCalculatorViewModel.Recalculate(fromHistory: true): replay
    // the commands with display updates muted, so the engine holds the value
    // and a following operator continues from it.
    Command degree = m_manager->GetCurrentDegreeMode();
    if (degree == Command::CommandNULL)
        degree = Command::CommandDEG;
    const bool scientific = m_mode == CalcMode::Scientific;
    m_manager->SetInHistoryItemLoadMode(true);
    m_manager->Reset(false);
    if (scientific)
        m_manager->SendCommand(Command::ModeScientific);
    if (m_feChecked)
        m_manager->SendCommand(Command::CommandFE);
    m_manager->SendCommand(degree);
    for (int c : replayCommands(*vec.spCommands))
        m_manager->SendCommand(static_cast<Command>(c));
    m_manager->SendCommand(Command::CommandFE);
    m_manager->SendCommand(Command::CommandFE);
    m_manager->SetInHistoryItemLoadMode(false);

    m_expression = joinTokens(*vec.spTokens);
    m_primary = QString::fromStdWString(vec.result);
    m_isError = false;
    emit expressionChanged(m_expression);
    emit primaryChanged(m_primary, false);
}

void CalcBridge::removeHistoryItem(int index)
{
    if (index >= 0)
        m_manager->RemoveHistoryItem(static_cast<unsigned int>(index));
    emit historyChanged();
}

void CalcBridge::clearHistory()
{
    m_manager->ClearHistory();
    emit historyChanged();
}

// ---------------------------------------------------------------------------
// ICalcDisplay
// ---------------------------------------------------------------------------

void CalcBridge::SetPrimaryDisplay(const std::wstring& text, bool isError)
{
    m_primary = QString::fromStdWString(text);
    m_isError = isError;
    emit primaryChanged(m_primary, isError);
}

void CalcBridge::SetIsInError(bool isInError)
{
    m_isError = isInError;
}

void CalcBridge::SetExpressionDisplay(
    std::shared_ptr<std::vector<std::pair<std::wstring, int>>> const& tokens,
    std::shared_ptr<std::vector<std::shared_ptr<IExpressionCommand>>> const&)
{
    m_expression = tokens ? joinTokens(*tokens) : QString();
    emit expressionChanged(m_expression);
}

void CalcBridge::SetParenthesisNumber(unsigned int count)
{
    emit parenthesisCountChanged(count);
}

void CalcBridge::MaxDigitsReached()
{
    emit maxDigitsReached();
}

void CalcBridge::OnHistoryItemAdded(unsigned int)
{
    emit historyChanged();
}

void CalcBridge::SetMemorizedNumbers(const std::vector<std::wstring>& numbers)
{
    m_memory.clear();
    for (const auto& n : numbers)
        m_memory.append(QString::fromStdWString(n));
    emit memoryChanged();
}

// ---------------------------------------------------------------------------

QString CalcBridge::joinTokens(const std::vector<std::pair<std::wstring, int>>& tokens)
{
    QStringList parts;
    for (const auto& t : tokens)
        parts.append(QString::fromStdWString(t.first));
    return parts.join(' ').simplified();
}

std::vector<int> CalcBridge::replayCommands(const std::vector<std::shared_ptr<IExpressionCommand>>& commands)
{
    // Port of StandardCalculatorViewModel.GetCommandsFromExpressionCommands.
    std::vector<int> out;
    for (const auto& cmd : commands)
    {
        switch (cmd->GetCommandType())
        {
        case CommandType::UnaryCommand:
        {
            auto unary = std::static_pointer_cast<IUnaryCommand>(cmd);
            for (int c : *unary->GetCommands())
                out.push_back(c);
            break;
        }
        case CommandType::BinaryCommand:
            out.push_back(std::static_pointer_cast<IBinaryCommand>(cmd)->GetCommand());
            break;
        case CommandType::Parentheses:
            out.push_back(std::static_pointer_cast<IParenthesisCommand>(cmd)->GetCommand());
            break;
        case CommandType::OperandCommand:
        {
            auto opnd = std::static_pointer_cast<IOpndCommand>(cmd);
            bool needSign = opnd->IsNegative();
            for (int c : *opnd->GetCommands())
            {
                out.push_back(c);
                if (needSign && c != static_cast<int>(Command::Command0))
                {
                    out.push_back(static_cast<int>(Command::CommandSIGN));
                    needSign = false;
                }
            }
            break;
        }
        }
    }
    return out;
}
