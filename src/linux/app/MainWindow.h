// Main window: a Qt port of the Windows Calculator Standard, Scientific and
// Programmer views. All arithmetic is done by the upstream CalcManager engine.
#pragma once

#include <QMainWindow>
#include <QPointer>
#include <functional>
#include <vector>

#include "CalcBridge.h"

class DatePage;
class FitLabel;
class QFrame;
class QGridLayout;
class QLabel;
class QListWidget;
class QPushButton;
class QStackedWidget;
class QTabWidget;
class QToolButton;
class QVBoxLayout;
class QWidget;

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void keyPressEvent(QKeyEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void changeEvent(QEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    // A keypad button. Negative commands are handled by the window itself.
    struct Key
    {
        QString label;
        int command;
        QString kind; // "digit", "func" or "equal"; used by the style sheet
        QString altLabel = {};
        int altCommand = 0;
    };

    enum Special
    {
        kSecond = -1,
        kShiftLeft = -2,
        kShiftRight = -3,
    };

    enum class ShiftMode
    {
        Arithmetic,
        Logical,
        Rotate,
        RotateCarry
    };

    void buildUi();
    QWidget* buildHeader();
    QWidget* buildSidePanel();
    void rebuildModeArea();
    void rebuildKeypad();
    QPushButton* makeKey(const Key& key);
    QPushButton* makeBarButton(const QString& text, const QString& tip);
    void addKeyRows(QGridLayout* grid, const std::vector<std::vector<Key>>& rows);

    // Menu index of the date view, after the three calculator modes.
    static constexpr int kDateMode = 3;
    static QString modeName(int mode);

    void setMode(CalcMode mode);
    void setDateMode();
    void pressKey(const Key& key);
    void sendCommand(int command);

    void showTrigPopup(QWidget* anchor);
    void showFunctionPopup(QWidget* anchor);
    void showBitwisePopup(QWidget* anchor);
    void showShiftPopup(QWidget* anchor);
    void showPanelPopup(int tab);
    QFrame* makePopup(QWidget* anchor, const std::function<void(QFrame*, QGridLayout*)>& fill);

    void fillHistoryList(QListWidget* list);
    void fillMemoryList(QListWidget* list);
    QWidget* makeHistoryPage(QListWidget*& list, bool closeOnPick);
    QWidget* makeMemoryPage(QListWidget*& list, bool closeOnPick);

    void onPrimaryChanged(const QString& text, bool isError);
    void onExpressionChanged(const QString& text);
    void refreshProgrammer();
    void refreshBits();
    void refreshEnabledKeys();
    void refreshMemoryBar();
    void refreshPanels();
    void updateSecondLabels();
    void updateDockedPanel();

    bool handleKey(QKeyEvent* event);
    void copyResult();
    void pasteValue();
    void setRadix(RadixType radix);
    void cycleWordSize();
    void cycleAngle();

    void applyTheme();
    void loadSettings();
    void saveSettings();

    CalcBridge* m_calc = nullptr;
    QString m_theme = QStringLiteral("system"); // "system", "light" or "dark"

    // Calculator and date views
    QStackedWidget* m_pages = nullptr;
    DatePage* m_datePage = nullptr;
    bool m_dateMode = false;

    // Header and display
    QLabel* m_modeTitle = nullptr;
    QToolButton* m_panelButton = nullptr;
    QToolButton* m_topButton = nullptr;
    QLabel* m_expression = nullptr;
    FitLabel* m_primary = nullptr;

    // Mode-dependent area between the display and the keypad
    QWidget* m_modeArea = nullptr;
    QVBoxLayout* m_modeAreaLayout = nullptr;
    QWidget* m_keypadHost = nullptr;
    QVBoxLayout* m_keypadLayout = nullptr;

    // Memory bar
    QPushButton* m_mcButton = nullptr;
    QPushButton* m_mrButton = nullptr;
    QPushButton* m_memListButton = nullptr;

    // Keypad state
    struct KeyButton
    {
        QPushButton* button;
        Key key;
    };
    std::vector<KeyButton> m_keys;
    bool m_second = false;
    bool m_trigInv = false;
    bool m_trigHyp = false;
    bool m_feOn = false;
    unsigned int m_parens = 0;

    // Scientific
    QPushButton* m_angleButton = nullptr;
    QPushButton* m_feButton = nullptr;

    // Programmer
    struct RadixRow
    {
        RadixType radix;
        QPushButton* button;
        QLabel* value;
    };
    std::vector<RadixRow> m_radixRows;
    QPushButton* m_wordButton = nullptr;
    QPushButton* m_bitsToggle = nullptr;
    std::vector<QPushButton*> m_bitButtons;
    bool m_bitPad = false;
    int m_wordBits = 64;
    ShiftMode m_shiftMode = ShiftMode::Arithmetic;

    // History and memory side panel (docked when the window is wide)
    QWidget* m_sidePanel = nullptr;
    QTabWidget* m_tabs = nullptr;
    QListWidget* m_historyList = nullptr;
    QListWidget* m_memoryList = nullptr;
    QPointer<QFrame> m_panelPopup;
    QListWidget* m_popupHistory = nullptr;
    QListWidget* m_popupMemory = nullptr;
};
