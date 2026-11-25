// SPDX-License-Identifier: MIT
#include "services/tradesignin.h"

namespace pou {

void SessionCookieSignIn::sign(const QNetworkRequest &request, const QObject *, Done done)
{
    QNetworkRequest signedIn = request;
    signedIn.setRawHeader(QByteArrayLiteral("Cookie"), QByteArrayLiteral("POESESSID=") + m_id);
    // or the network's cookie jar sends its own in place of it
    signedIn.setAttribute(QNetworkRequest::CookieLoadControlAttribute, QNetworkRequest::Manual);
    // nor keeps one the site sends back, which would still go out once
    // you've signed out
    signedIn.setAttribute(QNetworkRequest::CookieSaveControlAttribute, QNetworkRequest::Manual);
    // and a redirect to another site doesn't get it
    signedIn.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                          QNetworkRequest::SameOriginRedirectPolicy);
    done(signedIn, {});
}

} // namespace pou
