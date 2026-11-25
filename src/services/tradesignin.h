// SPDX-License-Identifier: MIT
#pragma once

#include <QByteArray>
#include <QNetworkRequest>
#include <QString>
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

} // namespace pou
