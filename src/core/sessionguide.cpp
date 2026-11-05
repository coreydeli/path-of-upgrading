// SPDX-License-Identifier: MIT
#include "core/sessionguide.h"

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

// "1. Open it." and so on
QStringList numbered(const QStringList &steps)
{
    QStringList lines;
    for (qsizetype i = 0; i < steps.size(); ++i)
        lines << u"%1. %2"_s.arg(i + 1).arg(steps[i]);
    return lines;
}

} // namespace

QUrl signInUrl()
{
    return QUrl(u"https://www.pathofexile.com/login"_s);
}

SessionGuide guideToSessionId()
{
    return {
        .steps = numbered({
            u"Sign in to the Path of Exile website in your browser."_s,
            u"Press F12 to open the browser's developer tools."_s,
            u"In Firefox, open the Storage tab. In Chrome and other Chromium browsers, open the "
            u"Application tab."_s,
            u"In the list on the left, open Cookies, then https://www.pathofexile.com."_s,
            u"Find the cookie named POESESSID and copy its value, a long string of letters and "
            u"digits."_s,
            u"Paste it into the Session ID field and press Save."_s,
        }),
        .notes =
            {
                u"Keep it to yourself. Anyone who has it can act as you on the site."_s,
                u"It's kept in your system keyring."_s,
                u"Signing out of the website ends it, and then you need to paste a new one."_s,
            },
    };
}

} // namespace pou
