// SPDX-License-Identifier: MIT
#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QUrl>
#include <optional>

namespace pou {

// Signing in to a Path of Exile account with the website's OAuth service: the
// app opens the browser, the player allows it there, and the browser hands a
// one-time code back to a port on this computer. Everything here is the part
// that needs no network.

// What the build was given to register with. All three come from CMake
// options, and with any of them missing the feature isn't offered.
struct PoeConfig
{
    QString clientId;
    QString contact;      // for the User-Agent the API asks for
    QList<quint16> ports; // the loopback ports registered as redirects
    // For searching the trade site as the account, once the API has a way to:
    // the scope that allows it and the address its requests go to. Both are
    // empty until Path of Exile publishes them, and nothing uses them then.
    QString tradeScope = {};
    QString tradeUrl = {};

    bool complete() const { return !clientId.isEmpty() && !contact.isEmpty() && !ports.isEmpty(); }
    // Whether trade searches can be made as the account: registered, a scope
    // and an https address to send them to.
    bool tradeComplete() const;
};

// The ports from the CMake option, "49082,49083": comma separated, leaving
// out anything that isn't a port.
QList<quint16> parsePorts(QStringView text);

// The scope the app always asks for.
QString poeScope();
// All of them: that one, and the trade scope when the build has one.
QString poeScopes(const PoeConfig &config);

// The proof key for the code exchange (RFC 7636, S256).
struct Pkce
{
    QString verifier;
    QString challenge;
};
QString pkceChallenge(const QString &verifier);
// From random bytes, so a test can give them. Needs at least 32.
Pkce pkceFrom(const QByteArray &random);
Pkce newPkce();
// Ties the answer to the sign in that was started.
QString newState();

// Where the browser sends the answer, on one of the registered ports.
QUrl redirectUrl(quint16 port);
QUrl authorizeUrl(const PoeConfig &config, quint16 port, const QString &state, const QString &challenge);

// What came back to the redirect.
struct RedirectAnswer
{
    enum class Kind { Code, Denied, WrongState, Unreadable };

    Kind kind = Kind::Unreadable;
    QString code; // for Kind::Code
};
RedirectAnswer readRedirect(const QUrl &url, const QString &state);

// The bodies the token address takes, form encoded.
QByteArray codeExchangeBody(const PoeConfig &config, const QString &code, const QString &verifier,
                            quint16 port);
QByteArray refreshBody(const PoeConfig &config, const QString &refreshToken);

struct PoeTokens
{
    QString access;
    QString refresh;
    int expiresIn = 0; // seconds
};
std::optional<PoeTokens> readTokens(const QByteArray &json);

// Why the token address turned a request down, in words for the player.
QString tokenErrorText(const QByteArray &json);
// The answer for a code or a refresh token that no longer works, so the
// player has to link again.
bool needsNewLink(const QByteArray &json);

} // namespace pou
