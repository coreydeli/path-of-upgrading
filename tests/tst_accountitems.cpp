// SPDX-License-Identifier: MIT
#include "core/accountitems.h"
#include "core/characterdata.h"
#include "core/listing.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

QJsonObject object(const char *json)
{
    return QJsonDocument::fromJson(json).object();
}

// made-up items, as the account API words them
const char *helm =
    R"({"inventoryId":"Helm","name":"Fake Crown","typeLine":"Iron Hat","baseType":"Iron Hat",)"
    R"("rarity":"Rare","ilvl":81,"identified":true,)"
    R"("properties":[{"name":"Quality","values":[["+20%",1]]},{"name":"Armour","values":[["45",1]]}],)"
    R"("implicitMods":["+10 to Spirit"],)"
    R"("explicitMods":["+45 to maximum Life","+12% to Fire Resistance"],)"
    R"("runeMods":["+8% to Cold Resistance"]})";

const char *ring =
    R"({"inventoryId":"Ring2","name":"Fake Loop","typeLine":"Fake Ring","baseType":"Fake Ring",)"
    R"("rarity":"Rare","ilvl":70,)"
    R"("explicitMods":[{"text":"+30 to maximum Life"},)"
    R"({"text":"+20% to Lightning Resistance","fractured":true},)"
    R"({"text":"+9 to Dexterity","desecrated":true}]})";

QJsonObject withPlace(const char *json, const QString &place, int x = 0)
{
    QJsonObject o = object(json);
    o.insert(u"inventoryId"_s, place);
    o.insert(u"x"_s, x);
    return o;
}

} // namespace

class TestAccountItems : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void slotsFromWhereItemsSit_data()
    {
        QTest::addColumn<QString>("place");
        QTest::addColumn<int>("x");
        QTest::addColumn<QString>("slot");
        QTest::newRow("weapon") << u"Weapon"_s << 0 << u"Weapon"_s;
        QTest::newRow("second weapon") << u"Weapon2"_s << 0 << u"Weapon 2"_s;
        QTest::newRow("offhand") << u"Offhand"_s << 0 << u"Offhand"_s;
        QTest::newRow("second offhand") << u"Offhand2"_s << 0 << u"Offhand 2"_s;
        QTest::newRow("helm") << u"Helm"_s << 0 << u"Helmet"_s;
        QTest::newRow("body") << u"BodyArmour"_s << 0 << u"Body Armour"_s;
        QTest::newRow("gloves") << u"Gloves"_s << 0 << u"Gloves"_s;
        QTest::newRow("boots") << u"Boots"_s << 0 << u"Boots"_s;
        QTest::newRow("belt") << u"Belt"_s << 0 << u"Belt"_s;
        QTest::newRow("amulet") << u"Amulet"_s << 0 << u"Amulet"_s;
        QTest::newRow("ring") << u"Ring"_s << 0 << u"Ring 1"_s;
        QTest::newRow("second ring") << u"Ring2"_s << 0 << u"Ring 2"_s;
        QTest::newRow("life flask") << u"Flask"_s << 0 << u"Life Flask"_s;
        QTest::newRow("mana flask") << u"Flask"_s << 1 << u"Mana Flask"_s;
        QTest::newRow("no third flask") << u"Flask"_s << 2 << QString();
        QTest::newRow("charm") << u"Charm"_s << 0 << u"Charm 1"_s;
        QTest::newRow("third charm") << u"Charm"_s << 2 << u"Charm 3"_s;
        QTest::newRow("no fourth charm") << u"Charm"_s << 3 << QString();
        QTest::newRow("jewel") << u"PassiveJewels"_s << 0 << u"Jewel 1"_s;
        QTest::newRow("last jewel") << u"PassiveJewels"_s << 5 << u"Jewel 6"_s;
        QTest::newRow("no seventh jewel") << u"PassiveJewels"_s << 6 << QString();
        QTest::newRow("a bag") << u"MainInventory"_s << 0 << QString();
        QTest::newRow("nowhere") << QString() << 0 << QString();
    }

    void slotsFromWhereItemsSit()
    {
        QFETCH(QString, place);
        QFETCH(int, x);
        QFETCH(QString, slot);
        QCOMPARE(accountSlotOf({{u"inventoryId"_s, place}, {u"x"_s, x}}), slot);
    }

    void readsAnItemLikeACopiedOne()
    {
        const Item item = accountItem(object(helm), u"Helmet"_s);
        QCOMPARE(item.itemClass, u"Helmets"_s);
        QCOMPARE(item.slot, u"Helmet"_s);
        QCOMPARE(item.rarity, u"Rare"_s);
        QCOMPARE(item.name, u"Fake Crown"_s);
        QCOMPARE(item.baseType, u"Iron Hat"_s);
        QCOMPARE(item.itemLevel, 81);
        QCOMPARE(item.quality, 20);
        QCOMPARE(item.stats.value(u"# to maximum Life"_s), 45.0);
        QCOMPARE(item.stats.value(u"[Base] Armour"_s), 45.0);
        bool rune = false;
        for (const ItemLine &line : item.lines)
            rune = rune || (line.tag == u"rune" && line.key.contains(u"Cold Resistance"));
        QVERIFY(rune);
    }

    void readsFracturedAndDesecratedMods()
    {
        const Item item = accountItem(object(ring), u"Ring 2"_s);
        QCOMPARE(item.itemClass, u"Rings"_s);
        QCOMPARE(item.stats.value(u"# to maximum Life"_s), 30.0);
        QString fractured;
        QString desecrated;
        QString explicitLines;
        for (const ItemLine &line : item.lines) {
            if (line.tag == u"fractured")
                fractured = line.text;
            else if (line.tag == u"desecrated")
                desecrated = line.text;
            else if (line.tag == u"explicit")
                explicitLines += line.text;
        }
        QVERIFY(fractured.contains(u"Lightning Resistance"));
        QVERIFY(desecrated.contains(u"Dexterity"));
        QVERIFY(explicitLines.contains(u"maximum Life"));
        QVERIFY(!explicitLines.contains(u"Lightning"));
        QVERIFY(!explicitLines.contains(u"Dexterity"));
    }

    void itemsAreTheSameAsTheirListings()
    {
        const QJsonObject o = object(helm);
        const Item direct = itemFromTradeJson(o, u"Helmets"_s);
        const Item listed = listingToItem({{u"item"_s, o}}, u"Helmets"_s);
        QCOMPARE(direct.raw, listed.raw);
        QVERIFY(!direct.raw.isEmpty());
    }

    void putsEachItemInItsSlot()
    {
        QJsonObject character;
        character.insert(u"equipment"_s, QJsonArray{
                                             object(helm),
                                             object(ring),
                                             withPlace(helm, u"Flask"_s, 0),
                                             withPlace(helm, u"Flask"_s, 1),
                                             // a bag, which isn't worn
                                             withPlace(helm, u"MainInventory"_s),
                                         });
        character.insert(u"jewels"_s, QJsonArray{withPlace(ring, u"PassiveJewels"_s, 2)});

        const Equipped gear = equipmentOf(character);
        QStringList slots(gear.keyBegin(), gear.keyEnd());
        slots.sort();
        QCOMPARE(slots,
                 (QStringList{u"Helmet"_s, u"Jewel 3"_s, u"Life Flask"_s, u"Mana Flask"_s, u"Ring 2"_s}));
        QCOMPARE(gear.value(u"Life Flask"_s).itemClass, u"Life Flasks"_s);
        QCOMPARE(gear.value(u"Jewel 3"_s).itemClass, u"Jewels"_s);
    }

    void theFirstItemInASlotStays()
    {
        QJsonObject second = object(helm);
        second.insert(u"name"_s, u"Fake Second"_s);
        QJsonObject character;
        character.insert(u"equipment"_s, QJsonArray{object(helm), second});
        const Equipped gear = equipmentOf(character);
        QCOMPARE(gear.size(), 1);
        QCOMPARE(gear.value(u"Helmet"_s).name, u"Fake Crown"_s);
    }

    void leavesOutWhatItCantRead()
    {
        QJsonObject character;
        character.insert(u"equipment"_s,
                         QJsonArray{QJsonObject{{u"inventoryId"_s, u"Helm"_s}}, QJsonValue(5)});
        QVERIFY(equipmentOf(character).isEmpty());
        QVERIFY(equipmentOf({}).isEmpty());
    }

    void layersGearOnACharacterAndKeepsTheRest()
    {
        Character before;
        before.name = u"FakeOne"_s;
        before.linked = AccountLink{.realm = u"poe2"_s, .id = u"fakeid1"_s, .name = u"FakeOne"_s};
        before.weights = {{u"# to maximum Life"_s, 3}};
        before.profilePicked = true;
        before.buildProfile = u"minions"_s;
        before.weightsOnly = true;
        before.gear.insert(u"Belt"_s, accountItem(object(helm), u"Helmet"_s));
        before.fromPob.insert(u"Belt"_s);

        const Equipped gear = equipmentOf({{u"equipment"_s, QJsonArray{object(ring)}}});
        const AccountLink link{.realm = u"poe2"_s, .id = u"fakeid1"_s, .name = u"FakeTwo"_s};
        const Character after = withAccountGear(before, link, gear);

        QCOMPARE(after.name, u"FakeTwo"_s);
        QCOMPARE(after.linked, std::optional<AccountLink>(link));
        QCOMPARE(after.gear.size(), 1);
        QVERIFY(after.gear.contains(u"Ring 2"_s));
        QVERIFY(after.fromPob.isEmpty());
        QCOMPARE(after.weights.value(u"# to maximum Life"_s), 3.0);
        QVERIFY(after.profilePicked);
        QCOMPARE(after.buildProfile, u"minions"_s);
        QVERIFY(after.weightsOnly);
    }

    void aNameYouGaveItStays()
    {
        Character before;
        before.name = u"My Witch"_s;
        before.linked = AccountLink{.realm = u"poe2"_s, .id = u"fakeid1"_s, .name = u"FakeOne"_s};
        const AccountLink link{.realm = u"poe2"_s, .id = u"fakeid1"_s, .name = u"FakeTwo"_s};
        QCOMPARE(withAccountGear(before, link, {}).name, u"My Witch"_s);
        QCOMPARE(withAccountGear(before, link, {}).linked->name, u"FakeTwo"_s);
    }

    void theLinkIsSavedWithTheCharacter()
    {
        Character c;
        c.name = u"FakeOne"_s;
        c.linked = AccountLink{.realm = u"poe2"_s, .id = u"fakeid1"_s, .name = u"FakeOne"_s};
        const std::optional<Character> back = characterFromJson(toJson(c));
        QVERIFY(back);
        QCOMPARE(back->linked, c.linked);
    }

    void oldFilesAndHalfWrittenLinksHaveNone()
    {
        QJsonObject o = toJson(Character{});
        QVERIFY(!o.contains(u"account"_s));
        QVERIFY(!characterFromJson(o)->linked);
        o.insert(u"account"_s, QJsonObject{{u"name"_s, u"FakeOne"_s}});
        QVERIFY(!characterFromJson(o)->linked);
        o.insert(u"account"_s, QJsonObject{{u"id"_s, u"fakeid1"_s}});
        const auto link = characterFromJson(o)->linked;
        QVERIFY(link);
        QCOMPARE(link->realm, u"poe2"_s);
    }
};

QTEST_GUILESS_MAIN(TestAccountItems)
#include "tst_accountitems.moc"
