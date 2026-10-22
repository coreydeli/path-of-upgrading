// SPDX-License-Identifier: MIT
// Small stand-ins for the trade site's data endpoints, shared by several tests.
#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

using namespace Qt::Literals::StringLiterals;

namespace tradefixtures {

inline QJsonObject stat(const QString &id, const QString &text, const QString &type)
{
    return {{u"id"_s, id}, {u"text"_s, text}, {u"type"_s, type}};
}

inline QJsonObject group(const QString &id, const QJsonArray &entries)
{
    return {{u"id"_s, id}, {u"entries"_s, entries}};
}

inline QJsonObject response(const QJsonArray &groups)
{
    return {{u"result"_s, groups}};
}

// Shaped like /api/trade2/data/stats
inline QJsonObject stats()
{
    return response({
        group(u"pseudo"_s,
              {
                  stat(u"pseudo.pseudo_total_life"_s, u"+# total maximum Life"_s, u"pseudo"_s),
                  stat(u"pseudo.pseudo_total_fire_resistance"_s, u"+#% total to Fire Resistance"_s,
                       u"pseudo"_s),
              }),
        group(u"explicit"_s,
              {
                  stat(u"explicit.stat_life"_s, u"+# to maximum Life"_s, u"explicit"_s),
                  stat(u"explicit.stat_fire"_s, u"+#% to Fire Resistance"_s, u"explicit"_s),
                  stat(u"explicit.stat_armour_local"_s, u"#% increased Armour (Local)"_s, u"explicit"_s),
                  stat(u"explicit.stat_armour"_s, u"#% increased Armour"_s, u"explicit"_s),
                  stat(u"explicit.stat_ms"_s, u"#% increased Movement Speed"_s, u"explicit"_s),
              }),
    });
}

inline QJsonObject categoryOption(const QJsonValue &id, const QString &text)
{
    return {{u"id"_s, id}, {u"text"_s, text}};
}

// Shaped like /api/trade2/data/filters
inline QJsonObject filters()
{
    const QJsonArray options = {
        categoryOption(QJsonValue::Null, u"Any"_s),
        categoryOption(u"armour.helmet"_s, u"Helmet"_s),
        categoryOption(u"weapon.twomace"_s, u"Two Hand Mace"_s),
        categoryOption(u"weapon.warstaff"_s, u"Quarterstaff"_s),
    };
    const QJsonObject category = {{u"id"_s, u"category"_s},
                                  {u"option"_s, QJsonObject{{u"options"_s, options}}}};
    return response({QJsonObject{{u"id"_s, u"type_filters"_s}, {u"filters"_s, QJsonArray{category}}}});
}

} // namespace tradefixtures
