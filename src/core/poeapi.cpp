// SPDX-License-Identifier: MIT
#include "core/poeapi.h"

#include <QJsonArray>
#include <QJsonDocument>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

// The API's own error codes, from its documentation.
constexpr int rateLimitedCode = 3;
constexpr int unavailableCode = 7;

QString secondsText(int seconds)
{
    return seconds == 1 ? u"1 second"_s : u"%1 seconds"_s.arg(seconds);
}

} // namespace

QUrl charactersUrl()
{
    return QUrl(u"https://api.pathofexile.com/character/poe2"_s);
}

QUrl characterUrl(const QString &name)
{
    return QUrl(charactersUrl().toString() + u'/' + QString::fromLatin1(QUrl::toPercentEncoding(name)));
}

QList<AccountCharacter> readCharacterList(const QByteArray &json)
{
    QList<AccountCharacter> list;
    const QJsonArray characters = QJsonDocument::fromJson(json).object().value(u"characters"_s).toArray();
    for (const QJsonValue &value : characters) {
        const QJsonObject object = value.toObject();
        if (object.value(u"deleted"_s).toBool() || object.value(u"expired"_s).toBool())
            continue;
        AccountCharacter character{.id = object.value(u"id"_s).toString(),
                                   .name = object.value(u"name"_s).toString(),
                                   .className = object.value(u"class"_s).toString(),
                                   .league = object.value(u"league"_s).toString(),
                                   .level = object.value(u"level"_s).toInt(),
                                   .current = object.value(u"current"_s).toBool()};
        if (!character.name.isEmpty())
            list << character;
    }
    return list;
}

std::optional<QJsonObject> readCharacter(const QByteArray &json)
{
    const QJsonObject character = QJsonDocument::fromJson(json).object().value(u"character"_s).toObject();
    if (character.value(u"name"_s).toString().isEmpty())
        return std::nullopt;
    return character;
}

int readRetryAfter(QStringView header)
{
    bool ok = false;
    const int seconds = header.trimmed().toInt(&ok);
    return ok && seconds > 0 ? seconds : 0;
}

PoeFailure poeFailure(int status, const QByteArray &body, QStringView retryAfter)
{
    using Kind = PoeFailure::Kind;
    const int code =
        QJsonDocument::fromJson(body).object().value(u"error"_s).toObject().value(u"code"_s).toInt(-1);

    if (status == 429 || code == rateLimitedCode) {
        const int wait = readRetryAfter(retryAfter);
        return {.kind = Kind::RateLimited,
                .waitSeconds = wait,
                .text = wait > 0
                            ? u"The website says to wait %1 before asking again."_s.arg(secondsText(wait))
                            : u"The website says to wait before asking again."_s};
    }
    if (status == 401)
        return {.kind = Kind::SignedOut, .text = u"Your link ran out. Link your account again."_s};
    if (status == 403)
        return {.kind = Kind::Refused,
                .text = u"The website won't let this app read your characters. Link your account again."_s};
    if (status == 404)
        return {.kind = Kind::NotFound, .text = u"That character isn't on your account any more."_s};
    if (status >= 500 || code == unavailableCode)
        return {.kind = Kind::Unavailable,
                .text = u"The website isn't answering properly right now. Try again in a minute."_s};
    return {.kind = Kind::Other,
            .text = u"The website sent back something unexpected. Try again in a minute."_s};
}

} // namespace pou
