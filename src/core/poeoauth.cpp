// SPDX-License-Identifier: MIT
#include "core/poeoauth.h"

#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QUrlQuery>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

QString base64Url(const QByteArray &bytes)
{
    return QString::fromLatin1(
        bytes.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

QByteArray randomBytes(qsizetype count)
{
    QByteArray bytes(count, Qt::Uninitialized);
    QRandomGenerator::system()->fillRange(reinterpret_cast<quint32 *>(bytes.data()), count / 4);
    return bytes;
}

QByteArray form(const QList<std::pair<QString, QString>> &fields)
{
    QUrlQuery query;
    for (const auto &[name, value] : fields)
        query.addQueryItem(name, value);
    return query.query(QUrl::FullyEncoded).toUtf8();
}

QString errorCode(const QByteArray &json)
{
    return QJsonDocument::fromJson(json).object().value(u"error"_s).toString();
}

} // namespace

QList<quint16> parsePorts(QStringView text)
{
    QList<quint16> ports;
    for (QStringView part : text.split(u',', Qt::SkipEmptyParts)) {
        bool ok = false;
        const uint port = part.trimmed().toUInt(&ok);
        if (ok && port >= 1024 && port <= 65535 && !ports.contains(quint16(port)))
            ports << quint16(port);
    }
    return ports;
}

QString poeScope()
{
    return u"account:characters"_s;
}

QString poeScopes(const PoeConfig &config)
{
    const QString trade = config.tradeScope.trimmed();
    return trade.isEmpty() ? poeScope() : poeScope() + u' ' + trade;
}

bool PoeConfig::tradeComplete() const
{
    const QUrl url(tradeUrl);
    return complete() && !tradeScope.trimmed().isEmpty() && url.isValid() && url.scheme() == u"https" &&
           !url.host().isEmpty();
}

QString pkceChallenge(const QString &verifier)
{
    return base64Url(QCryptographicHash::hash(verifier.toLatin1(), QCryptographicHash::Sha256));
}

Pkce pkceFrom(const QByteArray &random)
{
    const QString verifier = base64Url(random);
    return {.verifier = verifier, .challenge = pkceChallenge(verifier)};
}

Pkce newPkce()
{
    return pkceFrom(randomBytes(48));
}

QString newState()
{
    return base64Url(randomBytes(24));
}

QUrl redirectUrl(quint16 port)
{
    return QUrl(u"http://127.0.0.1:%1/callback"_s.arg(port));
}

QUrl authorizeUrl(const PoeConfig &config, quint16 port, const QString &state, const QString &challenge)
{
    QUrl url(u"https://www.pathofexile.com/oauth/authorize"_s);
    QUrlQuery query;
    query.addQueryItem(u"client_id"_s, config.clientId);
    query.addQueryItem(u"response_type"_s, u"code"_s);
    query.addQueryItem(u"scope"_s, poeScopes(config));
    query.addQueryItem(u"state"_s, state);
    query.addQueryItem(u"redirect_uri"_s, redirectUrl(port).toString());
    query.addQueryItem(u"code_challenge"_s, challenge);
    query.addQueryItem(u"code_challenge_method"_s, u"S256"_s);
    url.setQuery(query);
    return url;
}

RedirectAnswer readRedirect(const QUrl &url, const QString &state)
{
    const QUrlQuery query(url);
    // a different state is checked first, as an answer that isn't ours says
    // nothing we should act on, a denial included
    if (!query.hasQueryItem(u"state"_s) || query.queryItemValue(u"state"_s) != state)
        return {.kind = RedirectAnswer::Kind::WrongState};
    if (query.queryItemValue(u"error"_s) == u"access_denied"_s)
        return {.kind = RedirectAnswer::Kind::Denied};
    const QString code = query.queryItemValue(u"code"_s);
    if (code.isEmpty())
        return {};
    return {.kind = RedirectAnswer::Kind::Code, .code = code};
}

QByteArray codeExchangeBody(const PoeConfig &config, const QString &code, const QString &verifier,
                            quint16 port)
{
    return form({{u"client_id"_s, config.clientId},
                 {u"grant_type"_s, u"authorization_code"_s},
                 {u"code"_s, code},
                 {u"redirect_uri"_s, redirectUrl(port).toString()},
                 {u"scope"_s, poeScopes(config)},
                 {u"code_verifier"_s, verifier}});
}

QByteArray refreshBody(const PoeConfig &config, const QString &refreshToken)
{
    return form({{u"client_id"_s, config.clientId},
                 {u"grant_type"_s, u"refresh_token"_s},
                 {u"refresh_token"_s, refreshToken}});
}

std::optional<PoeTokens> readTokens(const QByteArray &json)
{
    const QJsonObject object = QJsonDocument::fromJson(json).object();
    PoeTokens tokens{.access = object.value(u"access_token"_s).toString(),
                     .refresh = object.value(u"refresh_token"_s).toString(),
                     .expiresIn = object.value(u"expires_in"_s).toInt()};
    if (tokens.access.isEmpty() || tokens.refresh.isEmpty() || tokens.expiresIn <= 0)
        return std::nullopt;
    return tokens;
}

bool needsNewLink(const QByteArray &json)
{
    const QString code = errorCode(json);
    return code == u"invalid_grant"_s || code == u"invalid_token"_s;
}

QString tokenErrorText(const QByteArray &json)
{
    if (needsNewLink(json))
        return u"Your link ran out. Link your account again."_s;
    if (errorCode(json) == u"access_denied"_s)
        return u"You didn't allow it, so nothing was linked."_s;
    return u"The website turned the sign in down. Try linking again."_s;
}

} // namespace pou
