#include "DateCalculator.h"

namespace
{
    bool inRange(const QDate& d)
    {
        return d.isValid() && d >= DateCalculator::minDate() && d <= DateCalculator::maxDate();
    }

    std::optional<QDate> checked(const QDate& d)
    {
        return inRange(d) ? std::optional<QDate>(d) : std::nullopt;
    }

    // AdjustCalendarDate upstream: add years, months or weeks.
    std::optional<QDate> adjust(const QDate& date, int unit, int amount)
    {
        switch (unit)
        {
        case DateUnit::Year: return checked(date.addYears(amount));
        case DateUnit::Month: return checked(date.addMonths(amount));
        case DateUnit::Week: return checked(date.addDays(7LL * amount));
        default: return date;
        }
    }
}

QDate DateCalculator::minDate()
{
    return QDate(1601, 1, 1);
}

QDate DateCalculator::maxDate()
{
    return QDate(9999, 12, 31);
}

std::optional<QDate> DateCalculator::addDuration(QDate start, const DateDifference& duration)
{
    // Same order as upstream: years, then months, then days.
    std::optional<QDate> d = checked(start);
    if (d && duration.year != 0)
        d = checked(d->addYears(duration.year));
    if (d && duration.month != 0)
        d = checked(d->addMonths(duration.month));
    if (d && duration.day != 0)
        d = checked(d->addDays(duration.day));
    return d;
}

std::optional<QDate> DateCalculator::subtractDuration(QDate start, const DateDifference& duration)
{
    // Reverse order: days, then months, then years.
    std::optional<QDate> d = checked(start);
    if (d && duration.day != 0)
        d = checked(d->addDays(-duration.day));
    if (d && duration.month != 0)
        d = checked(d->addMonths(-duration.month));
    if (d && duration.year != 0)
        d = checked(d->addYears(-duration.year));
    return d;
}

std::optional<DateDifference> DateCalculator::difference(QDate date1, QDate date2, int outputFormat)
{
    if (!inRange(date1) || !inRange(date2))
        return std::nullopt;

    const QDate startDate = date1 < date2 ? date1 : date2;
    const QDate endDate = date1 < date2 ? date2 : date1;
    QDate pivotDate = startDate;
    long long daysDiff = startDate.daysTo(endDate);
    long long diff[4] = {0, 0, 0, 0};

    if ((outputFormat & 7) != 0)
    {
        // Upstream approximates a year with the end date's year and a month
        // with the start date's month, then corrects by stepping the pivot.
        const long long daysIn[] = {endDate.daysInYear(), startDate.daysInMonth(), 7, 1};
        for (int unitIndex = 0; unitIndex < 3; ++unitIndex)
        {
            const QDate tempPivotDate = pivotDate;
            const int unit = 1 << unitIndex;
            if ((outputFormat & unit) == 0)
                continue;

            bool isEndDateHit = false;
            diff[unitIndex] = daysDiff / daysIn[unitIndex];
            while (diff[unitIndex] != 0)
            {
                if (auto p = adjust(tempPivotDate, unit, static_cast<int>(diff[unitIndex])))
                {
                    pivotDate = *p;
                    break;
                }
                diff[unitIndex] -= 1;
            }

            long long tempDaysDiff;
            do
            {
                tempDaysDiff = pivotDate.daysTo(endDate);
                if (tempDaysDiff < 0)
                {
                    // Overshot the end date: step back one unit.
                    if (diff[unitIndex] == 0)
                        return std::nullopt;
                    diff[unitIndex] -= 1;
                    auto p = adjust(tempPivotDate, unit, static_cast<int>(diff[unitIndex]));
                    if (!p)
                        return std::nullopt;
                    pivotDate = *p;
                    isEndDateHit = true;
                }
                else if (tempDaysDiff > 0)
                {
                    if (isEndDateHit)
                        break;
                    // Undershot: try one more unit.
                    auto p = adjust(tempPivotDate, unit, static_cast<int>(diff[unitIndex] + 1));
                    if (!p)
                        break;
                    pivotDate = *p;
                    diff[unitIndex] += 1;
                }
            } while (tempDaysDiff != 0);

            const long long signedDaysDiff = pivotDate.daysTo(endDate);
            if (signedDaysDiff < 0)
                return std::nullopt;
            daysDiff = signedDaysDiff;
        }
    }

    diff[3] = daysDiff;
    return DateDifference{static_cast<int>(diff[0]), static_cast<int>(diff[1]), static_cast<int>(diff[2]), static_cast<int>(diff[3])};
}
