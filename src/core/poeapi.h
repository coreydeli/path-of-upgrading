// SPDX-License-Identifier: MIT
#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QUrl>
#include <optional>

namespace pou {

// Path of Exile's account API, for the characters of a linked account. This is
// the reading of its addresses and answers, with no network.

QUrl charactersUrl();
QUrl characterUrl(const QString &name);

// One character in the account's list.
struct AccountCharacter
{
    QString id;
    QString name;
    QString className;
    QString league;
    int level = 0;
    bool current = false;
};

// The list, leaving out characters that were deleted or have expired, and
// ones without a name.
QList<AccountCharacter> readCharacterList(const QByteArray &json);

// One character in full, with its equipment, skills and passives, as the
// object the API wraps it in.
std::optional<QJsonObject> readCharacter(const QByteArray &json);

// Why a request failed.
struct PoeFailure
{
    enum class Kind {
        SignedOut, // the token is no good, so refresh it or link again
        Refused,   // the account didn't give the app this
        NotFound,
        RateLimited,
        Unavailable, // the website's side, try later
        Other,
    };

    Kind kind = Kind::Other;
    int waitSeconds = 0; // how long to leave it, for RateLimited
    QString text;        // for the player
};

// Retry-After's seconds, or zero when it isn't there or isn't a number of
// seconds.
int readRetryAfter(QStringView header);

// From the HTTP status and body, with the Retry-After header's text.
PoeFailure poeFailure(int status, const QByteArray &body, QStringView retryAfter);

} // namespace pou
