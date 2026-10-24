// Port of the upstream DateCalculationEngine (Common/DateCalculator.cs),
// Gregorian calendar only. Supported range is 1601-01-01 to 9999-12-31, as on
// Windows; leaving it is the case where upstream catches ArgumentException.
#pragma once

#include <QDate>
#include <optional>

struct DateDifference
{
    int year = 0;
    int month = 0;
    int week = 0;
    int day = 0;
};

namespace DateUnit
{
    enum : int
    {
        Year = 0x01,
        Month = 0x02,
        Week = 0x04,
        Day = 0x08,
        All = Year | Month | Week | Day,
    };
}

namespace DateCalculator
{
    QDate minDate();
    QDate maxDate();

    std::optional<QDate> addDuration(QDate start, const DateDifference& duration);
    std::optional<QDate> subtractDuration(QDate start, const DateDifference& duration);

    // Difference between two dates, split into the units in outputFormat.
    // The order of the dates does not matter.
    std::optional<DateDifference> difference(QDate date1, QDate date2, int outputFormat);
}
