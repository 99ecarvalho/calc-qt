// Headless checks of the upstream engine on Linux.
// Each case sends a key sequence to CalculatorManager and compares the
// primary display with the value the Windows app shows.
#include <cstdio>
#include <string>
#include <vector>

#include "CalculatorManager.h"
#include "CalculatorResource.h"

using namespace CalculationManager;

namespace
{
    class Resources final : public IResourceProvider
    {
    public:
        std::wstring GetCEngineString(std::wstring_view id) override
        {
            if (id == L"sDecimal") return L".";
            if (id == L"sThousand") return L",";
            if (id == L"sGrouping") return L"3;0";
            return L"";
        }
    };

    class Display final : public ICalcDisplay
    {
    public:
        std::wstring primary;
        bool error = false;
        void SetPrimaryDisplay(const std::wstring& text, bool isError) override { primary = text; error = isError; }
        void SetIsInError(bool) override {}
        void SetExpressionDisplay(std::shared_ptr<std::vector<std::pair<std::wstring, int>>> const&,
                                  std::shared_ptr<std::vector<std::shared_ptr<IExpressionCommand>>> const&) override {}
        void SetParenthesisNumber(unsigned int) override {}
        void OnNoRightParenAdded() override {}
        void MaxDigitsReached() override {}
        void BinaryOperatorReceived() override {}
        void OnHistoryItemAdded(unsigned int) override {}
        void SetMemorizedNumbers(const std::vector<std::wstring>&) override {}
        void MemoryItemChanged(unsigned int) override {}
        void InputChanged() override {}
    };

    // Turns "12+3=" into commands. Letters map to named commands below.
    std::vector<Command> parse(const std::string& keys)
    {
        std::vector<Command> out;
        for (char c : keys)
        {
            if (c >= '0' && c <= '9') out.push_back(static_cast<Command>(static_cast<int>(Command::Command0) + (c - '0')));
            else if (c == '+') out.push_back(Command::CommandADD);
            else if (c == '-') out.push_back(Command::CommandSUB);
            else if (c == '*') out.push_back(Command::CommandMUL);
            else if (c == '/') out.push_back(Command::CommandDIV);
            else if (c == '=') out.push_back(Command::CommandEQU);
            else if (c == '.') out.push_back(Command::CommandPNT);
            else if (c == '(') out.push_back(Command::CommandOPENP);
            else if (c == ')') out.push_back(Command::CommandCLOSEP);
            else if (c == '%') out.push_back(Command::CommandPERCENT);
            else if (c == '^') out.push_back(Command::CommandPWR);
            else if (c == 'q') out.push_back(Command::CommandSQRT);
            else if (c == 'r') out.push_back(Command::CommandREC);
            else if (c == '!') out.push_back(Command::CommandFAC);
            else if (c == 's') out.push_back(Command::CommandSIN);
            else if (c == 'n') out.push_back(Command::CommandSIGN);
            else if (c == 'p') out.push_back(Command::CommandPI);
        }
        return out;
    }

    struct Case
    {
        const char* mode;
        const char* keys;
        const wchar_t* expected;
        bool error = false;
    };

    const Case kCases[] = {
        {"std", "1+2=", L"3"},
        {"std", "2+3*4=", L"20"},                  // standard mode evaluates left to right
        {"sci", "2+3*4=", L"14"},                  // scientific respects precedence
        {"std", "1/3=", L"0.3333333333333333"},
        {"std", "1234567*1000=", L"1,234,567,000"},
        {"std", "2q", L"1.414213562373095"},
        {"std", "0.1+0.2=", L"0.3"},               // exact rational arithmetic
        {"std", "50+10%", L"5"},
        {"std", "1/0=", L"", true},
        {"sci", "(2+3)*4=", L"20"},
        {"sci", "2^10=", L"1,024"},
        {"sci", "20!", L"2,432,902,008,176,640,000"},
        {"sci", "30s", L"0.5"},                    // degrees by default
        {"sci", "5n*3=", L"-15"},
        {"prog", "255", L"255"},
    };
}

int main()
{
    Resources res;
    int failures = 0;
    for (const Case& c : kCases)
    {
        Display display;
        CalculatorManager manager(&display, &res);
        const std::string mode = c.mode;
        if (mode == "sci")
        {
            manager.SetScientificMode();
            manager.SetPrecision(static_cast<int>(CalculatorPrecision::ScientificModePrecision));
        }
        else if (mode == "prog")
        {
            manager.SetProgrammerMode();
        }
        else
        {
            manager.SetStandardMode();
            manager.SetPrecision(static_cast<int>(CalculatorPrecision::StandardModePrecision));
            manager.UpdateMaxIntDigits();
        }
        for (Command cmd : parse(c.keys))
            manager.SendCommand(cmd);

        bool ok = c.error ? display.error : (!display.error && display.primary == c.expected);
        if (mode == "prog" && ok)
        {
            ok = manager.GetResultForRadix(16, 64, false) == L"FF" && manager.GetResultForRadix(2, 64, false) == L"11111111";
        }
        std::printf("%s  %-5s %-16s -> %ls\n", ok ? "PASS" : "FAIL", c.mode, c.keys, display.primary.c_str());
        if (!ok)
        {
            std::printf("      expected %ls%s\n", c.expected, c.error ? " (error)" : "");
            ++failures;
        }
    }
    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
