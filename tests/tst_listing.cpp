// SPDX-License-Identifier: MIT
#include "core/listing.h"

#include <QJsonArray>
#include <QTest>
#include <QTimeZone>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

// a value as the site sends it: the text and how it's styled
QJsonObject itemProperty(const QString &name, const QStringList &values)
{
    QJsonArray shown;
    for (const QString &v : values)
        shown.append(QJsonArray{v, 1});
    return {{u"name"_s, name}, {u"values"_s, shown}};
}

QJsonObject price(double amount, const QString &currency)
{
    return {{u"type"_s, u"~price"_s}, {u"amount"_s, amount}, {u"currency"_s, currency}};
}

QJsonObject result(const QString &id, const QJsonObject &listing, const QJsonObject &item)
{
    return {{u"id"_s, id}, {u"listing"_s, listing}, {u"item"_s, item}};
}

QJsonObject rareItem(const QString &name, const QString &base, int itemLevel)
{
    return {{u"rarity"_s, u"Rare"_s}, {u"name"_s, name}, {u"typeLine"_s, base}, {u"ilvl"_s, itemLevel}};
}

// Mods as plain lines, as the site used to send them.
QJsonObject gloves()
{
    QJsonObject item = rareItem(u"Grim Grasp"_s, u"Sorcerer Gloves"_s, 81);
    item.insert(u"properties"_s, QJsonArray{itemProperty(u"Quality"_s, {u"+20%"_s}),
                                            itemProperty(u"[EnergyShield|Energy Shield]"_s, {u"250"_s})});
    item.insert(u"runeMods"_s, QJsonArray{u"+12% to [Resistances|Cold Resistance]"_s});
    item.insert(u"explicitMods"_s,
                QJsonArray{u"+80 to maximum [Life|Life]"_s, u"+40% to [Fire|Fire] [Resistances|Resistance]"_s,
                           u"52% increased [EnergyShield|Energy Shield]"_s});

    const QJsonObject account{{u"name"_s, u"SellerAcc"_s},
                              {u"lastCharacterName"_s, u"SellerChar"_s},
                              {u"online"_s, QJsonObject{{u"league"_s, u"Forbidden Rites"_s}}}};
    const QJsonObject listing{{u"indexed"_s, u"2026-10-20T08:00:00Z"_s},
                              {u"whisper"_s, u"@SellerChar Hi, I would like to buy your Grim Grasp Sorcerer "
                                             "Gloves listed for 3 divine in Forbidden Rites"_s},
                              {u"account"_s, account},
                              {u"price"_s, price(3, u"divine"_s)}};
    return result(u"abc123"_s, listing, item);
}

// Shaped like what the site sent in October 2026: the item class first as a
// property without values, and mods as objects. The seller is made up.
QJsonObject cuffs()
{
    QJsonArray mods;
    for (const QString &description :
         {u"80% increased [Armour|Armour] and [EnergyShield|Energy Shield]"_s,
          u"Adds 4 to 10 [Physical|Physical] Damage to [Attack|Attacks]"_s,
          u"Adds 1 to 28 [Lightning] damage to [Attack|Attacks]"_s,
          u"17% increased [ItemRarity|Rarity of Items] found"_s, u"Gain 9 Life per enemy killed"_s}) {
        const QJsonObject affix{{u"name"_s, u"Sparking"_s}, {u"tier"_s, u"P5"_s}, {u"level"_s, 60}};
        mods.append(QJsonObject{{u"description"_s, description},
                                {u"domain"_s, 1},
                                {u"hash"_s, u"stat.explicit.stat_3321629045"_s},
                                {u"mods"_s, QJsonArray{affix}}});
    }
    QJsonObject item = rareItem(u"Rift Talons"_s, u"Bound Cuffs"_s, 80);
    item.insert(u"baseType"_s, u"Bound Cuffs"_s);
    item.insert(u"frameType"_s, 2);
    item.insert(u"identified"_s, true);
    item.insert(u"properties"_s,
                QJsonArray{itemProperty(u"Gloves"_s, {}), itemProperty(u"[Armour]"_s, {u"160"_s}),
                           itemProperty(u"[EnergyShield|Energy Shield]"_s, {u"45"_s})});
    item.insert(u"requirements"_s, QJsonArray{itemProperty(u"Level"_s, {u"65"_s})});
    item.insert(u"explicitMods"_s, mods);

    const QJsonObject account{{u"name"_s, u"Hollowmere#0412"_s},
                              {u"lastCharacterName"_s, u"AshenTollkeeper"_s},
                              {u"online"_s, QJsonObject{{u"league"_s, u"Standard"_s}}},
                              {u"language"_s, u"en_US"_s},
                              {u"realm"_s, u"poe2"_s}};
    const QJsonObject listing{
        {u"method"_s, u"psapi"_s},
        {u"indexed"_s, u"2024-12-30T11:58:13Z"_s},
        {u"stash"_s, QJsonObject{{u"name"_s, u"~price 1 exalted"_s}, {u"x"_s, 12}, {u"y"_s, 19}}},
        {u"whisper"_s, u"@AshenTollkeeper Hi, I would like to buy your Rift Talons Bound Cuffs listed for 1 "
                       "exalted in Standard (stash tab \"~price 1 exalted\"; position: left 12, top 19)"_s},
        {u"account"_s, account},
        {u"price"_s, price(1, u"exalted"_s)}};
    return result(u"f00d"_s, listing, item);
}

const ItemLine *lineWith(const Item &item, QStringView text)
{
    for (const ItemLine &line : item.lines) {
        if (line.text.contains(text))
            return &line;
    }
    return nullptr;
}

const CurrencyRates rates = {{u"divine"_s, 700}, {u"exalted"_s, 1}, {u"chaos"_s, 10}};

} // namespace

class TestListing : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void readsTheItem()
    {
        const Item item = listingToItem(gloves(), u"Gloves"_s);
        QCOMPARE(item.slot, u"Gloves"_s);
        QCOMPARE(item.name, u"Grim Grasp"_s);
        QCOMPARE(item.baseType, u"Sorcerer Gloves"_s);
        QCOMPARE(item.itemLevel, 81);
        QCOMPARE(item.stats.value(u"[Base] Energy Shield"_s), 250.0);
        QCOMPARE(item.stats.value(u"# to maximum Life"_s), 80.0);
        QCOMPARE(item.stats.value(u"#% to Fire Resistance"_s), 40.0);
        QCOMPARE(item.stats.value(u"#% to Cold Resistance"_s), 12.0);
        const ItemLine *cold = lineWith(item, u"Cold");
        QVERIFY(cold);
        QCOMPARE(cold->tag, u"rune"_s);
    }

    void summary()
    {
        const Listing listing = readListing(gloves(), u"Gloves"_s, rates);
        QCOMPARE(listing.id, u"abc123"_s);
        QCOMPARE(formatPrice(listing.price), u"3 div"_s);
        QCOMPARE(listing.exalts.value_or(0), 2100.0);
        QCOMPARE(listing.seller, u"SellerChar"_s);
        QCOMPARE(listing.account, u"SellerAcc"_s);
        QVERIFY(listing.online);
        QVERIFY(!listing.instantBuyout);
        QVERIFY(listing.whisper.startsWith(u"@SellerChar Hi"));
        QCOMPARE(listing.item.name, u"Grim Grasp"_s);
    }

    void modLines()
    {
        const Listing listing = readListing(gloves(), u"Gloves"_s, rates);
        const QList<ListingMod> expected = {
            {u"+12% to Cold Resistance"_s, u"rune"_s},
            {u"+80 to maximum Life"_s, u"explicit"_s},
            {u"+40% to Fire Resistance"_s, u"explicit"_s},
            {u"52% increased Energy Shield"_s, u"explicit"_s},
        };
        QCOMPARE(listing.mods, expected);
    }

    // in the game's order whatever order the site lists them, and with the
    // mods the item doesn't keep
    void modLinesInOrder()
    {
        QJsonObject item = rareItem(u"Empyrean Knot"_s, u"Iron Ring"_s, 82);
        item.insert(u"explicitMods"_s, QJsonArray{u"+56 to maximum [Mana|Mana]"_s, u"Cannot be Frozen"_s,
                                                  u"Gain 3 Life per enemy killed"_s,
                                                  u"+5% to all Elemental "
                                                  "Resistances"_s,
                                                  u"+5 to Strength"_s, u"+5 to Dexterity"_s, u"Wild"_s});
        item.insert(u"craftedMods"_s, QJsonArray{QJsonObject{{u"description"_s, u"Crafted line"_s}}});
        item.insert(u"enchantMods"_s, QJsonArray{u"Enchant line"_s, QString()});
        item.insert(u"desecratedMods"_s, QJsonArray{u"Desecrated line"_s});
        item.insert(u"fracturedMods"_s, QJsonArray{u"Fractured line"_s});
        item.insert(u"implicitMods"_s, QJsonArray{u"Implicit line"_s});
        const QJsonObject listing{{u"price"_s, price(1, u"divine"_s)}};

        const Listing l = readListing(result(u"x"_s, listing, item), u"Rings"_s, rates);
        QStringList tags;
        for (const ListingMod &mod : l.mods) {
            if (!tags.contains(mod.tag))
                tags << mod.tag;
        }
        QCOMPARE(tags, (QStringList{u"enchant"_s, u"implicit"_s, u"fractured"_s, u"explicit"_s,
                                    u"desecrated"_s, u"crafted"_s}));
        QCOMPARE(l.mods.size(), 12);
        QCOMPARE(l.mods.at(0).text, u"Enchant line"_s);
        // the ones without numbers are here, and in order
        QVERIFY(l.mods.contains({u"Cannot be Frozen"_s, u"explicit"_s}));
        QCOMPARE(l.mods.at(3).text, u"+56 to maximum Mana"_s);
        QCOMPARE(l.mods.at(9).text, u"Wild"_s);
        QCOMPARE(l.mods.last(), (ListingMod{u"Crafted line"_s, u"crafted"_s}));
        // and a listing without mods has none
        QVERIFY(
            readListing(result(u"y"_s, listing, rareItem(u"Plain"_s, u"Iron Ring"_s, 1)), u"Rings"_s, rates)
                .mods.isEmpty());
    }

    void instantBuyout()
    {
        QJsonObject r = gloves();
        QJsonObject listing = r.value(u"listing"_s).toObject();
        listing.remove(u"whisper"_s);
        listing.insert(u"hideout_token"_s, u"token"_s);
        listing.insert(u"price"_s, price(50, u"exalted"_s));
        r.insert(u"listing"_s, listing);

        const Listing merchant = readListing(r, u"Gloves"_s, rates);
        QVERIFY(merchant.instantBuyout);
        QVERIFY(merchant.whisper.isEmpty());
        QCOMPARE(merchant.hideoutToken, u"token"_s);
        QCOMPARE(merchant.exalts.value_or(0), 50.0);
    }

    void whisperTemplates()
    {
        QJsonObject r = gloves();
        QJsonObject listing = r.value(u"listing"_s).toObject();
        listing.insert(u"whisper"_s, u"@{0} Hi, I'd like to buy your {1}"_s);
        r.insert(u"listing"_s, listing);

        const Listing l = readListing(r, u"Gloves"_s, rates);
        QVERIFY(l.whisper.isEmpty());
        QVERIFY(!l.instantBuyout);
    }

    void sellerWithoutCharacter()
    {
        QJsonObject r = gloves();
        QJsonObject listing = r.value(u"listing"_s).toObject();
        listing.insert(u"account"_s, QJsonObject{{u"name"_s, u"SellerAcc"_s}, {u"online"_s, QJsonValue()}});
        r.insert(u"listing"_s, listing);

        const Listing l = readListing(r, u"Gloves"_s, rates);
        QCOMPARE(l.seller, u"SellerAcc"_s);
        QVERIFY(!l.online);
    }

    void noPrice()
    {
        QJsonObject r = gloves();
        QJsonObject listing = r.value(u"listing"_s).toObject();
        listing.remove(u"price"_s);
        r.insert(u"listing"_s, listing);

        const Listing l = readListing(r, u"Gloves"_s, rates);
        QVERIFY(!l.price);
        QVERIFY(!l.exalts);
        QCOMPARE(formatPrice(l.price), u"no price"_s);
    }

    void modsAsObjects()
    {
        QJsonObject item = rareItem(u"Empyrean Knot"_s, u"Iron Ring"_s, 82);
        item.insert(
            u"implicitMods"_s,
            QJsonArray{QJsonObject{{u"description"_s, u"Adds 1 to 4 [Physical|Physical] Damage to Attacks"_s},
                                   {u"hash"_s, u"implicit.stat_3032590688"_s}}});
        item.insert(
            u"explicitMods"_s,
            QJsonArray{QJsonObject{{u"description"_s, u"+56 to maximum [Mana|Mana]"_s}},
                       QJsonObject{{u"description"_s, u"Minions deal 21% increased Damage if you've Hit "
                                                      "Recently"_s}}});
        const QJsonObject listing{{u"price"_s, price(1, u"divine"_s)},
                                  {u"account"_s, QJsonObject{{u"name"_s, u"A"_s}}}};

        const Listing l = readListing(result(u"x"_s, listing, item), u"Rings"_s, {{u"divine"_s, 700}});
        QCOMPARE(l.item.stats.value(u"# to maximum Mana"_s), 56.0);
        const ItemLine *implicit = lineWith(l.item, u"Adds 1 to 4 Physical Damage to Attacks");
        QVERIFY(implicit);
        QCOMPARE(implicit->tag, u"implicit"_s);
        QVERIFY(!l.item.raw.contains(u"description"));
    }

    void asTheSiteSendsIt()
    {
        const Listing l = readListing(cuffs(), u"Gloves"_s, rates);
        QCOMPARE(l.item.raw, u"Item Class: Gloves\n"
                             "Rarity: Rare\n"
                             "Rift Talons\n"
                             "Bound Cuffs\n"
                             "--------\n"
                             "Armour: 160\n"
                             "Energy Shield: 45\n"
                             "--------\n"
                             "Item Level: 80\n"
                             "--------\n"
                             "80% increased Armour and Energy Shield\n"
                             "Adds 4 to 10 Physical Damage to Attacks\n"
                             "Adds 1 to 28 Lightning damage to Attacks\n"
                             "17% increased Rarity of Items found\n"
                             "Gain 9 Life per enemy killed"_s);
        QCOMPARE(l.item.stats.value(u"[Base] Armour"_s), 160.0);
        QCOMPARE(l.item.stats.value(u"[Base] Energy Shield"_s), 45.0);
        QCOMPARE(l.item.stats.value(u"#% increased Rarity of Items found"_s), 17.0);
        QCOMPARE(formatPrice(l.price), u"1 ex"_s);
        QCOMPARE(l.exalts.value_or(0), 1.0);
        QCOMPARE(l.seller, u"AshenTollkeeper"_s);
        QCOMPARE(l.account, u"Hollowmere#0412"_s);
        QVERIFY(l.online);
        QVERIFY(l.whisper.endsWith(u"position: left 12, top 19)"));
        QCOMPARE(l.indexed, QDateTime(QDate(2024, 12, 30), QTime(11, 58, 13), QTimeZone::UTC));
    }

    void templatedProperties()
    {
        QJsonObject item = rareItem({}, u"Ultimate Life Flask"_s, 60);
        item.insert(u"rarity"_s, u"Normal"_s);
        item.insert(u"properties"_s,
                    QJsonArray{itemProperty(u"Recovers {0} Life over {1} Seconds"_s, {u"1120"_s, u"3"_s}),
                               itemProperty(u"Consumes {0} of {1} Charges on use"_s, {u"10"_s, u"65"_s})});
        const Item flask = listingToItem(result(u"x"_s, {}, item), u"Life Flasks"_s);
        QVERIFY(flask.raw.contains(u"\nRecovers 1120 Life over 3 Seconds\n"));
        QVERIFY(flask.raw.contains(u"\nConsumes 10 of 65 Charges on use\n"));
        QCOMPARE(flask.slot, u"Life Flask"_s);
    }

    void rarityFromTheFrame()
    {
        const auto rarityFor = [](const QJsonValue &frame) {
            QJsonObject item{{u"name"_s, u"Some Name"_s}, {u"typeLine"_s, u"Iron Ring"_s}};
            if (!frame.isUndefined())
                item.insert(u"frameType"_s, frame);
            return listingToItem(result(u"x"_s, {}, item), u"Rings"_s).rarity;
        };
        QCOMPARE(rarityFor(0), u"Normal"_s);
        QCOMPARE(rarityFor(1), u"Magic"_s);
        QCOMPARE(rarityFor(2), u"Rare"_s);
        QCOMPARE(rarityFor(3), u"Unique"_s);
        QCOMPARE(rarityFor(9), u"Rare"_s);
        QCOMPARE(rarityFor(QJsonValue::Undefined), u"Rare"_s);
    }

    // named with its affixes, as the game writes it
    void magicItems()
    {
        const QJsonObject item{{u"rarity"_s, u"Magic"_s},
                               {u"name"_s, QString()},
                               {u"typeLine"_s, u"Hale Gold Ring of the Whelpling"_s},
                               {u"baseType"_s, u"Gold Ring"_s}};
        const Item ring = listingToItem(result(u"x"_s, {}, item), u"Rings"_s);
        QCOMPARE(ring.name, u"Hale Gold Ring of the Whelpling"_s);
        QVERIFY(ring.baseType.isEmpty());
    }

    void unidentifiedAndCorrupted()
    {
        QJsonObject item = rareItem({}, u"Sorcerer Gloves"_s, 81);
        item.insert(u"identified"_s, false);
        item.insert(u"corrupted"_s, true);
        const Item unknown = listingToItem(result(u"x"_s, {}, item), u"Gloves"_s);
        QVERIFY(unknown.unidentified);
        QVERIFY(unknown.corrupted);

        item.insert(u"identified"_s, true);
        item.remove(u"corrupted"_s);
        const Item known = listingToItem(result(u"x"_s, {}, item), u"Gloves"_s);
        QVERIFY(!known.unidentified);
        QVERIFY(!known.corrupted);
    }

    void cleaning_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<QString>("expected");

        QTest::newRow("tagged") << u"+39% to [Resistances|Lightning Resistance]"_s
                                << u"+39% to Lightning Resistance"_s;
        QTest::newRow("two") << u"+40% to [Fire|Fire] [Resistances|Resistance]"_s
                             << u"+40% to Fire Resistance"_s;
        QTest::newRow("plain link") << u"Adds 1 to 28 [Lightning] damage"_s
                                    << u"Adds 1 to 28 Lightning damage"_s;
        QTest::newRow("styled") << u"<augmented>{+20%}"_s << u"+20%"_s;
        QTest::newRow("spacing") << u"  Gain 9 Life\nper  enemy killed "_s
                                 << u"Gain 9 Life per enemy killed"_s;
        QTest::newRow("nothing to do") << u"+72 to maximum Life"_s << u"+72 to maximum Life"_s;
        QTest::newRow("empty") << QString() << QString();
    }

    void cleaning()
    {
        QFETCH(QString, text);
        QFETCH(QString, expected);
        QCOMPARE(cleanListingText(text), expected);
    }

    void priceTexts_data()
    {
        QTest::addColumn<double>("amount");
        QTest::addColumn<QString>("currency");
        QTest::addColumn<QString>("expected");

        QTest::newRow("divine") << 3.0 << u"divine"_s << u"3 div"_s;
        QTest::newRow("part of one") << 1.5 << u"divine"_s << u"1.5 div"_s;
        QTest::newRow("exalted") << 50.0 << u"exalted"_s << u"50 ex"_s;
        QTest::newRow("chaos") << 2.0 << u"chaos"_s << u"2 chaos"_s;
        QTest::newRow("others as named") << 1.0 << u"mirror"_s << u"1 mirror"_s;
    }

    void priceTexts()
    {
        QFETCH(double, amount);
        QFETCH(QString, currency);
        QFETCH(QString, expected);
        QCOMPARE(formatPrice(ListingPrice{amount, currency}), expected);
    }

    void inExalts()
    {
        QCOMPARE(priceInExalts({3, u"divine"_s}, rates).value_or(0), 2100.0);
        QCOMPARE(priceInExalts({4, u"chaos"_s}, rates).value_or(0), 40.0);
        QCOMPARE(priceInExalts({2, u"Divine"_s}, rates).value_or(0), 1400.0);
        // exalts need no rate, everything else does
        QCOMPARE(priceInExalts({50, u"exalted"_s}, {}).value_or(0), 50.0);
        QVERIFY(!priceInExalts({3, u"divine"_s}, {}));
        QVERIFY(!priceInExalts({2, u"mystery"_s}, rates));
        QVERIFY(!priceInExalts({2, u"divine"_s}, {{u"divine"_s, 0}}));
    }

    void convertedPrices_data()
    {
        QTest::addColumn<double>("amount");
        QTest::addColumn<QString>("currency");
        QTest::addColumn<double>("exalts");
        QTest::addColumn<double>("divineRate");
        QTest::addColumn<QString>("expected");

        QTest::newRow("divines in exalts") << 3.0 << u"divine"_s << 2100.0 << 700.0 << u"about 2100 ex"_s;
        QTest::newRow("a divine's worth on")
            << 1000.0 << u"exalted"_s << 1000.0 << 700.0 << u"about 1.4 div"_s;
        QTest::newRow("exactly a divine") << 700.0 << u"exalted"_s << 700.0 << 700.0 << u"about 1 div"_s;
        QTest::newRow("chaos in exalts") << 4.0 << u"chaos"_s << 40.0 << 700.0 << u"about 40 ex"_s;
        QTest::newRow("chaos in divines") << 100.0 << u"chaos"_s << 1000.0 << 700.0 << u"about 1.4 div"_s;
        QTest::newRow("under ten keeps a decimal") << 1.0 << u"chaos"_s << 2.5 << 700.0 << u"about 2.5 ex"_s;
        QTest::newRow("exalts say no more") << 50.0 << u"exalted"_s << 50.0 << 700.0 << QString();
        QTest::newRow("no divine rate") << 3.0 << u"chaos"_s << 30.0 << 0.0 << u"about 30 ex"_s;
        QTest::newRow("exalts without a rate") << 5000.0 << u"exalted"_s << 5000.0 << 0.0 << QString();
        QTest::newRow("capitals") << 2.0 << u"Divine"_s << 1400.0 << 700.0 << u"about 1400 ex"_s;
    }

    void convertedPrices()
    {
        QFETCH(double, amount);
        QFETCH(QString, currency);
        QFETCH(double, exalts);
        QFETCH(double, divineRate);
        QFETCH(QString, expected);
        QCOMPARE(formatConvertedPrice(ListingPrice{amount, currency}, exalts, divineRate), expected);
    }

    void unusableExalts()
    {
        QVERIFY(formatConvertedPrice({1, u"chaos"_s}, qQNaN(), 700).isEmpty());
        QVERIFY(formatConvertedPrice({1, u"chaos"_s}, qInf(), 700).isEmpty());
    }

    void listedAgoTexts_data()
    {
        QTest::addColumn<qint64>("seconds");
        QTest::addColumn<QString>("expected");

        QTest::newRow("this second") << 0LL << u"just now"_s;
        QTest::newRow("under a minute") << 59LL << u"just now"_s;
        QTest::newRow("a minute") << 60LL << u"1 minute ago"_s;
        QTest::newRow("minutes") << 5 * 60LL + 30 << u"5 minutes ago"_s;
        QTest::newRow("under an hour") << 59 * 60LL + 59 << u"59 minutes ago"_s;
        QTest::newRow("an hour") << 3600LL << u"1 hour ago"_s;
        QTest::newRow("hours") << 5 * 3600LL << u"5 hours ago"_s;
        QTest::newRow("under a day") << 24 * 3600LL - 1 << u"23 hours ago"_s;
        QTest::newRow("a day") << 24 * 3600LL << u"1 day ago"_s;
        QTest::newRow("days") << 40 * 24 * 3600LL << u"40 days ago"_s;
        QTest::newRow("ahead of the clock") << -90LL << u"just now"_s;
    }

    void listedAgoTexts()
    {
        QFETCH(qint64, seconds);
        QFETCH(QString, expected);
        const QDateTime indexed = QDateTime(QDate(2026, 10, 20), QTime(8, 0), QTimeZone::utc());
        QCOMPARE(listedAgo(indexed, indexed.addSecs(seconds)), expected);
    }

    void listedAgoNeedsTimes()
    {
        const QDateTime now = QDateTime(QDate(2026, 10, 20), QTime(8, 0), QTimeZone::utc());
        QVERIFY(listedAgo({}, now).isEmpty());
        QVERIFY(listedAgo(now, {}).isEmpty());
    }

    // what came back for three ids, one sold in the meantime
    void fetchAnswer()
    {
        const QJsonObject response{{u"result"_s, QJsonArray{gloves(), QJsonValue(), cuffs()}}};
        const QList<Listing> listings = listingsFromJson(response, u"Gloves"_s, rates);
        QCOMPARE(listings.size(), 2);
        QCOMPARE(listings.at(0).id, u"abc123"_s);
        QCOMPARE(listings.at(1).id, u"f00d"_s);
        QVERIFY(listingsFromJson({}, u"Gloves"_s, rates).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestListing)
#include "tst_listing.moc"
