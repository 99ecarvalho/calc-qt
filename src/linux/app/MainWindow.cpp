#include "MainWindow.h"

#include <QActionGroup>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QScreen>
#include <QSettings>
#include <QStackedWidget>
#include <QStyleHints>
#include <QTabBar>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include "AppStrings.h"
#include "DatePage.h"
#include "FitLabel.h"

namespace
{
    constexpr int C(Command c) { return static_cast<int>(c); }
    constexpr int kDigit0 = C(Command::Command0);
    constexpr int kDigitF = C(Command::CommandF);
    constexpr int kDockWidth = 620;

    // Code points in the upstream CalculatorIcons.ttf ("Calculator Fluent Icons").
    constexpr char16_t kGlyphHistory = 0xE81C;
    constexpr char16_t kGlyphPin = 0xEE49;
    constexpr char16_t kGlyphDelete = 0xE74D;

    void clearLayout(QLayout* layout)
    {
        while (QLayoutItem* item = layout->takeAt(0))
        {
            if (QWidget* w = item->widget())
                w->deleteLater();
            if (QLayout* child = item->layout())
                clearLayout(child);
            delete item;
        }
    }

    int radixBase(RadixType r)
    {
        switch (r)
        {
        case RadixType::Hex: return 16;
        case RadixType::Octal: return 8;
        case RadixType::Binary: return 2;
        default: return 10;
        }
    }

    QString digitLabel(int d) { return QString::number(d, 16).toUpper(); }
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_calc(new CalcBridge(this))
{
    setWindowTitle(tr("Calculator"));
    setWindowIcon(QIcon(QStringLiteral(":/icons/CalculatorAppList.targetsize-256_altform-unplated.png")));
    setMinimumSize(320, 500);

    connect(m_calc, &CalcBridge::primaryChanged, this, &MainWindow::onPrimaryChanged);
    connect(m_calc, &CalcBridge::expressionChanged, this, &MainWindow::onExpressionChanged);
    connect(m_calc, &CalcBridge::historyChanged, this, &MainWindow::refreshPanels);
    connect(m_calc, &CalcBridge::memoryChanged, this, [this] {
        refreshPanels();
        refreshMemoryBar();
    });
    connect(m_calc, &CalcBridge::parenthesisCountChanged, this, [this](unsigned int n) {
        m_parens = n;
        for (auto& k : m_keys)
            if (k.key.command == C(Command::CommandOPENP))
                k.button->setText(n ? QStringLiteral("(%1").arg(n) : QStringLiteral("("));
    });
    connect(m_calc, &CalcBridge::maxDigitsReached, this, [] { QApplication::beep(); });

    buildUi();
    applyTheme();
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, &MainWindow::applyTheme);

    qApp->installEventFilter(this);
    loadSettings();
}

MainWindow::~MainWindow()
{
    qApp->removeEventFilter(this);
}

// ---------------------------------------------------------------------------
// UI construction
// ---------------------------------------------------------------------------

void MainWindow::buildUi()
{
    auto* central = new QWidget(this);
    central->setObjectName(QStringLiteral("central"));
    auto* body = new QHBoxLayout(central);
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);

    auto* column = new QWidget(central);
    auto* col = new QVBoxLayout(column);
    col->setContentsMargins(4, 0, 4, 4);
    col->setSpacing(2);

    col->addWidget(buildHeader());

    m_pages = new QStackedWidget(column);
    col->addWidget(m_pages, 1);
    auto* calcPage = new QWidget(m_pages);
    auto* calc = new QVBoxLayout(calcPage);
    calc->setContentsMargins(0, 0, 0, 0);
    calc->setSpacing(2);
    m_pages->addWidget(calcPage);
    m_datePage = new DatePage(m_pages);
    m_pages->addWidget(m_datePage);
    column = calcPage;
    col = calc;

    m_expression = new QLabel(column);
    m_expression->setObjectName(QStringLiteral("expression"));
    m_expression->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_expression->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    m_expression->setFixedHeight(24);
    m_expression->setContentsMargins(8, 0, 8, 0);
    col->addWidget(m_expression);

    m_primary = new FitLabel(column);
    m_primary->setObjectName(QStringLiteral("primary"));
    m_primary->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    m_primary->setFixedHeight(72);
    m_primary->setContentsMargins(8, 0, 8, 0);
    m_primary->setSizeRange(18, 48);
    m_primary->setFitText(QStringLiteral("0"));
    col->addWidget(m_primary);

    m_modeArea = new QWidget(column);
    m_modeAreaLayout = new QVBoxLayout(m_modeArea);
    m_modeAreaLayout->setContentsMargins(0, 0, 0, 0);
    m_modeAreaLayout->setSpacing(2);
    col->addWidget(m_modeArea);

    m_keypadHost = new QWidget(column);
    m_keypadLayout = new QVBoxLayout(m_keypadHost);
    m_keypadLayout->setContentsMargins(0, 0, 0, 0);
    col->addWidget(m_keypadHost, 1);

    body->addWidget(m_pages->parentWidget(), 1);
    body->addWidget(buildSidePanel());

    setCentralWidget(central);
    rebuildModeArea();
    rebuildKeypad();
}

QWidget* MainWindow::buildHeader()
{
    auto* header = new QWidget(this);
    auto* row = new QHBoxLayout(header);
    row->setContentsMargins(0, 4, 0, 0);

    auto* menuButton = new QToolButton(header);
    menuButton->setText(QStringLiteral("☰"));
    menuButton->setToolTip(tr("Open navigation"));
    menuButton->setPopupMode(QToolButton::InstantPopup);
    menuButton->setFocusPolicy(Qt::NoFocus);

    auto* menu = new QMenu(menuButton);
    auto* group = new QActionGroup(menu);
    for (int n = 0; n <= kDateMode; ++n)
    {
        QAction* a = menu->addAction(modeName(n), this, [this, n] {
            if (n == kDateMode)
                setDateMode();
            else
                setMode(static_cast<CalcMode>(n));
        });
        a->setCheckable(true);
        a->setShortcut(QKeySequence(Qt::ALT | (Qt::Key_1 + n)));
        a->setShortcutVisibleInContextMenu(true);
        a->setData(n);
        group->addAction(a);
        if (n == static_cast<int>(CalcMode::Programmer))
            menu->addSeparator();
    }
    menu->addSeparator();
    QMenu* themeMenu = menu->addMenu(tr("Theme"));
    auto* themeGroup = new QActionGroup(themeMenu);
    const std::pair<QString, QString> themes[] = {
        {tr("Use system setting"), QStringLiteral("system")},
        {tr("Light"), QStringLiteral("light")},
        {tr("Dark"), QStringLiteral("dark")},
    };
    for (const auto& [name, id] : themes)
    {
        QAction* a = themeMenu->addAction(name, this, [this, id = id] {
            m_theme = id;
            applyTheme();
        });
        a->setCheckable(true);
        a->setData(id);
        themeGroup->addAction(a);
    }
    connect(themeMenu, &QMenu::aboutToShow, themeMenu, [this, themeGroup] {
        for (QAction* a : themeGroup->actions())
            a->setChecked(a->data().toString() == m_theme);
    });
    menu->addAction(tr("About"), this, [this] {
        QMessageBox::about(this, tr("About Calculator"),
                           tr("<h3>Calculator</h3>"
                              "<p>A Linux port of the open source Windows Calculator.</p>"
                              "<p>The calculation engine (CalcManager) is the upstream Microsoft "
                              "code, unchanged. The interface is rebuilt with Qt %1.</p>"
                              "<p>Microsoft Calculator is licensed under the MIT License.</p>")
                               .arg(QString::fromLatin1(qVersion())));
    });
    connect(menu, &QMenu::aboutToShow, menu, [this, group] {
        for (QAction* a : group->actions())
            a->setChecked(a->data().toInt() == (m_dateMode ? kDateMode : static_cast<int>(m_calc->mode())));
    });
    menuButton->setMenu(menu);
    addActions(group->actions()); // keep Alt+1..3 working while the menu is closed

    m_modeTitle = new QLabel(header);
    m_modeTitle->setObjectName(QStringLiteral("modeTitle"));

    m_topButton = new QToolButton(header);
    m_topButton->setText(QString(QChar(kGlyphPin)));
    m_topButton->setProperty("kind", "icon");
    m_topButton->setToolTip(tr("Keep on top"));
    m_topButton->setCheckable(true);
    m_topButton->setFocusPolicy(Qt::NoFocus);
    connect(m_topButton, &QToolButton::toggled, this, [this](bool on) {
        setWindowFlag(Qt::WindowStaysOnTopHint, on);
        show();
    });

    m_panelButton = new QToolButton(header);
    m_panelButton->setText(QString(QChar(kGlyphHistory)));
    m_panelButton->setProperty("kind", "icon");
    m_panelButton->setToolTip(tr("History (Ctrl+H)"));
    m_panelButton->setFocusPolicy(Qt::NoFocus);
    connect(m_panelButton, &QToolButton::clicked, this, [this] {
        showPanelPopup(m_calc->mode() == CalcMode::Programmer ? 1 : 0);
    });

    row->addWidget(menuButton);
    row->addWidget(m_modeTitle);
    row->addWidget(m_topButton);
    row->addStretch(1);
    row->addWidget(m_panelButton);
    return header;
}

QWidget* MainWindow::buildSidePanel()
{
    m_sidePanel = new QWidget(this);
    m_sidePanel->setObjectName(QStringLiteral("sidePanel"));
    m_sidePanel->setFixedWidth(300);
    auto* lay = new QVBoxLayout(m_sidePanel);
    lay->setContentsMargins(8, 8, 8, 8);

    m_tabs = new QTabWidget(m_sidePanel);
    m_tabs->tabBar()->setFocusPolicy(Qt::NoFocus);
    m_tabs->setDocumentMode(true);
    m_tabs->tabBar()->setDrawBase(false);
    m_tabs->addTab(makeHistoryPage(m_historyList, false), tr("History"));
    m_tabs->addTab(makeMemoryPage(m_memoryList, false), tr("Memory"));
    lay->addWidget(m_tabs);
    return m_sidePanel;
}

QWidget* MainWindow::makeHistoryPage(QListWidget*& list, bool closeOnPick)
{
    auto* page = new QWidget;
    auto* lay = new QVBoxLayout(page);
    lay->setContentsMargins(0, 4, 0, 0);

    list = new QListWidget(page);
    list->setFocusPolicy(Qt::NoFocus);
    list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    list->setContextMenuPolicy(Qt::CustomContextMenu);
    lay->addWidget(list, 1);

    QListWidget* l = list;
    connect(list, &QListWidget::itemClicked, this, [this, closeOnPick](QListWidgetItem* item) {
        const QVariant idx = item->data(Qt::UserRole);
        if (!idx.isValid())
            return;
        m_calc->loadHistoryItem(idx.toInt());
        if (closeOnPick && m_panelPopup)
            m_panelPopup->close();
    });
    connect(list, &QListWidget::customContextMenuRequested, this, [this, l](const QPoint& pos) {
        QListWidgetItem* item = l->itemAt(pos);
        if (!item || !item->data(Qt::UserRole).isValid())
            return;
        const int idx = item->data(Qt::UserRole).toInt();
        QMenu menu;
        menu.addAction(tr("Copy"), this, [this, idx] {
            const auto h = m_calc->history();
            if (idx < h.size())
                QGuiApplication::clipboard()->setText(h[idx].result);
        });
        menu.addAction(tr("Delete"), this, [this, idx] { m_calc->removeHistoryItem(idx); });
        menu.exec(l->viewport()->mapToGlobal(pos));
    });

    auto* bottom = new QHBoxLayout;
    bottom->addStretch(1);
    auto* trash = new QToolButton(page);
    trash->setText(QString(QChar(kGlyphDelete)));
    trash->setProperty("kind", "icon");
    trash->setToolTip(tr("Clear all history (Ctrl+Shift+D)"));
    trash->setFocusPolicy(Qt::NoFocus);
    connect(trash, &QToolButton::clicked, this, [this] { m_calc->clearHistory(); });
    bottom->addWidget(trash);
    lay->addLayout(bottom);

    fillHistoryList(list);
    return page;
}

QWidget* MainWindow::makeMemoryPage(QListWidget*& list, bool closeOnPick)
{
    auto* page = new QWidget;
    auto* lay = new QVBoxLayout(page);
    lay->setContentsMargins(0, 4, 0, 0);

    list = new QListWidget(page);
    list->setFocusPolicy(Qt::NoFocus);
    list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    lay->addWidget(list, 1);

    connect(list, &QListWidget::itemClicked, this, [this, closeOnPick](QListWidgetItem* item) {
        const QVariant idx = item->data(Qt::UserRole);
        if (!idx.isValid())
            return;
        m_calc->memoryRecall(idx.toUInt());
        if (closeOnPick && m_panelPopup)
            m_panelPopup->close();
    });

    auto* bottom = new QHBoxLayout;
    bottom->addStretch(1);
    auto* trash = new QToolButton(page);
    trash->setText(QString(QChar(kGlyphDelete)));
    trash->setProperty("kind", "icon");
    trash->setToolTip(tr("Clear all memory (Ctrl+L)"));
    trash->setFocusPolicy(Qt::NoFocus);
    connect(trash, &QToolButton::clicked, this, [this] { m_calc->memoryClearAll(); });
    bottom->addWidget(trash);
    lay->addLayout(bottom);

    fillMemoryList(list);
    return page;
}

QPushButton* MainWindow::makeBarButton(const QString& text, const QString& tip)
{
    auto* b = new QPushButton(text);
    b->setProperty("kind", "bar");
    b->setToolTip(tip);
    b->setFocusPolicy(Qt::NoFocus);
    return b;
}

void MainWindow::rebuildModeArea()
{
    clearLayout(m_modeAreaLayout);
    m_mcButton = m_mrButton = m_memListButton = nullptr;
    m_angleButton = m_feButton = m_wordButton = m_bitsToggle = nullptr;
    m_radixRows.clear();

    const CalcMode mode = m_calc->mode();

    auto barRow = [this] {
        auto* w = new QWidget(m_modeArea);
        auto* h = new QHBoxLayout(w);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(2);
        m_modeAreaLayout->addWidget(w);
        return h;
    };

    if (mode == CalcMode::Scientific)
    {
        auto* h = barRow();
        m_angleButton = makeBarButton(QString(), tr("Angle unit (F3 / F4 / F5)"));
        connect(m_angleButton, &QPushButton::clicked, this, &MainWindow::cycleAngle);
        m_feButton = makeBarButton(QStringLiteral("F-E"), tr("Scientific notation (V)"));
        m_feButton->setCheckable(true);
        m_feButton->setChecked(m_feOn);
        connect(m_feButton, &QPushButton::clicked, this, [this] { sendCommand(C(Command::CommandFE)); });
        h->addWidget(m_angleButton);
        h->addWidget(m_feButton);
        h->addStretch(1);
        const Command a = m_calc->angleMode();
        m_angleButton->setText(a == Command::CommandRAD ? QStringLiteral("RAD") : a == Command::CommandGRAD ? QStringLiteral("GRAD") : QStringLiteral("DEG"));
    }

    if (mode == CalcMode::Programmer)
    {
        const std::pair<RadixType, QString> rows[] = {
            {RadixType::Hex, QStringLiteral("HEX")},
            {RadixType::Decimal, QStringLiteral("DEC")},
            {RadixType::Octal, QStringLiteral("OCT")},
            {RadixType::Binary, QStringLiteral("BIN")},
        };
        for (const auto& [radix, name] : rows)
        {
            auto* w = new QWidget(m_modeArea);
            auto* h = new QHBoxLayout(w);
            h->setContentsMargins(0, 0, 0, 0);
            auto* b = new QPushButton(name, w);
            b->setProperty("kind", "radix");
            b->setCheckable(true);
            b->setFocusPolicy(Qt::NoFocus);
            b->setFixedWidth(56);
            auto* v = new QLabel(w);
            v->setObjectName(QStringLiteral("radixValue"));
            v->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
            v->setWordWrap(true);
            h->addWidget(b);
            h->addWidget(v, 1);
            connect(b, &QPushButton::clicked, this, [this, radix = radix] { setRadix(radix); });
            m_radixRows.push_back({radix, b, v});
            m_modeAreaLayout->addWidget(w);
        }

        auto* h = barRow();
        m_bitsToggle = makeBarButton(m_bitPad ? QStringLiteral("⌨") : QStringLiteral("⁘"),
                                     tr("Switch between the full keypad and the bit toggling keypad"));
        m_bitsToggle->setCheckable(true);
        m_bitsToggle->setChecked(m_bitPad);
        connect(m_bitsToggle, &QPushButton::clicked, this, [this](bool on) {
            m_bitPad = on;
            m_bitsToggle->setText(on ? QStringLiteral("⌨") : QStringLiteral("⁘"));
            rebuildKeypad();
        });
        static const char* words[] = {"QWORD", "DWORD", "WORD", "BYTE"};
        const int wi = m_wordBits == 64 ? 0 : m_wordBits == 32 ? 1 : m_wordBits == 16 ? 2 : 3;
        m_wordButton = makeBarButton(QString::fromLatin1(words[wi]), tr("Word size"));
        connect(m_wordButton, &QPushButton::clicked, this, &MainWindow::cycleWordSize);
        h->addWidget(m_bitsToggle);
        h->addWidget(m_wordButton);
        h->addStretch(1);
        auto* ms = makeBarButton(QStringLiteral("MS"), tr("Memory store (Ctrl+M)"));
        connect(ms, &QPushButton::clicked, m_calc, &CalcBridge::memoryStore);
        m_memListButton = makeBarButton(QStringLiteral("M˅"), tr("Memory"));
        connect(m_memListButton, &QPushButton::clicked, this, [this] { showPanelPopup(1); });
        h->addWidget(ms);
        h->addWidget(m_memListButton);
    }
    else
    {
        auto* h = barRow();
        struct MemKey
        {
            const char* label;
            QString tip;
            std::function<void()> fn;
        };
        const MemKey memKeys[] = {
            {"MC", tr("Clear all memory (Ctrl+L)"), [this] { m_calc->memoryClearAll(); }},
            {"MR", tr("Memory recall (Ctrl+R)"), [this] { m_calc->memoryRecall(0); }},
            {"M+", tr("Memory add (Ctrl+P)"), [this] { m_calc->memoryAdd(0); }},
            {"M−", tr("Memory subtract (Ctrl+Q)"), [this] { m_calc->memorySubtract(0); }},
            {"MS", tr("Memory store (Ctrl+M)"), [this] { m_calc->memoryStore(); }},
        };
        for (const auto& mk : memKeys)
        {
            auto* b = makeBarButton(QString::fromUtf8(mk.label), mk.tip);
            connect(b, &QPushButton::clicked, this, mk.fn);
            h->addWidget(b);
            if (QLatin1String(mk.label) == QLatin1String("MC"))
                m_mcButton = b;
            if (QLatin1String(mk.label) == QLatin1String("MR"))
                m_mrButton = b;
        }
        m_memListButton = makeBarButton(QStringLiteral("M˅"), tr("Memory"));
        connect(m_memListButton, &QPushButton::clicked, this, [this] { showPanelPopup(1); });
        h->addWidget(m_memListButton);
    }

    if (mode == CalcMode::Scientific)
    {
        auto* h = barRow();
        auto* trig = makeBarButton(tr("△ Trigonometry ˅"), tr("Trigonometric functions"));
        connect(trig, &QPushButton::clicked, this, [this, trig] { showTrigPopup(trig); });
        auto* func = makeBarButton(tr("ƒ Function ˅"), tr("More functions"));
        connect(func, &QPushButton::clicked, this, [this, func] { showFunctionPopup(func); });
        h->addWidget(trig);
        h->addWidget(func);
        h->addStretch(1);
    }
    if (mode == CalcMode::Programmer)
    {
        auto* h = barRow();
        auto* bitwise = makeBarButton(tr("Bitwise ˅"), tr("Bitwise operators"));
        connect(bitwise, &QPushButton::clicked, this, [this, bitwise] { showBitwisePopup(bitwise); });
        auto* shift = makeBarButton(tr("Bit shift ˅"), tr("Bit shift mode"));
        connect(shift, &QPushButton::clicked, this, [this, shift] { showShiftPopup(shift); });
        h->addWidget(bitwise);
        h->addWidget(shift);
        h->addStretch(1);
    }

    refreshMemoryBar();
    refreshProgrammer();
}

QPushButton* MainWindow::makeKey(const Key& key)
{
    auto* b = new QPushButton(key.label);
    b->setProperty("kind", key.kind);
    b->setFocusPolicy(Qt::NoFocus);
    b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    if (key.command == kSecond)
        b->setCheckable(true);
    connect(b, &QPushButton::clicked, this, [this, b] {
        for (const auto& k : m_keys)
            if (k.button == b)
            {
                pressKey(k.key);
                return;
            }
    });
    return b;
}

void MainWindow::addKeyRows(QGridLayout* grid, const std::vector<std::vector<Key>>& rows)
{
    for (int r = 0; r < static_cast<int>(rows.size()); ++r)
        for (int c = 0; c < static_cast<int>(rows[r].size()); ++c)
        {
            QPushButton* b = makeKey(rows[r][c]);
            grid->addWidget(b, r, c);
            m_keys.push_back({b, rows[r][c]});
        }
}

void MainWindow::rebuildKeypad()
{
    clearLayout(m_keypadLayout);
    m_keys.clear();
    m_bitButtons.clear();
    m_second = false;

    auto* host = new QWidget(m_keypadHost);
    auto* grid = new QGridLayout(host);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(2);
    m_keypadLayout->addWidget(host);

    const QString dec(m_calc->decimalSeparator());
    auto digit = [](int d) { return Key{digitLabel(d), kDigit0 + d, QStringLiteral("digit")}; };
    auto func = [](const QString& label, Command c, const QString& alt = {}, Command altC = Command::CommandNULL) {
        return Key{label, C(c), QStringLiteral("func"), alt, C(altC)};
    };
    const Key sign{QStringLiteral("⁺⁄₋"), C(Command::CommandSIGN), QStringLiteral("digit")};
    const Key point{dec, C(Command::CommandPNT), QStringLiteral("digit")};
    const Key equal{QStringLiteral("="), C(Command::CommandEQU), QStringLiteral("equal")};
    const Key div = func(QStringLiteral("÷"), Command::CommandDIV);
    const Key mul = func(QStringLiteral("×"), Command::CommandMUL);
    const Key sub = func(QStringLiteral("−"), Command::CommandSUB);
    const Key add = func(QStringLiteral("+"), Command::CommandADD);
    const Key back = func(QStringLiteral("⌫"), Command::CommandBACK);
    const Key clear = func(QStringLiteral("C"), Command::CommandCLEAR);

    switch (m_calc->mode())
    {
    case CalcMode::Standard:
        addKeyRows(grid, {
            {func("%", Command::CommandPERCENT), func("CE", Command::CommandCENTR), clear, back},
            {func("¹⁄ₓ", Command::CommandREC), func("x²", Command::CommandSQR), func("²√x", Command::CommandSQRT), div},
            {digit(7), digit(8), digit(9), mul},
            {digit(4), digit(5), digit(6), sub},
            {digit(1), digit(2), digit(3), add},
            {sign, digit(0), point, equal},
        });
        break;

    case CalcMode::Scientific:
        addKeyRows(grid, {
            {Key{QStringLiteral("2ⁿᵈ"), kSecond, QStringLiteral("func")}, func("π", Command::CommandPI), func("e", Command::CommandEuler), clear, back},
            {func("x²", Command::CommandSQR, "x³", Command::CommandCUB), func("¹⁄ₓ", Command::CommandREC), func("|x|", Command::CommandAbs),
             func("exp", Command::CommandEXP), func("mod", Command::CommandMOD)},
            {func("²√x", Command::CommandSQRT, "³√x", Command::CommandCUBEROOT), func("(", Command::CommandOPENP), func(")", Command::CommandCLOSEP),
             func("n!", Command::CommandFAC), div},
            {func("xʸ", Command::CommandPWR, "ʸ√x", Command::CommandROOT), digit(7), digit(8), digit(9), mul},
            {func("10ˣ", Command::CommandPOW10, "2ˣ", Command::CommandPOW2), digit(4), digit(5), digit(6), sub},
            {func("log", Command::CommandLOG, "logᵧx", Command::CommandLogBaseY), digit(1), digit(2), digit(3), add},
            {func("ln", Command::CommandLN, "eˣ", Command::CommandPOWE), sign, digit(0), point, equal},
        });
        break;

    case CalcMode::Programmer:
        if (m_bitPad)
        {
            // 64 bits, four rows of sixteen, grouped by nibble like upstream.
            for (int row = 0; row < 4; ++row)
            {
                for (int i = 0; i < 16; ++i)
                {
                    const int bit = 63 - row * 16 - i;
                    const int col = i + i / 4;
                    auto* b = new QPushButton(QStringLiteral("0"), host);
                    b->setProperty("kind", "bit");
                    b->setFocusPolicy(Qt::NoFocus);
                    b->setToolTip(tr("Bit %1").arg(bit));
                    connect(b, &QPushButton::clicked, this, [this, bit] {
                        sendCommand(C(Command::CommandBINEDITSTART) + bit);
                    });
                    grid->addWidget(b, row * 2, col);
                    if (bit % 4 == 0)
                    {
                        auto* idx = new QLabel(QString::number(bit), host);
                        idx->setObjectName(QStringLiteral("bitIndex"));
                        idx->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
                        grid->addWidget(idx, row * 2 + 1, col);
                    }
                    m_bitButtons.push_back(b); // index i in vector == 63 - bit
                }
            }
            for (int g = 1; g < 4; ++g)
                grid->setColumnMinimumWidth(g * 5 - 1, 8);
            grid->setRowStretch(8, 1);
            refreshBits();
            break;
        }
        {
            const bool rotate = m_shiftMode == ShiftMode::Rotate || m_shiftMode == ShiftMode::RotateCarry;
            const QString carry = m_shiftMode == ShiftMode::RotateCarry ? QStringLiteral("C") : QString();
            const Key shl{rotate ? QStringLiteral("RoL") + carry : QStringLiteral("≪"), kShiftLeft, QStringLiteral("func")};
            const Key shr{rotate ? QStringLiteral("RoR") + carry : QStringLiteral("≫"), kShiftRight, QStringLiteral("func")};
            auto hex = [](int d) { return Key{digitLabel(d), kDigit0 + d, QStringLiteral("func")}; };
            addKeyRows(grid, {
                {hex(10), shl, shr, clear, back},
                {hex(11), func("(", Command::CommandOPENP), func(")", Command::CommandCLOSEP), func("%", Command::CommandMOD), div},
                {hex(12), digit(7), digit(8), digit(9), mul},
                {hex(13), digit(4), digit(5), digit(6), sub},
                {hex(14), digit(1), digit(2), digit(3), add},
                {hex(15), sign, digit(0), point, equal},
            });
        }
        break;
    }

    refreshEnabledKeys();
}

// ---------------------------------------------------------------------------
// Popups
// ---------------------------------------------------------------------------

QFrame* MainWindow::makePopup(QWidget* anchor, const std::function<void(QFrame*, QGridLayout*)>& fill)
{
    auto* popup = new QFrame(this, Qt::Popup);
    popup->setObjectName(QStringLiteral("popup"));
    popup->setAttribute(Qt::WA_DeleteOnClose);
    auto* grid = new QGridLayout(popup);
    grid->setContentsMargins(6, 6, 6, 6);
    grid->setSpacing(2);
    fill(popup, grid);
    popup->adjustSize();

    QPoint pos = anchor->mapToGlobal(QPoint(0, anchor->height()));
    if (QScreen* s = anchor->screen())
    {
        const QRect avail = s->availableGeometry();
        pos.setX(qBound(avail.left(), pos.x(), avail.right() - popup->width()));
        if (pos.y() + popup->height() > avail.bottom())
            pos.setY(anchor->mapToGlobal(QPoint(0, 0)).y() - popup->height());
    }
    popup->move(pos);
    popup->show();
    return popup;
}

void MainWindow::showTrigPopup(QWidget* anchor)
{
    makePopup(anchor, [this](QFrame* popup, QGridLayout* grid) {
        struct Trig
        {
            const char* name;
            Command plain, inv, hyp, invHyp;
        };
        static const Trig trig[] = {
            {"sin", Command::CommandSIN, Command::CommandASIN, Command::CommandSINH, Command::CommandASINH},
            {"cos", Command::CommandCOS, Command::CommandACOS, Command::CommandCOSH, Command::CommandACOSH},
            {"tan", Command::CommandTAN, Command::CommandATAN, Command::CommandTANH, Command::CommandATANH},
            {"sec", Command::CommandSEC, Command::CommandASEC, Command::CommandSECH, Command::CommandASECH},
            {"csc", Command::CommandCSC, Command::CommandACSC, Command::CommandCSCH, Command::CommandACSCH},
            {"cot", Command::CommandCOT, Command::CommandACOT, Command::CommandCOTH, Command::CommandACOTH},
        };

        auto* second = new QPushButton(QStringLiteral("2ⁿᵈ"), popup);
        auto* hyp = new QPushButton(QStringLiteral("hyp"), popup);
        for (QPushButton* t : {second, hyp})
        {
            t->setProperty("kind", "func");
            t->setCheckable(true);
            t->setFocusPolicy(Qt::NoFocus);
            t->setMinimumSize(72, 40);
        }
        second->setChecked(m_trigInv);
        hyp->setChecked(m_trigHyp);
        grid->addWidget(second, 0, 0);
        grid->addWidget(hyp, 0, 1);

        auto buttons = std::make_shared<std::vector<QPushButton*>>();
        auto relabel = [this, buttons] {
            for (int i = 0; i < 6; ++i)
            {
                QString s = QString::fromLatin1(trig[i].name);
                if (m_trigHyp)
                    s += 'h';
                if (m_trigInv)
                    s += QStringLiteral("⁻¹");
                (*buttons)[i]->setText(s);
            }
        };
        for (int i = 0; i < 6; ++i)
        {
            auto* b = new QPushButton(popup);
            b->setProperty("kind", "func");
            b->setFocusPolicy(Qt::NoFocus);
            b->setMinimumSize(72, 40);
            connect(b, &QPushButton::clicked, popup, [this, popup, i] {
                const Trig& t = trig[i];
                const Command c = m_trigHyp ? (m_trigInv ? t.invHyp : t.hyp) : (m_trigInv ? t.inv : t.plain);
                popup->close();
                sendCommand(C(c));
            });
            grid->addWidget(b, 1 + i / 3, i % 3);
            buttons->push_back(b);
        }
        connect(second, &QPushButton::toggled, popup, [this, relabel](bool on) { m_trigInv = on; relabel(); });
        connect(hyp, &QPushButton::toggled, popup, [this, relabel](bool on) { m_trigHyp = on; relabel(); });
        relabel();
    });
}

void MainWindow::showFunctionPopup(QWidget* anchor)
{
    makePopup(anchor, [this](QFrame* popup, QGridLayout* grid) {
        const std::pair<const char*, Command> fns[] = {
            {"|x|", Command::CommandAbs},   {"⌊x⌋", Command::CommandFloor},    {"⌈x⌉", Command::CommandCeil},
            {"rand", Command::CommandRand}, {"→dms", Command::CommandDMS}, {"→deg", Command::CommandDegrees},
        };
        int i = 0;
        for (const auto& [label, cmd] : fns)
        {
            auto* b = new QPushButton(QString::fromUtf8(label), popup);
            b->setProperty("kind", "func");
            b->setFocusPolicy(Qt::NoFocus);
            b->setMinimumSize(72, 40);
            connect(b, &QPushButton::clicked, popup, [this, popup, cmd = cmd] {
                popup->close();
                sendCommand(C(cmd));
            });
            grid->addWidget(b, i / 3, i % 3);
            ++i;
        }
    });
}

void MainWindow::showBitwisePopup(QWidget* anchor)
{
    makePopup(anchor, [this](QFrame* popup, QGridLayout* grid) {
        const std::pair<const char*, Command> ops[] = {
            {"AND", Command::CommandAnd},  {"OR", Command::CommandOR},   {"NOT", Command::CommandNot},
            {"NAND", Command::CommandNand}, {"NOR", Command::CommandNor}, {"XOR", Command::CommandXor},
        };
        int i = 0;
        for (const auto& [label, cmd] : ops)
        {
            auto* b = new QPushButton(QString::fromLatin1(label), popup);
            b->setProperty("kind", "func");
            b->setFocusPolicy(Qt::NoFocus);
            b->setMinimumSize(72, 40);
            connect(b, &QPushButton::clicked, popup, [this, popup, cmd = cmd] {
                popup->close();
                sendCommand(C(cmd));
            });
            grid->addWidget(b, i / 3, i % 3);
            ++i;
        }
    });
}

void MainWindow::showShiftPopup(QWidget* anchor)
{
    makePopup(anchor, [this](QFrame* popup, QGridLayout* grid) {
        const std::pair<QString, ShiftMode> modes[] = {
            {tr("Arithmetic shift"), ShiftMode::Arithmetic},
            {tr("Logical shift"), ShiftMode::Logical},
            {tr("Rotate circular shift"), ShiftMode::Rotate},
            {tr("Rotate through carry circular shift"), ShiftMode::RotateCarry},
        };
        int row = 0;
        for (const auto& [label, mode] : modes)
        {
            auto* b = new QPushButton(label, popup);
            b->setProperty("kind", "menu");
            b->setCheckable(true);
            b->setChecked(m_shiftMode == mode);
            b->setFocusPolicy(Qt::NoFocus);
            connect(b, &QPushButton::clicked, popup, [this, popup, mode = mode] {
                m_shiftMode = mode;
                popup->close();
                rebuildKeypad();
            });
            grid->addWidget(b, row++, 0);
        }
    });
}

void MainWindow::showPanelPopup(int tab)
{
    if (m_sidePanel->isVisible())
    {
        m_tabs->setCurrentIndex(tab);
        return;
    }

    auto* popup = new QFrame(this, Qt::Popup);
    popup->setObjectName(QStringLiteral("popup"));
    popup->setAttribute(Qt::WA_DeleteOnClose);
    auto* lay = new QVBoxLayout(popup);
    lay->setContentsMargins(8, 8, 8, 8);
    auto* tabs = new QTabWidget(popup);
    tabs->setDocumentMode(true);
    tabs->tabBar()->setDrawBase(false);
    tabs->tabBar()->setFocusPolicy(Qt::NoFocus);
    tabs->addTab(makeHistoryPage(m_popupHistory, true), tr("History"));
    tabs->addTab(makeMemoryPage(m_popupMemory, true), tr("Memory"));
    tabs->setTabVisible(0, m_calc->mode() != CalcMode::Programmer);
    tabs->setCurrentIndex(tab);
    lay->addWidget(tabs);
    connect(popup, &QObject::destroyed, this, [this] {
        m_popupHistory = nullptr;
        m_popupMemory = nullptr;
    });

    // Cover the keypad area, like the flyout in the Windows app.
    const QRect area(m_modeArea->mapToGlobal(QPoint(0, 0)),
                     m_keypadHost->mapToGlobal(QPoint(m_keypadHost->width(), m_keypadHost->height())));
    popup->setGeometry(area);
    m_panelPopup = popup;
    popup->show();
}

// ---------------------------------------------------------------------------
// History and memory lists
// ---------------------------------------------------------------------------

void MainWindow::fillHistoryList(QListWidget* list)
{
    if (!list)
        return;
    list->clear();
    const auto items = m_calc->history();
    if (items.isEmpty())
    {
        auto* empty = new QListWidgetItem(m_calc->mode() == CalcMode::Programmer ? QString() : tr("There's no history yet"), list);
        empty->setFlags(Qt::NoItemFlags);
        return;
    }
    // Newest first.
    for (int i = static_cast<int>(items.size()) - 1; i >= 0; --i)
    {
        auto* item = new QListWidgetItem(list);
        item->setData(Qt::UserRole, i);
        auto* label = new QLabel(QStringLiteral("<div align=right><span style='font-size:13px; opacity:0.7'>%1</span><br>"
                                                "<span style='font-size:22px; font-weight:600'>%2</span></div>")
                                     .arg(items[i].expression.toHtmlEscaped(), items[i].result.toHtmlEscaped()));
        label->setObjectName(QStringLiteral("listEntry"));
        label->setAlignment(Qt::AlignRight);
        label->setWordWrap(true);
        label->setContentsMargins(8, 6, 8, 6);
        item->setSizeHint(QSize(0, label->sizeHint().height() + 4));
        list->setItemWidget(item, label);
    }
}

void MainWindow::fillMemoryList(QListWidget* list)
{
    if (!list)
        return;
    list->clear();
    const QStringList& mem = m_calc->memory();
    if (mem.isEmpty())
    {
        auto* empty = new QListWidgetItem(tr("There's nothing saved in memory"), list);
        empty->setFlags(Qt::NoItemFlags);
        return;
    }
    for (int i = 0; i < mem.size(); ++i)
    {
        auto* item = new QListWidgetItem(list);
        item->setData(Qt::UserRole, i);
        auto* w = new QWidget;
        auto* v = new QVBoxLayout(w);
        v->setContentsMargins(8, 6, 8, 6);
        v->setSpacing(2);
        auto* value = new QLabel(mem[i], w);
        value->setObjectName(QStringLiteral("memoryValue"));
        value->setAlignment(Qt::AlignRight);
        v->addWidget(value);
        auto* h = new QHBoxLayout;
        h->addStretch(1);
        const unsigned int idx = static_cast<unsigned int>(i);
        const std::pair<const char*, std::function<void()>> actions[] = {
            {"MC", [this, idx] { m_calc->memoryClear(idx); }},
            {"M+", [this, idx] { m_calc->memoryAdd(idx); }},
            {"M−", [this, idx] { m_calc->memorySubtract(idx); }},
        };
        for (const auto& [label, fn] : actions)
        {
            auto* b = new QPushButton(QString::fromUtf8(label), w);
            b->setProperty("kind", "small");
            b->setFocusPolicy(Qt::NoFocus);
            connect(b, &QPushButton::clicked, this, fn);
            h->addWidget(b);
        }
        v->addLayout(h);
        item->setSizeHint(QSize(0, w->sizeHint().height() + 4));
        list->setItemWidget(item, w);
    }
}

void MainWindow::refreshPanels()
{
    fillHistoryList(m_historyList);
    fillMemoryList(m_memoryList);
    fillHistoryList(m_popupHistory);
    fillMemoryList(m_popupMemory);
}

// ---------------------------------------------------------------------------
// Actions
// ---------------------------------------------------------------------------

void MainWindow::setMode(CalcMode mode)
{
    if (m_panelPopup)
        m_panelPopup->close();
    m_calc->setMode(mode);
    m_feOn = false;
    m_parens = 0;
    if (mode == CalcMode::Programmer)
        m_wordBits = 64;

    m_dateMode = false;
    m_pages->setCurrentIndex(0);
    m_modeTitle->setText(modeName(static_cast<int>(mode)));
    rebuildModeArea();
    rebuildKeypad();
    updateDockedPanel();
    refreshPanels();
    onPrimaryChanged(m_calc->primary(), m_calc->isInError());
    onExpressionChanged(m_calc->expression());
}

void MainWindow::setDateMode()
{
    if (m_panelPopup)
        m_panelPopup->close();
    m_dateMode = true;
    m_pages->setCurrentIndex(1);
    m_modeTitle->setText(modeName(kDateMode));
    updateDockedPanel();
}

QString MainWindow::modeName(int mode)
{
    static const char* keys[] = {"StandardModeText", "ScientificModeText", "ProgrammerModeText", "DateCalculationModeText"};
    return AppStrings::get(QString::fromLatin1(keys[qBound(0, mode, int(kDateMode))]));
}

void MainWindow::pressKey(const Key& key)
{
    switch (key.command)
    {
    case kSecond:
        m_second = !m_second;
        updateSecondLabels();
        return;
    case kShiftLeft:
    case kShiftRight:
    {
        const bool left = key.command == kShiftLeft;
        Command c = Command::CommandNULL;
        switch (m_shiftMode)
        {
        case ShiftMode::Arithmetic: c = left ? Command::CommandLSHF : Command::CommandRSHF; break;
        case ShiftMode::Logical: c = left ? Command::CommandLSHF : Command::CommandRSHFL; break;
        case ShiftMode::Rotate: c = left ? Command::CommandROL : Command::CommandROR; break;
        case ShiftMode::RotateCarry: c = left ? Command::CommandROLC : Command::CommandRORC; break;
        }
        sendCommand(C(c));
        return;
    }
    default:
        sendCommand(m_second && key.altCommand ? key.altCommand : key.command);
    }
}

void MainWindow::sendCommand(int command)
{
    if (command == C(Command::CommandFE))
    {
        m_feOn = !m_feOn;
        if (m_feButton)
            m_feButton->setChecked(m_feOn);
    }
    m_calc->send(command);
}

void MainWindow::updateSecondLabels()
{
    for (auto& k : m_keys)
    {
        if (k.key.command == kSecond)
            k.button->setChecked(m_second);
        else if (!k.key.altLabel.isEmpty())
            k.button->setText(m_second ? k.key.altLabel : k.key.label);
    }
}

void MainWindow::setRadix(RadixType radix)
{
    m_calc->setRadix(radix);
    refreshEnabledKeys();
    refreshProgrammer();
}

void MainWindow::cycleWordSize()
{
    static const std::pair<int, Command> next[] = {
        {32, Command::CommandDword}, {16, Command::CommandWord}, {8, Command::CommandByte}, {64, Command::CommandQword}};
    static const char* names[] = {"DWORD", "WORD", "BYTE", "QWORD"};
    const int i = m_wordBits == 64 ? 0 : m_wordBits == 32 ? 1 : m_wordBits == 16 ? 2 : 3;
    m_wordBits = next[i].first;
    sendCommand(C(next[i].second));
    if (m_wordButton)
        m_wordButton->setText(QString::fromLatin1(names[i]));
    refreshProgrammer();
}

void MainWindow::cycleAngle()
{
    const Command cur = m_calc->angleMode();
    const Command next = cur == Command::CommandRAD ? Command::CommandGRAD : cur == Command::CommandGRAD ? Command::CommandDEG : Command::CommandRAD;
    sendCommand(C(next));
    if (m_angleButton)
        m_angleButton->setText(next == Command::CommandRAD ? QStringLiteral("RAD") : next == Command::CommandGRAD ? QStringLiteral("GRAD") : QStringLiteral("DEG"));
}

void MainWindow::copyResult()
{
    if (m_calc->isInError())
        return;
    QString text = m_calc->primary();
    text.remove(m_calc->groupSeparator());
    text.remove(QChar(' '));
    QGuiApplication::clipboard()->setText(text);
}

void MainWindow::pasteValue()
{
    QString text = QGuiApplication::clipboard()->text().trimmed();
    text.remove(QChar(' '));
    text.remove(QChar(0x00A0));
    text.remove(QChar(0x202F));
    const QChar dec = m_calc->decimalSeparator();
    if (m_calc->groupSeparator() != dec)
        text.remove(m_calc->groupSeparator());

    std::vector<int> cmds;
    bool negative = false;
    if (m_calc->mode() == CalcMode::Programmer)
    {
        const int base = radixBase(m_calc->radix());
        if (text.startsWith('-'))
        {
            negative = true;
            text.remove(0, 1);
        }
        if (text.isEmpty())
            return QApplication::beep();
        for (QChar ch : text)
        {
            bool ok = false;
            const int d = QString(ch).toInt(&ok, 16);
            if (!ok || d >= base)
                return QApplication::beep();
            cmds.push_back(kDigit0 + d);
        }
    }
    else
    {
        static const QRegularExpression re(QStringLiteral("^([+-])?(\\d*)(?:[.,](\\d*))?(?:[eE]([+-])?(\\d+))?$"));
        const auto m = re.match(text);
        if (!m.hasMatch() || (m.captured(2).isEmpty() && m.captured(3).isEmpty()))
            return QApplication::beep();
        negative = m.captured(1) == QLatin1String("-");
        const QString intPart = m.captured(2).isEmpty() ? QStringLiteral("0") : m.captured(2);
        for (QChar ch : intPart)
            cmds.push_back(kDigit0 + ch.digitValue());
        if (!m.captured(3).isEmpty())
        {
            cmds.push_back(C(Command::CommandPNT));
            for (QChar ch : m.captured(3))
                cmds.push_back(kDigit0 + ch.digitValue());
        }
        if (!m.captured(5).isEmpty())
        {
            cmds.push_back(C(Command::CommandEXP));
            for (QChar ch : m.captured(5))
                cmds.push_back(kDigit0 + ch.digitValue());
            if (m.captured(4) == QLatin1String("-"))
                cmds.push_back(C(Command::CommandSIGN));
        }
    }

    // Replace the current entry, then type the number in.
    m_calc->send(Command::CommandCENTR);
    bool signDone = !negative;
    for (int c : cmds)
    {
        m_calc->send(c);
        if (!signDone && c != kDigit0)
        {
            m_calc->send(Command::CommandSIGN);
            signDone = true;
        }
    }
}

// ---------------------------------------------------------------------------
// Display updates
// ---------------------------------------------------------------------------

void MainWindow::onPrimaryChanged(const QString& text, bool isError)
{
    m_primary->setSizeRange(isError ? 16 : 18, isError ? 32 : 48);
    m_primary->setFitText(text);
    if (m_calc->mode() == CalcMode::Programmer)
    {
        refreshProgrammer();
        refreshBits();
    }
}

void MainWindow::onExpressionChanged(const QString& text)
{
    const QFontMetrics fm(m_expression->font());
    m_expression->setText(fm.elidedText(text, Qt::ElideLeft, m_expression->width() - 16));
    m_expression->setToolTip(text);
}

void MainWindow::refreshProgrammer()
{
    if (m_radixRows.empty())
        return;
    const uint32_t bases[] = {16, 10, 8, 2};
    for (auto& row : m_radixRows)
    {
        const uint32_t base = bases[static_cast<int>(row.radix) == static_cast<int>(RadixType::Hex)       ? 0
                                    : static_cast<int>(row.radix) == static_cast<int>(RadixType::Decimal) ? 1
                                    : static_cast<int>(row.radix) == static_cast<int>(RadixType::Octal)   ? 2
                                                                                                         : 3];
        QString v = m_calc->resultForRadix(base, true);
        if (v.isEmpty())
            v = m_calc->primary();
        row.value->setText(v);
        row.button->setChecked(row.radix == m_calc->radix());
    }
}

void MainWindow::refreshBits()
{
    if (m_bitButtons.empty())
        return;
    const QString bin = m_calc->resultForRadix(2, false);
    for (int i = 0; i < 64; ++i)
    {
        const int bit = 63 - i;
        const int pos = static_cast<int>(bin.size()) - 1 - bit;
        const bool set = pos >= 0 && bin.at(pos) == '1';
        m_bitButtons[i]->setText(set ? QStringLiteral("1") : QStringLiteral("0"));
        m_bitButtons[i]->setEnabled(bit < m_wordBits);
    }
}

void MainWindow::refreshEnabledKeys()
{
    const bool programmer = m_calc->mode() == CalcMode::Programmer;
    const int base = programmer ? radixBase(m_calc->radix()) : 10;
    for (auto& k : m_keys)
    {
        const int c = k.key.command;
        if (c >= kDigit0 && c <= kDigitF)
            k.button->setEnabled(c - kDigit0 < base);
        else if (c == C(Command::CommandPNT))
            k.button->setEnabled(!programmer);
    }
}

void MainWindow::refreshMemoryBar()
{
    const bool empty = m_calc->memory().isEmpty();
    if (m_mcButton)
        m_mcButton->setEnabled(!empty);
    if (m_mrButton)
        m_mrButton->setEnabled(!empty);
    if (m_memListButton)
        m_memListButton->setEnabled(!empty);
}

void MainWindow::updateDockedPanel()
{
    const bool docked = width() >= kDockWidth;
    m_sidePanel->setVisible(docked && !m_dateMode);
    m_panelButton->setVisible(!docked && !m_dateMode);
    if (m_memListButton)
        m_memListButton->setVisible(!docked);
    const bool programmer = m_calc->mode() == CalcMode::Programmer;
    m_tabs->setTabVisible(0, !programmer);
    if (programmer)
        m_tabs->setCurrentIndex(1);
    m_panelButton->setText(programmer ? QStringLiteral("M") : QString(QChar(kGlyphHistory)));
    m_panelButton->setToolTip(programmer ? tr("Memory") : tr("History (Ctrl+H)"));
}

// ---------------------------------------------------------------------------
// Events
// ---------------------------------------------------------------------------

void MainWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    updateDockedPanel();
    onExpressionChanged(m_calc->expression());
}

void MainWindow::changeEvent(QEvent* event)
{
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::PaletteChange && !property("applyingTheme").toBool())
        applyTheme();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    saveSettings();
    QMainWindow::closeEvent(event);
}

void MainWindow::keyPressEvent(QKeyEvent* event)
{
    if (!handleKey(event))
        QMainWindow::keyPressEvent(event);
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    // Keys reach the calculator whatever child has focus, as in the Windows app.
    // Date mode needs ordinary typing in its fields, so it is left alone.
    if (!m_dateMode && event->type() == QEvent::KeyPress && watched != this && watched->isWidgetType()
        && static_cast<QWidget*>(watched)->window() == this)
    {
        if (handleKey(static_cast<QKeyEvent*>(event)))
            return true;
    }
    return QMainWindow::eventFilter(watched, event);
}

bool MainWindow::handleKey(QKeyEvent* event)
{
    if (m_dateMode)
    {
        if (event->matches(QKeySequence::Copy) && !qobject_cast<QAbstractSpinBox*>(QApplication::focusWidget()))
        {
            QGuiApplication::clipboard()->setText(m_datePage->resultText());
            return true;
        }
        return false;
    }
    const CalcMode mode = m_calc->mode();
    const bool sci = mode == CalcMode::Scientific;
    const bool prog = mode == CalcMode::Programmer;
    const Qt::KeyboardModifiers mods = event->modifiers() & ~Qt::KeypadModifier;
    const int key = event->key();

    auto send = [this](Command c) { sendCommand(C(c)); };

    if (mods & Qt::ControlModifier)
    {
        switch (key)
        {
        case Qt::Key_C: copyResult(); return true;
        case Qt::Key_V: pasteValue(); return true;
        case Qt::Key_M: m_calc->memoryStore(); return true;
        case Qt::Key_R: if (!m_calc->memory().isEmpty()) m_calc->memoryRecall(0); return true;
        case Qt::Key_P: m_calc->memoryAdd(0); return true;
        case Qt::Key_Q: m_calc->memorySubtract(0); return true;
        case Qt::Key_L: m_calc->memoryClearAll(); return true;
        case Qt::Key_H: showPanelPopup(prog ? 1 : 0); return true;
        case Qt::Key_D:
            if (mods & Qt::ShiftModifier)
                m_calc->clearHistory();
            return true;
        default: break;
        }
        return false;
    }

    switch (key)
    {
    case Qt::Key_Return:
    case Qt::Key_Enter: send(Command::CommandEQU); return true;
    case Qt::Key_Backspace: send(Command::CommandBACK); return true;
    case Qt::Key_Escape: send(Command::CommandCLEAR); return true;
    case Qt::Key_Delete: send(Command::CommandCENTR); return true;
    case Qt::Key_F9: send(Command::CommandSIGN); return true;
    default: break;
    }

    if (sci)
    {
        switch (key)
        {
        case Qt::Key_F3: send(Command::CommandDEG); if (m_angleButton) m_angleButton->setText(QStringLiteral("DEG")); return true;
        case Qt::Key_F4: send(Command::CommandRAD); if (m_angleButton) m_angleButton->setText(QStringLiteral("RAD")); return true;
        case Qt::Key_F5: send(Command::CommandGRAD); if (m_angleButton) m_angleButton->setText(QStringLiteral("GRAD")); return true;
        default: break;
        }
    }
    if (prog)
    {
        switch (key)
        {
        case Qt::Key_F5: setRadix(RadixType::Hex); return true;
        case Qt::Key_F6: setRadix(RadixType::Decimal); return true;
        case Qt::Key_F7: setRadix(RadixType::Octal); return true;
        case Qt::Key_F8: setRadix(RadixType::Binary); return true;
        default: break;
        }
    }

    const QString text = event->text();
    if (text.size() != 1)
    {
        return false;
    }
    const QChar ch = text.at(0);

    // Digits, and A-F in hexadecimal.
    {
        bool ok = false;
        const int d = QString(ch).toInt(&ok, 16);
        const int base = prog ? radixBase(m_calc->radix()) : 10;
        if (ok && d < base && (ch.isDigit() || prog))
        {
            sendCommand(kDigit0 + d);
            return true;
        }
    }

    switch (ch.unicode())
    {
    case '+': send(Command::CommandADD); return true;
    case '-': send(Command::CommandSUB); return true;
    case '*': send(Command::CommandMUL); return true;
    case '/': send(Command::CommandDIV); return true;
    case '=': send(Command::CommandEQU); return true;
    case '.':
    case ',':
        if (!prog)
            send(Command::CommandPNT);
        return true;
    case '%': send(prog ? Command::CommandMOD : Command::CommandPERCENT); return true;
    case '(':
        if (sci || prog)
            send(Command::CommandOPENP);
        return true;
    case ')':
        if (sci || prog)
            send(Command::CommandCLOSEP);
        return true;
    default: break;
    }

    if (!prog)
    {
        switch (ch.unicode())
        {
        case 'r': send(Command::CommandREC); return true;
        case '@': send(Command::CommandSQRT); return true;
        case 'q': send(Command::CommandSQR); return true;
        default: break;
        }
    }
    if (sci)
    {
        switch (ch.unicode())
        {
        case '^':
        case 'y': send(Command::CommandPWR); return true;
        case '#': send(Command::CommandCUB); return true;
        case '!': send(Command::CommandFAC); return true;
        case 'p': send(Command::CommandPI); return true;
        case 'e': send(Command::CommandEuler); return true;
        case 's': send(Command::CommandSIN); return true;
        case 'o': send(Command::CommandCOS); return true;
        case 't': send(Command::CommandTAN); return true;
        case 'S': send(Command::CommandASIN); return true;
        case 'O': send(Command::CommandACOS); return true;
        case 'T': send(Command::CommandATAN); return true;
        case 'n': send(Command::CommandLN); return true;
        case 'l': send(Command::CommandLOG); return true;
        case 'x': send(Command::CommandEXP); return true;
        case 'm': send(Command::CommandDMS); return true;
        case 'v': send(Command::CommandFE); return true;
        default: break;
        }
    }
    if (prog)
    {
        switch (ch.unicode())
        {
        case '<': pressKey(Key{{}, kShiftLeft, {}}); return true;
        case '>': pressKey(Key{{}, kShiftRight, {}}); return true;
        case '&': send(Command::CommandAnd); return true;
        case '|': send(Command::CommandOR); return true;
        case '~': send(Command::CommandNot); return true;
        case '^': send(Command::CommandXor); return true;
        default: break;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// Theme and settings
// ---------------------------------------------------------------------------

void MainWindow::applyTheme()
{
    bool dark = QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
    if (QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Unknown)
        dark = QGuiApplication::palette().color(QPalette::Window).lightness() < 128;
    if (m_theme != QLatin1String("system"))
        dark = m_theme == QLatin1String("dark");

    // Windows 11 Calculator colours.
    struct Theme
    {
        const char *bg, *panel, *digit, *digitHover, *func, *funcHover, *press, *text, *subtext, *disabled, *border,
            *accent, *accentText, *accentHover, *accentFg, *popup;
    };
    static const Theme darkTheme{"#202020", "#2b2b2b", "#3b3b3b", "#363636", "#323232", "#3b3b3b", "#282828", "#ffffff", "#c5c5c5",
                                 "#6e6e6e", "#2e2e2e", "#4cc2ff", "#000000", "#48b2e9", "#4cc2ff", "#2c2c2c"};
    static const Theme lightTheme{"#f3f3f3", "#ebebeb", "#ffffff", "#f9f9f9", "#f9f9f9", "#f0f0f0", "#e8e8e8", "#1b1b1b", "#5f5f5f",
                                  "#a0a0a0", "#e5e5e5", "#005fb8", "#ffffff", "#1a6fc2", "#005fb8", "#fbfbfb"};
    const Theme& t = dark ? darkTheme : lightTheme;

    QString css = QStringLiteral(R"(
        QMainWindow, QWidget#central { background: @bg; }
        QWidget#sidePanel { background: @bg; }
        QLabel { color: @text; }
        QLabel#modeTitle { font-size: 19px; font-weight: 600; padding-left: 6px; }
        QLabel#expression { color: @subtext; font-size: 14px; }
        QLabel#primary { color: @text; font-weight: 600; }
        QLabel#radixValue { font-size: 14px; }
        QLabel#bitIndex { color: @subtext; font-size: 10px; }
        QLabel#memoryValue { font-size: 20px; font-weight: 600; }
        QPushButton { border: 1px solid @border; border-radius: 5px; background: @func; color: @text;
                      font-size: 15px; min-height: 30px; padding: 0 6px; }
        QPushButton:hover { background: @funcHover; }
        QPushButton:pressed { background: @press; color: @subtext; }
        QPushButton:disabled { color: @disabled; }
        QPushButton:checked { color: @accentFg; }
        QPushButton[kind="digit"] { background: @digit; font-size: 18px; font-weight: 600; }
        QPushButton[kind="digit"]:hover { background: @digitHover; }
        QPushButton[kind="digit"]:pressed { background: @press; }
        QPushButton[kind="equal"] { background: @accent; color: @accentText; border-color: @accent; font-size: 22px; }
        QPushButton[kind="equal"]:hover { background: @accentHover; }
        QPushButton[kind="bar"], QPushButton[kind="radix"], QPushButton[kind="bit"], QPushButton[kind="menu"] {
                      background: transparent; border: none; font-size: 13px; min-height: 28px; }
        QPushButton[kind="bar"]:hover, QPushButton[kind="radix"]:hover, QPushButton[kind="bit"]:hover,
        QPushButton[kind="menu"]:hover { background: @funcHover; }
        QPushButton[kind="radix"] { text-align: left; padding-left: 10px; font-weight: 600; border-left: 3px solid transparent;
                      border-radius: 0; }
        QPushButton[kind="radix"]:checked { border-left: 3px solid @accentFg; color: @text; }
        QPushButton[kind="menu"] { text-align: left; padding: 0 12px; }
        QPushButton[kind="bit"] { font-family: monospace; font-size: 16px; min-width: 12px; padding: 0; }
        QPushButton[kind="small"] { background: transparent; font-size: 12px; min-height: 22px; padding: 0 8px; }
        QPushButton[kind="small"]:hover { background: @funcHover; }
        QToolButton { background: transparent; border: none; color: @text; padding: 6px; border-radius: 4px; font-size: 16px; }
        QToolButton:hover { background: @funcHover; }
        QToolButton:checked { background: @press; }
        QToolButton::menu-indicator { image: none; }
        QToolButton[kind="icon"] { font-family: "Calculator Fluent Icons"; font-size: 16px; }
        QListWidget { background: transparent; border: none; color: @subtext; outline: none; }
        QListWidget::item { border-radius: 4px; }
        QListWidget::item:hover { background: @funcHover; }
        QListWidget::item:selected { background: @press; }
        QTabWidget::pane { border: none; }
        QTabBar::tab { background: transparent; color: @text; padding: 6px 14px; border: none; font-weight: 600;
                       border-bottom: 2px solid transparent; }
        QTabBar::tab:selected { border-bottom: 2px solid @accentFg; }
        QFrame#popup { background: @popup; border: 1px solid @border; border-radius: 8px; }
        QMenu { background: @popup; color: @text; border: 1px solid @border; padding: 4px; }
        QMenu::item { padding: 6px 24px 6px 12px; border-radius: 4px; }
        QMenu::item:selected { background: @funcHover; }
        QLabel#dateHeader { color: @subtext; font-size: 13px; }
        QLabel#dateResult { font-size: 24px; font-weight: 600; }
        QLabel#dateSubResult { color: @subtext; font-size: 15px; }
        QComboBox, QDateEdit, QSpinBox { background: @digit; color: @text; border: 1px solid @border; border-radius: 5px;
                      padding: 5px 8px; min-height: 22px; font-size: 14px; }
        QComboBox:hover, QDateEdit:hover, QSpinBox:hover { background: @digitHover; }
        QComboBox::drop-down, QDateEdit::drop-down { subcontrol-origin: padding; subcontrol-position: center right;
                      border: none; width: 24px; }
        QComboBox::down-arrow, QDateEdit::down-arrow, QSpinBox::down-arrow { image: url(:/icons/chevron-down.png);
                      width: 12px; height: 12px; }
        QSpinBox::up-arrow { image: url(:/icons/chevron-up.png); width: 12px; height: 12px; }
        QSpinBox { padding-right: 24px; }
        QSpinBox::up-button, QSpinBox::down-button { subcontrol-origin: border; width: 22px; border: none;
                      background: transparent; }
        QSpinBox::up-button { subcontrol-position: top right; }
        QSpinBox::down-button { subcontrol-position: bottom right; }
        QSpinBox::up-button:hover, QSpinBox::down-button:hover { background: @funcHover; }
        QComboBox QAbstractItemView { background: @popup; color: @text; selection-background-color: @funcHover;
                      selection-color: @text; border: 1px solid @border; }
        QRadioButton { color: @text; font-size: 14px; spacing: 6px; }
        QCalendarWidget QWidget { background: @popup; color: @text; alternate-background-color: @func; }
        QCalendarWidget QToolButton { color: @text; font-size: 14px; }
        QCalendarWidget QAbstractItemView:enabled { selection-background-color: @accent; selection-color: @accentText; }
        QToolTip { background: @popup; color: @text; border: 1px solid @border; }
        QScrollBar:vertical { background: transparent; width: 6px; }
        QScrollBar::handle:vertical { background: @disabled; border-radius: 3px; min-height: 24px; }
        QScrollBar::add-line, QScrollBar::sub-line { height: 0; }
    )");
    const std::pair<const char*, const char*> tokens[] = {
        {"@bg", t.bg}, {"@panel", t.panel}, {"@digitHover", t.digitHover}, {"@digit", t.digit}, {"@funcHover", t.funcHover},
        {"@func", t.func}, {"@press", t.press}, {"@subtext", t.subtext}, {"@text", t.text}, {"@disabled", t.disabled},
        {"@border", t.border}, {"@accentText", t.accentText}, {"@accentHover", t.accentHover}, {"@accentFg", t.accentFg},
        {"@accent", t.accent}, {"@popup", t.popup},
    };
    for (const auto& [k, v] : tokens)
        css.replace(QLatin1String(k), QLatin1String(v));

    setProperty("applyingTheme", true);
    qApp->setStyleSheet(css);
    setProperty("applyingTheme", false);
}

void MainWindow::loadSettings()
{
    QSettings s;
    const int mode = s.value(QStringLiteral("mode"), 0).toInt();
    if (!restoreGeometry(s.value(QStringLiteral("geometry")).toByteArray()))
        resize(340, 580);
    m_topButton->setChecked(s.value(QStringLiteral("keepOnTop"), false).toBool());
    m_theme = s.value(QStringLiteral("theme"), QStringLiteral("system")).toString();
    applyTheme();
    setMode(static_cast<CalcMode>(qBound(0, mode, 2)));
    if (mode == kDateMode)
        setDateMode();
}

void MainWindow::saveSettings()
{
    QSettings s;
    s.setValue(QStringLiteral("mode"), m_dateMode ? int(kDateMode) : static_cast<int>(m_calc->mode()));
    s.setValue(QStringLiteral("geometry"), saveGeometry());
    s.setValue(QStringLiteral("keepOnTop"), m_topButton->isChecked());
    s.setValue(QStringLiteral("theme"), m_theme);
}
