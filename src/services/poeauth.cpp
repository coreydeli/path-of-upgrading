// SPDX-License-Identifier: MIT
#include "services/poeauth.h"

#include "core/poeapi.h"
#include "services/network.h"

#include <QNetworkAccessManager>

using namespace Qt::Literals::StringLiterals;

namespace pou {

PoeAuth::PoeAuth(QNetworkAccessManager &network, PoeConfig config, QObject *parent)
    : QObject(parent)
    , m_network(network)
    , m_config(std::move(config))
    , m_tokenUrl(u"https://www.pathofexile.com/oauth/token"_s)
{
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this,
            [this] { fail(u"Nothing came back from the browser in time. Try linking again."_s); });
    connect(&m_server, &LoopbackServer::requested, this, &PoeAuth::answered);
}

void PoeAuth::start()
{
    cancel();
    const quint16 port = m_server.listen(m_config.ports);
    if (port == 0) {
        Q_EMIT failed(u"Path of Upgrading couldn't listen for the browser, as the ports it uses are taken. "
                      "Close whatever is using them and try again."_s,
                      false);
        return;
    }
    const Pkce pkce = newPkce();
    m_state =
        std::make_unique<Waiting>(Waiting{.state = newState(), .verifier = pkce.verifier, .port = port});
    m_timer.start(m_waitLimit);
    Q_EMIT browserWanted(authorizeUrl(m_config, port, m_state->state, pkce.challenge));
}

void PoeAuth::cancel()
{
    m_timer.stop();
    m_server.close();
    m_state.reset();
}

void PoeAuth::fail(const QString &text, bool newLinkNeeded)
{
    cancel();
    Q_EMIT failed(text, newLinkNeeded);
}

void PoeAuth::answered(const QUrl &url)
{
    if (!m_state)
        return;
    const RedirectAnswer answer = readRedirect(url, m_state->state);
    switch (answer.kind) {
    case RedirectAnswer::Kind::WrongState:
        fail(u"The answer didn't come from the sign in we started, so it was ignored."_s);
        return;
    case RedirectAnswer::Kind::Denied:
        fail(u"You didn't allow it, so nothing was linked."_s);
        return;
    case RedirectAnswer::Kind::Unreadable:
        fail(u"The answer from the browser couldn't be read. Try linking again."_s);
        return;
    case RedirectAnswer::Kind::Code:
        break;
    }
    // done listening, and the code is only good for 30 seconds
    const Waiting waiting = *m_state;
    cancel();
    takeTokens(post(codeExchangeBody(m_config, answer.code, waiting.verifier, waiting.port)), true);
}

void PoeAuth::refresh(const QString &refreshToken)
{
    takeTokens(post(refreshBody(m_config, refreshToken)), false);
}

QNetworkReply *PoeAuth::post(const QByteArray &body)
{
    QNetworkRequest request = poeRequest(m_tokenUrl, m_config);
    request.setHeader(QNetworkRequest::ContentTypeHeader, u"application/x-www-form-urlencoded"_s);
    return m_network.post(request, body);
}

void PoeAuth::takeTokens(QNetworkReply *reply, bool signIn)
{
    connect(reply, &QNetworkReply::finished, this, [this, reply, signIn] {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->readAll();
        if (status == 0) {
            Q_EMIT failed(u"Couldn't reach the website. Check your connection and try again."_s, false);
            return;
        }
        if (status == 200) {
            if (const auto tokens = readTokens(body)) {
                if (signIn)
                    Q_EMIT linked(*tokens);
                else
                    Q_EMIT refreshed(*tokens);
                return;
            }
            Q_EMIT failed(u"The website's answer couldn't be read. Try again in a minute."_s, false);
            return;
        }
        // a refused code or refresh token says what's wrong itself, and the
        // limits and outages are the API's usual ones
        if (status == 429 || status >= 500) {
            Q_EMIT failed(poeFailure(status, body, QString::fromLatin1(reply->rawHeader("Retry-After"))).text,
                          false);
            return;
        }
        Q_EMIT failed(tokenErrorText(body), needsNewLink(body));
    });
}

} // namespace pou
