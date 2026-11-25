// SPDX-License-Identifier: MIT
#pragma once

#include <QByteArray>
#include <QNetworkRequest>
#include <QString>
#include <QUrl>
#include <functional>

class QObject;

namespace pou {

// How requests to the trade site say who's sending them. The client is
// handed one, and asks it to sign each request just before it goes out.
class TradeSignIn
{
public:
    // The request, signed, or why it can't be. Called once.
    using Done = std::function<void(const QNetworkRequest &request, const QString &error)>;

    virtual ~TradeSignIn() = default;

    // Calls done now or later, but not at all if the context is gone by then.
    virtual void sign(const QNetworkRequest &request, const QObject *context, Done done) = 0;
    // The site said it doesn't know who's asking. True when signing the same
    // request again could be different, which the client does once.
    virtual bool refused() { return false; }
};

// The POESESSID cookie of a browser that's signed in to the site.
class SessionCookieSignIn : public TradeSignIn
{
public:
    // The id has to be one, see TradeClient::isSessionId().
    explicit SessionCookieSignIn(const QByteArray &id)
        : m_id(id)
    {}

    void sign(const QNetworkRequest &request, const QObject *context, Done done) override;

private:
    QByteArray m_id;
};

class PoeTokenSource;

// The linked account's token, sent to the API address the build was given in
// place of the site's own. A request that wouldn't go to that address is
// turned down, so the token can't leave for any other host, and a redirect to
// another host doesn't get it.
class BearerSignIn : public TradeSignIn
{
public:
    // The tokens have to outlive it, and the base is PoeConfig::tradeUrl.
    BearerSignIn(PoeTokenSource *tokens, const QUrl &base)
        : m_tokens(tokens)
        , m_base(base)
    {}

    void sign(const QNetworkRequest &request, const QObject *context, Done done) override;
    // The token was no good, so the next is a new one.
    bool refused() override;

private:
    PoeTokenSource *m_tokens;
    QUrl m_base;
};

} // namespace pou
