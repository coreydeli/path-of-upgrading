// SPDX-License-Identifier: MIT
#include "core/ratelimit.h"

namespace pou {
namespace {

// The number, or -1 when there's none.
int number(QStringView text)
{
    bool ok = false;
    const int n = text.toInt(&ok);
    return ok ? n : -1;
}

} // namespace

QList<RateWindow> rateWindows(QStringView header)
{
    QList<RateWindow> windows;
    for (QStringView part : header.split(u',', Qt::SkipEmptyParts)) {
        const QList<QStringView> numbers = part.split(u':');
        if (numbers.size() != 3)
            continue;
        const RateWindow window{number(numbers[0]), number(numbers[1]), number(numbers[2])};
        if (window.hits >= 0 && window.seconds > 0 && window.ban >= 0)
            windows << window;
    }
    return windows;
}

} // namespace pou
