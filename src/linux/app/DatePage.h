// Date calculation view: difference between dates, or add/subtract a
// duration. Logic from DateCalculatorViewModel.cs upstream; Gregorian only.
#pragma once

#include <QWidget>

class QComboBox;
class QDateEdit;
class QLabel;
class QRadioButton;
class QSpinBox;
class QStackedWidget;

class DatePage final : public QWidget
{
    Q_OBJECT

public:
    explicit DatePage(QWidget* parent = nullptr);

    // The text the Copy command puts on the clipboard.
    QString resultText() const;

private:
    QDateEdit* makeDateEdit();
    void recalculate();

    QComboBox* m_option = nullptr;
    QStackedWidget* m_pages = nullptr;

    QDateEdit* m_from = nullptr;
    QDateEdit* m_to = nullptr;
    QLabel* m_diffResult = nullptr;
    QLabel* m_diffInDays = nullptr;

    QDateEdit* m_start = nullptr;
    QRadioButton* m_add = nullptr;
    QRadioButton* m_subtract = nullptr;
    QSpinBox* m_years = nullptr;
    QSpinBox* m_months = nullptr;
    QSpinBox* m_days = nullptr;
    QLabel* m_dateResult = nullptr;
};
