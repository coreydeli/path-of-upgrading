// SPDX-License-Identifier: MIT
#include "services/tradesignin.h"

#include "core/tradesite.h"
#include "services/poeclient.h"

using namespace Qt::Literals::StringLiterals;

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

void BearerSignIn::sign(const QNetworkRequest &request, const QObject *context, Done done)
{
    const QUrl url = tradeApiUrl(m_base, request.url());
    if (url.isEmpty() || url.host() != m_base.host()) {
        done(request, u"That request can't be made as your Path of Exile account."_s);
        return;
    }
    QNetworkRequest signedIn = request;
    signedIn.setUrl(url);
    signedIn.setAttribute(QNetworkRequest::CookieLoadControlAttribute, QNetworkRequest::Manual);
    signedIn.setAttribute(QNetworkRequest::CookieSaveControlAttribute, QNetworkRequest::Manual);
    signedIn.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                          QNetworkRequest::SameOriginRedirectPolicy);
    // nothing in the handler leans on this object, which can go while the
    // token is on its way
    m_tokens->withToken(
        context, [signedIn, done = std::move(done)](const QString &token, const QString &error) {
            if (token.isEmpty()) {
                done(signedIn, error.isEmpty() ? u"Your Path of Exile account isn't linked."_s : error);
                return;
            }
            QNetworkRequest withToken = signedIn;
            withToken.setRawHeader("Authorization", "Bearer " + token.toUtf8());
            done(withToken, {});
        });
}

bool BearerSignIn::refused()
{
    m_tokens->rejectToken();
    return true;
}

} // namespace pou
