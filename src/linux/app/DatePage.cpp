#include "DatePage.h"

#include <QComboBox>
#include <QDateEdit>
#include <QGridLayout>
#include <QLabel>
#include <QLocale>
#include <QRadioButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "AppStrings.h"
#include "DateCalculator.h"

namespace
{
    constexpr int kMaxOffset = 999; // MaxOffsetValue upstream

    QString tr_(const char* key) { return AppStrings::get(QString::fromLatin1(key)); }

    QLabel* header(const QString& text, QWidget* parent)
    {
        auto* l = new QLabel(text, parent);
        l->setObjectName(QStringLiteral("dateHeader"));
        return l;
    }

    QString unitText(int n, const char* one, const char* many)
    {
        return QLocale().toString(n) + ' ' + tr_(n == 1 ? one : many);
    }
}

DatePage::DatePage(QWidget* parent)
    : QWidget(parent)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(12, 8, 12, 12);
    lay->setSpacing(6);

    m_option = new QComboBox(this);
    m_option->addItem(tr_("Date_DifferenceOption.Content"));
    m_option->addItem(tr_("Date_AddSubtractOption.Content"));
    lay->addWidget(m_option);
    lay->addSpacing(8);

    m_pages = new QStackedWidget(this);
    lay->addWidget(m_pages);
    lay->addStretch(1);

    // Difference between dates
    {
        auto* page = new QWidget(m_pages);
        auto* v = new QVBoxLayout(page);
        v->setContentsMargins(0, 0, 0, 0);
        v->setSpacing(4);
        m_from = makeDateEdit();
        m_to = makeDateEdit();
        v->addWidget(header(tr_("DateDiff_FromHeader.Header"), page));
        v->addWidget(m_from);
        v->addSpacing(8);
        v->addWidget(header(tr_("DateDiff_ToHeader.Header"), page));
        v->addWidget(m_to);
        v->addSpacing(16);
        v->addWidget(header(tr_("Date_DifferenceLabel.Text"), page));
        m_diffResult = new QLabel(page);
        m_diffResult->setObjectName(QStringLiteral("dateResult"));
        m_diffResult->setWordWrap(true);
        m_diffResult->setTextInteractionFlags(Qt::TextSelectableByMouse);
        m_diffInDays = new QLabel(page);
        m_diffInDays->setObjectName(QStringLiteral("dateSubResult"));
        m_diffInDays->setTextInteractionFlags(Qt::TextSelectableByMouse);
        v->addWidget(m_diffResult);
        v->addWidget(m_diffInDays);
        m_pages->addWidget(page);
    }

    // Add or subtract days
    {
        auto* page = new QWidget(m_pages);
        auto* v = new QVBoxLayout(page);
        v->setContentsMargins(0, 0, 0, 0);
        v->setSpacing(4);
        m_start = makeDateEdit();
        v->addWidget(header(tr_("AddSubtract_Date_FromHeader.Header"), page));
        v->addWidget(m_start);
        v->addSpacing(8);

        auto* radios = new QHBoxLayout;
        m_add = new QRadioButton(tr_("AddOption.Content"), page);
        m_subtract = new QRadioButton(tr_("SubtractOption.Content"), page);
        m_add->setChecked(true);
        radios->addWidget(m_add);
        radios->addWidget(m_subtract);
        radios->addStretch(1);
        v->addLayout(radios);
        v->addSpacing(8);

        auto* grid = new QGridLayout;
        grid->setHorizontalSpacing(8);
        const std::pair<const char*, QSpinBox**> spins[] = {
            {"YearsLabel.Text", &m_years}, {"MonthsLabel.Text", &m_months}, {"DaysLabel.Text", &m_days}};
        int col = 0;
        for (const auto& [key, spin] : spins)
        {
            grid->addWidget(header(tr_(key), page), 0, col);
            *spin = new QSpinBox(page);
            (*spin)->setRange(0, kMaxOffset);
            (*spin)->setAccelerated(true);
            grid->addWidget(*spin, 1, col);
            connect(*spin, &QSpinBox::valueChanged, this, &DatePage::recalculate);
            ++col;
        }
        v->addLayout(grid);
        v->addSpacing(16);
        v->addWidget(header(tr_("DateLabel.Text"), page));
        m_dateResult = new QLabel(page);
        m_dateResult->setObjectName(QStringLiteral("dateResult"));
        m_dateResult->setWordWrap(true);
        m_dateResult->setTextInteractionFlags(Qt::TextSelectableByMouse);
        v->addWidget(m_dateResult);
        m_pages->addWidget(page);

        connect(m_add, &QRadioButton::toggled, this, &DatePage::recalculate);
    }

    connect(m_option, &QComboBox::currentIndexChanged, this, [this](int i) {
        m_pages->setCurrentIndex(i);
        recalculate();
    });
    for (QDateEdit* e : {m_from, m_to, m_start})
        connect(e, &QDateEdit::dateChanged, this, &DatePage::recalculate);

    recalculate();
}

QDateEdit* DatePage::makeDateEdit()
{
    auto* e = new QDateEdit(QDate::currentDate(), this);
    e->setCalendarPopup(true);
    e->setDateRange(DateCalculator::minDate(), DateCalculator::maxDate());
    // The locale's short format, always with a four-digit year.
    QString fmt = QLocale().dateFormat(QLocale::ShortFormat);
    if (!fmt.contains(QLatin1String("yyyy")))
        fmt.replace(QLatin1String("yy"), QLatin1String("yyyy"));
    e->setDisplayFormat(fmt);
    return e;
}

void DatePage::recalculate()
{
    if (m_pages->currentIndex() == 0)
    {
        // Same decision tree as DateCalculatorViewModel.UpdateDisplayResult.
        const auto inDays = DateCalculator::difference(m_from->date(), m_to->date(), DateUnit::Day);
        if (!inDays)
        {
            m_diffResult->setText(tr_("CalculationFailed"));
            m_diffInDays->clear();
            return;
        }
        const QString daysText = unitText(inDays->day, "Date_Day", "Date_Days");
        if (inDays->day == 0)
        {
            m_diffResult->setText(tr_("Date_SameDates"));
            m_diffInDays->clear();
            return;
        }
        const auto full = DateCalculator::difference(m_from->date(), m_to->date(), DateUnit::All);
        if (!full || (full->year == 0 && full->month == 0 && full->week == 0))
        {
            m_diffResult->setText(daysText);
            m_diffInDays->clear();
            return;
        }
        QStringList parts;
        if (full->year > 0)
            parts << unitText(full->year, "Date_Year", "Date_Years");
        if (full->month > 0)
            parts << unitText(full->month, "Date_Month", "Date_Months");
        if (full->week > 0)
            parts << unitText(full->week, "Date_Week", "Date_Weeks");
        if (full->day > 0)
            parts << unitText(full->day, "Date_Day", "Date_Days");
        m_diffResult->setText(parts.join(QStringLiteral(", ")));
        m_diffInDays->setText(daysText);
    }
    else
    {
        const DateDifference duration{m_years->value(), m_months->value(), 0, m_days->value()};
        const auto result = m_add->isChecked() ? DateCalculator::addDuration(m_start->date(), duration)
                                               : DateCalculator::subtractDuration(m_start->date(), duration);
        m_dateResult->setText(result ? QLocale().toString(*result, QLocale::LongFormat) : tr_("Date_OutOfBoundMessage"));
    }
}

QString DatePage::resultText() const
{
    return m_pages->currentIndex() == 0 ? m_diffResult->text() : m_dateResult->text();
}
