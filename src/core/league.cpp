// SPDX-License-Identifier: MIT
#include "core/league.h"

#include <QJsonArray>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

constexpr QStringView standardName = u"Standard";

bool isStandard(const QString &name)
{
    return name.isEmpty() || name == standardName;
}

// Takes a word like "HC" off the front of a league's name, if it's there.
bool takePrefix(QStringView &name, QStringView prefix)
{
    if (!name.startsWith(prefix, Qt::CaseInsensitive))
        return false;
    const QStringView rest = name.sliced(prefix.size());
    if (!rest.isEmpty() && !rest.front().isSpace())
        return false;
    name = rest.trimmed();
    return true;
}

} // namespace

QString League::tradeId() const
{
    if (soloSelfFound)
        return {};
    if (isStandard(name))
        return hardcore ? u"Hardcore"_s : standardName.toString();
    return hardcore ? u"HC "_s + name : name;
}

QString League::displayName() const
{
    QStringList parts;
    if (hardcore)
        parts << u"Hardcore"_s;
    if (soloSelfFound)
        parts << u"SSF"_s;
    // hardcore Standard is just "Hardcore"
    if (!isStandard(name))
        parts << name;
    else if (!hardcore)
        parts << standardName.toString();
    return parts.join(u' ');
}

League leagueFromName(QStringView name)
{
    League league;
    QStringView rest = name.trimmed();
    while (true) {
        if (takePrefix(rest, u"Hardcore") || takePrefix(rest, u"HC"))
            league.hardcore = true;
        else if (takePrefix(rest, u"Solo Self-Found") || takePrefix(rest, u"SSF"))
            league.soloSelfFound = true;
        else
            break;
    }
    league.name = rest.isEmpty() ? standardName.toString() : rest.toString();
    return league;
}

QStringList tradeLeaguesFromJson(const QJsonObject &response)
{
    QStringList ids;
    for (const QJsonValue &v : response.value(u"result").toArray()) {
        const QJsonObject league = v.toObject();
        const QString id = league.value(u"id").toString();
        const QString realm = league.value(u"realm").toString();
        if (!id.isEmpty() && (realm.isEmpty() || realm == u"poe2") && !ids.contains(id))
            ids << id;
    }
    return ids;
}

QStringList leagueNames(const QStringList &tradeLeagues)
{
    QStringList names;
    bool hasStandard = false;
    for (const QString &id : tradeLeagues) {
        const League league = leagueFromName(id);
        if (isStandard(league.name))
            hasStandard = true;
        else if (!names.contains(league.name))
            names << league.name;
    }
    if (hasStandard)
        names << standardName.toString();
    return names;
}

League resolveLeague(const League &wanted, const QStringList &tradeLeagues)
{
    League league = wanted;
    const QStringList names = leagueNames(tradeLeagues);
    if (names.isEmpty()) {
        if (league.name.isEmpty())
            league.name = standardName.toString();
    } else if (!names.contains(league.name)) {
        league.name = names.first();
    }
    return league;
}

} // namespace pou
