// Checks of the date engine port against the behaviour of the Windows app.
#include <cstdio>

#include "DateCalculator.h"

namespace
{
    int failures = 0;

    void check(bool ok, const char* what)
    {
        std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
        if (!ok)
            ++failures;
    }

    bool diffIs(QDate a, QDate b, int y, int m, int w, int d)
    {
        const auto r = DateCalculator::difference(a, b, DateUnit::All);
        if (!r)
            return false;
        if (r->year != y || r->month != m || r->week != w || r->day != d)
        {
            std::printf("      got %d y %d m %d w %d d\n", r->year, r->month, r->week, r->day);
            return false;
        }
        return true;
    }

    int days(QDate a, QDate b)
    {
        const auto r = DateCalculator::difference(a, b, DateUnit::Day);
        return r ? r->day : -1;
    }
}

int main()
{
    using DC = DateDifference;
    namespace D = DateCalculator;

    check(diffIs(QDate(2024, 1, 1), QDate(2025, 1, 1), 1, 0, 0, 0), "2024-01-01 to 2025-01-01 is 1 year");
    check(days(QDate(2024, 1, 1), QDate(2025, 1, 1)) == 366, "... which is 366 days (leap year)");
    check(diffIs(QDate(2025, 1, 1), QDate(2024, 1, 1), 1, 0, 0, 0), "order of dates does not matter");
    check(diffIs(QDate(2024, 1, 31), QDate(2024, 3, 1), 0, 1, 0, 1), "2024-01-31 to 2024-03-01 is 1 month, 1 day");
    check(diffIs(QDate(2026, 10, 1), QDate(2026, 10, 24), 0, 0, 3, 2), "23 days is 3 weeks, 2 days");
    check(diffIs(QDate(2000, 2, 29), QDate(2026, 10, 24), 26, 7, 3, 5), "2000-02-29 to 2026-10-24 (Feb 29 + 26 y clamps to Feb 28)");
    check(diffIs(QDate(2026, 5, 5), QDate(2026, 5, 5), 0, 0, 0, 0), "same dates");
    check(days(QDate(1601, 1, 1), QDate(9999, 12, 31)) == 3067670, "full supported range in days");
    check(!D::difference(QDate(1600, 12, 31), QDate(2000, 1, 1), DateUnit::All), "before 1601 fails");

    check(D::addDuration(QDate(2024, 1, 31), DC{0, 1, 0, 0}) == QDate(2024, 2, 29), "Jan 31 + 1 month = Feb 29 (leap)");
    check(D::addDuration(QDate(2020, 2, 29), DC{1, 0, 0, 0}) == QDate(2021, 2, 28), "Feb 29 + 1 year = Feb 28");
    check(D::addDuration(QDate(2026, 10, 24), DC{1, 2, 0, 10}) == QDate(2028, 1, 3), "years, then months, then days");
    check(D::subtractDuration(QDate(2026, 10, 24), DC{0, 0, 0, 24}) == QDate(2026, 9, 30), "subtract 24 days");
    check(D::subtractDuration(QDate(2024, 3, 31), DC{0, 1, 0, 1}) == QDate(2024, 2, 29), "subtract days first, then months");
    check(!D::subtractDuration(QDate(1601, 1, 1), DC{0, 0, 0, 1}), "before 1601-01-01 is out of bound");
    check(!D::addDuration(QDate(9999, 12, 31), DC{0, 0, 0, 1}), "after 9999-12-31 is out of bound");

    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
