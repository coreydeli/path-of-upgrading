// SPDX-License-Identifier: MIT
#include "core/listing.h"

#include "core/numbers.h"
#include "core/parser.h"

#include <QJsonArray>
#include <QRegularExpression>
#include <algorithm>
#include <array>
#include <cmath>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

// The site's mod lists, in the order the game shows them, with the tag the
// game puts after each of their lines.
struct ModGroup
{
    QStringView field;
    QStringView tag;
};

constexpr std::array modGroups = {
    ModGroup{u"enchantMods", u"enchant"},   ModGroup{u"runeMods", u"rune"},
    ModGroup{u"implicitMods", u"implicit"}, ModGroup{u"fracturedMods", u"fractured"},
    ModGroup{u"explicitMods", {}},          ModGroup{u"desecratedMods", u"desecrated"},
    ModGroup{u"craftedMods", u"crafted"},
};

QString rarityOf(const QJsonObject &item)
{
    if (const QString rarity = item.value(u"rarity").toString(); !rarity.isEmpty())
        return rarity;
    // without it, the frame drawn around the item's name says
    static const std::array frames = {u"Normal"_s, u"Magic"_s, u"Rare"_s, u"Unique"_s};
    const int frame = item.value(u"frameType").toInt(-1);
    return frame >= 0 && frame < int(frames.size()) ? frames.at(frame) : u"Rare"_s;
}

// Values come as ["160", 1], the text and how the site styles it.
QString valueText(const QJsonValue &value)
{
    const QJsonValue v = value.isArray() ? value.toArray().at(0) : value;
    return v.isDouble() ? formatNumber(v.toDouble()) : v.toString();
}

// "Energy Shield: 45", or for templates like "Recovers {0} Life over {1}
// Seconds" the template filled in. Properties without values, like the item
// class the site lists first, are left out.
QString propertyLine(const QJsonObject &property)
{
    const QString name = cleanListingText(property.value(u"name").toString());
    QStringList values;
    for (const QJsonValue &v : property.value(u"values").toArray())
        values << cleanListingText(valueText(v));
    const bool hasValues =
        std::any_of(values.cbegin(), values.cend(), [](const QString &v) { return !v.isEmpty(); });
    if (name.isEmpty() || !hasValues)
        return {};

    if (name.contains(u"{0}")) {
        QString line = name;
        for (qsizetype i = 0; i < values.size(); ++i)
            line.replace(u"{%1}"_s.arg(i), values.at(i));
        return line;
    }
    values.removeAll(QString());
    return name + u": "_s + values.join(u", "_s);
}

// Mods come as plain lines, or as the site now sends them, objects with the
// line in "description" next to the affixes it rolled from.
QString modText(const QJsonValue &mod)
{
    if (mod.isObject())
        return cleanListingText(mod.toObject().value(u"description").toString());
    return cleanListingText(mod.toString());
}

// The texts of one of the site's lists of mods.
QStringList modTexts(const QJsonObject &item, QStringView field)
{
    QStringList texts;
    for (const QJsonValue &mod : item.value(field).toArray()) {
        if (const QString text = modText(mod); !text.isEmpty())
            texts << text;
    }
    return texts;
}

// {"type": "~price", "amount": 3, "currency": "divine"}
std::optional<ListingPrice> priceOf(const QJsonObject &price)
{
    const QJsonValue amount = price.value(u"amount");
    const QString currency = price.value(u"currency").toString();
    if (!amount.isDouble() || currency.isEmpty())
        return std::nullopt;
    return ListingPrice{amount.toDouble(), currency};
}

} // namespace

QString cleanListingText(const QString &text)
{
    // [Tag|Shown text], [Shown text] and <style>{shown text}
    static const QRegularExpression taggedLink(uR"(\[([^\]|]+)\|([^\]]+)\])"_s);
    static const QRegularExpression link(uR"(\[([^\]]+)\])"_s);
    static const QRegularExpression styled(uR"(<[^>]*>\{([^}]*)\})"_s);
    static const QRegularExpression spaces(uR"(\s+)"_s);
    return QString(text)
        .replace(taggedLink, u"\\2"_s)
        .replace(link, u"\\1"_s)
        .replace(styled, u"\\1"_s)
        .replace(spaces, u" "_s)
        .trimmed();
}

Item listingToItem(const QJsonObject &result, const QString &itemClass)
{
    const QJsonObject item = result.value(u"item").toObject();

    QStringList header = {u"Item Class: "_s + (itemClass.isEmpty() ? u"Unknown"_s : itemClass),
                          u"Rarity: "_s + rarityOf(item)};
    if (const QString name = cleanListingText(item.value(u"name").toString()); !name.isEmpty())
        header << name;
    // a magic item's type line is its whole name, affixes and all, as in game
    const QString typeLine = item.value(u"typeLine").toString();
    const QString base = cleanListingText(typeLine.isEmpty() ? item.value(u"baseType").toString() : typeLine);
    if (!base.isEmpty())
        header << base;
    QStringList blocks = {header.join(u'\n')};

    QStringList properties;
    for (const QJsonValue &v : item.value(u"properties").toArray()) {
        if (const QString line = propertyLine(v.toObject()); !line.isEmpty())
            properties << line;
    }
    if (!properties.isEmpty())
        blocks << properties.join(u'\n');

    if (const int level = item.value(u"ilvl").toInt(); level > 0)
        blocks << u"Item Level: "_s + QString::number(level);

    for (const ModGroup &group : modGroups) {
        QStringList lines = modTexts(item, group.field);
        if (lines.isEmpty())
            continue;
        if (!group.tag.isEmpty()) {
            for (QString &line : lines)
                line = u"%1 (%2)"_s.arg(line, group.tag);
        }
        blocks << lines.join(u'\n');
    }

    if (item.value(u"corrupted").toBool())
        blocks << u"Corrupted"_s;
    if (const QJsonValue identified = item.value(u"identified"); identified.isBool() && !identified.toBool())
        blocks << u"Unidentified"_s;

    // it reads as item text whatever the listing had, as it starts with the class
    return parseItem(blocks.join(u"\n--------\n"_s)).value_or(Item{});
}

std::optional<double> priceInExalts(const ListingPrice &price, const CurrencyRates &rates)
{
    const QString currency = price.currency.toLower();
    double rate = rates.value(currency);
    if (!(rate > 0) && currency == u"exalted")
        rate = 1;
    if (!(rate > 0) || !std::isfinite(price.amount))
        return std::nullopt;
    return price.amount * rate;
}

QString formatPrice(const std::optional<ListingPrice> &price)
{
    if (!price)
        return u"no price"_s;
    QString currency = price->currency;
    if (currency == u"divine")
        currency = u"div"_s;
    else if (currency == u"exalted")
        currency = u"ex"_s;
    return formatNumber(price->amount) + u' ' + currency;
}

QString formatConvertedPrice(const ListingPrice &price, double exalts, double divineRate)
{
    if (!std::isfinite(exalts))
        return {};
    const QString currency = price.currency.toLower();
    if (currency != u"divine" && divineRate > 0 && exalts >= divineRate)
        return u"about %1 div"_s.arg(formatNumber(roundTo(exalts / divineRate, 1)));
    if (currency == u"exalted")
        return {};
    return u"about %1 ex"_s.arg(formatNumber(roundTo(exalts, exalts < 10 ? 1 : 0)));
}

QString listedAgo(const QDateTime &indexed, const QDateTime &now)
{
    if (!indexed.isValid() || !now.isValid())
        return {};
    const qint64 minutes = indexed.secsTo(now) / 60;
    if (minutes < 1)
        return u"just now"_s;
    const auto ago = [](qint64 n, const QString &unit) {
        return u"%1 %2%3 ago"_s.arg(n).arg(unit, n == 1 ? QString() : u"s"_s);
    };
    if (minutes < 60)
        return ago(minutes, u"minute"_s);
    if (minutes < 24 * 60)
        return ago(minutes / 60, u"hour"_s);
    return ago(minutes / (24 * 60), u"day"_s);
}

Listing readListing(const QJsonObject &result, const QString &itemClass, const CurrencyRates &rates)
{
    const QJsonObject listing = result.value(u"listing").toObject();
    const QJsonObject account = listing.value(u"account").toObject();

    Listing out;
    out.id = result.value(u"id").toString();
    out.item = listingToItem(result, itemClass);
    const QJsonObject item = result.value(u"item").toObject();
    for (const ModGroup &group : modGroups) {
        for (const QString &text : modTexts(item, group.field))
            out.mods << ListingMod{text, group.tag.isEmpty() ? u"explicit"_s : group.tag.toString()};
    }
    out.price = priceOf(listing.value(u"price").toObject());
    if (out.price)
        out.exalts = priceInExalts(*out.price, rates);

    out.account = account.value(u"name").toString();
    out.seller = account.value(u"lastCharacterName").toString();
    if (out.seller.isEmpty())
        out.seller = out.account;
    // where they play while online, null while not
    const QJsonValue online = account.value(u"online");
    out.online = online.isObject() || online.toBool();

    const QString whisper = listing.value(u"whisper").toString();
    out.instantBuyout = whisper.isEmpty();
    // one with placeholders is a template the site fills in itself
    static const QRegularExpression placeholder(uR"(\{\d\})"_s);
    if (!placeholder.match(whisper).hasMatch())
        out.whisper = whisper;
    out.hideoutToken = listing.value(u"hideout_token").toString();
    out.indexed = QDateTime::fromString(listing.value(u"indexed").toString(), Qt::ISODate);
    return out;
}

QList<Listing> listingsFromJson(const QJsonObject &response, const QString &itemClass,
                                const CurrencyRates &rates)
{
    QList<Listing> listings;
    for (const QJsonValue &result : response.value(u"result").toArray()) {
        if (result.isObject())
            listings << readListing(result.toObject(), itemClass, rates);
    }
    return listings;
}

} // namespace pou
