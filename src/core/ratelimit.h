// SPDX-License-Identifier: MIT
#pragma once

#include <QList>
#include <QString>

namespace pou {

// The trade site's request limits. Its answers give them in x-rate-limit-ip
// and, when you're signed in, x-rate-limit-account, and searches have limits
// apart from fetching listings.

// One of the limits: "5:10:60" is 5 requests in 10 seconds, with a ban of 60
// seconds for more. In a -state header the numbers are the requests made in
// that window so far and how long a ban has left to run.
struct RateWindow
{
    int hits = 0;
    int seconds = 0;
    int ban = 0;

    friend bool operator==(const RateWindow &, const RateWindow &) = default;
};

// The windows in a header, "5:10:60,15:60:300", leaving out any it can't read.
QList<RateWindow> rateWindows(QStringView header);

} // namespace pou
