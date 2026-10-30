// SPDX-License-Identifier: MIT
#include "app/tradereport.h"

#include <QLocale>

using namespace Qt::Literals::StringLiterals;

namespace pou {

TradeReport searchingReport(std::chrono::milliseconds wait)
{
    TradeReport report{.asked = true, .busy = true};
    const auto seconds = std::chrono::ceil<std::chrono::seconds>(wait).count();
    if (seconds <= 0) {
        report.status = u"Searching the trade site"_s;
        return report;
    }
    report.status = seconds == 1 ? u"Waiting a second for the trade site"_s
                                 : u"Waiting %1 seconds for the trade site"_s.arg(seconds);
    report.note = u"It takes only a few searches at a time."_s;
    return report;
}

TradeReport reportOf(const TradeFinder::Found &found)
{
    TradeReport report{.asked = true, .title = found.title, .note = found.note, .url = found.url};
    report.listed = found.total > 0;
    report.status =
        found.total == 0
            ? u"None listed in %1 right now"_s.arg(found.league)
            : u"%1 listed in %2"_s.arg(QLocale(QLocale::English).toString(found.total), found.league);
    return report;
}

TradeReport failedReport(const QString &why, const QUrl &url)
{
    return {.asked = true, .failed = true, .status = why, .url = url};
}

} // namespace pou
