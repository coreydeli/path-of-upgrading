// SPDX-License-Identifier: MIT
#include "core/catalog.h"
#include "core/statnorm.h"
#include "tradefixtures.h"

#include <QTest>

using namespace pou;
using namespace tradefixtures;

namespace {

const CatalogEntry *findEntry(const StatCatalog &catalog, const QString &text)
{
    const QString normalized = normalizeStat(text);
    for (const CatalogEntry &e : catalog.entries) {
        if (e.normalized == normalized)
            return &e;
    }
    return nullptr;
}

qsizetype indexOf(const StatCatalog &catalog, const QString &text)
{
    for (qsizetype i = 0; i < catalog.entries.size(); ++i) {
        if (catalog.entries.at(i).text == text)
            return i;
    }
    return -1;
}

SeenStats seenStats()
{
    return {
        {u"Gloves"_s, {{u"# to maximum Life"_s, 3}}},
        {u"Helmets"_s, {{u"# to maximum Life"_s, 1}, {u"#% to Fire Resistance"_s, 2}}},
    };
}

} // namespace

class TestCatalog : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void mergesTheVersions()
    {
        const StatCatalog catalog = buildCatalog(StatIndex::fromJson(stats()), seenStats());
        const CatalogEntry *life = findEntry(catalog, u"+# to maximum Life"_s);
        QVERIFY(life);
        // the explicit wording, not the pseudo "total" that comes first
        QCOMPARE(life->text, u"+# to maximum Life"_s);
        QCOMPARE(life->types, (QStringList{u"pseudo"_s, u"explicit"_s}));
        QCOMPARE(life->seen, (QHash<QString, int>{{u"Gloves"_s, 3}, {u"Helmets"_s, 1}}));
        QCOMPARE(catalog.classes, (QStringList{u"Gloves"_s, u"Helmets"_s}));
    }

    void seenOnAClass()
    {
        const StatCatalog catalog = buildCatalog(StatIndex::fromJson(stats()), seenStats());
        const CatalogEntry *life = findEntry(catalog, u"# to maximum Life"_s);
        QVERIFY(life);
        QCOMPARE(life->seenOn(u"Gloves"_s), 3);
        QCOMPARE(life->seenOn(u"Boots"_s), 0);
        QCOMPARE(life->seenOn(), 4);

        const CatalogEntry *fire = findEntry(catalog, u"#% to Fire Resistance"_s);
        QVERIFY(fire);
        QCOMPARE(fire->seenOn(), 2);
        QCOMPARE(findEntry(catalog, u"#% increased Movement Speed"_s)->seenOn(), 0);
    }

    void localAndGlobalMods()
    {
        const StatCatalog catalog = buildCatalog(StatIndex::fromJson(stats()), {});
        const CatalogEntry *armour = findEntry(catalog, u"#% increased Armour"_s);
        QVERIFY(armour);
        // one entry, worded the way items put it even though "(Local)" comes first
        QCOMPARE(armour->text, u"#% increased Armour"_s);
        QCOMPARE(armour->types, (QStringList{u"explicit"_s}));
        QCOMPARE(catalog.entries.size(), 4);
    }

    void leavesOutCountsAndSanctum()
    {
        const QJsonObject data = response({
            group(u"pseudo"_s,
                  {
                      stat(u"pseudo.pseudo_number_of_empty_prefix_mods"_s, u"# Empty Prefix Modifiers"_s,
                           u"pseudo"_s),
                      stat(u"pseudo.pseudo_number_of_affix_mods"_s, u"# Modifiers"_s, u"pseudo"_s),
                      stat(u"pseudo.pseudo_number_of_desecrated_suffix_mods"_s,
                           u"# Desecrated Suffix Modifiers"_s, u"pseudo"_s),
                      stat(u"pseudo.pseudo_tablet_uses"_s, u"# uses remaining (Tablets)"_s, u"pseudo"_s),
                      stat(u"pseudo.pseudo_total_life"_s, u"+# total maximum Life"_s, u"pseudo"_s),
                  }),
            group(u"sanctum"_s, {stat(u"sanctum.stat_avoid_resolve"_s,
                                      u"#% chance to Avoid Resolve loss from Enemy Hits"_s, u"sanctum"_s)}),
        });
        const StatCatalog catalog = buildCatalog(StatIndex::fromJson(data), {});
        QCOMPARE(catalog.entries.size(), 1);
        // with nothing else to go by, the pseudo wording
        QCOMPARE(catalog.entries.first().text, u"+# total maximum Life"_s);
        QCOMPARE(catalog.entries.first().types, (QStringList{u"pseudo"_s}));
    }

    void alphabetical()
    {
        const QJsonObject data = response({group(
            u"explicit"_s,
            {
                stat(u"explicit.stat_leech"_s, u"Leech #% of Physical Attack Damage as Life"_s,
                     u"explicit"_s),
                stat(u"explicit.stat_gain"_s, u"Gain #% of Damage as Extra Fire Damage"_s, u"explicit"_s),
                stat(u"explicit.stat_adds"_s, u"Adds # to # Physical Damage"_s, u"explicit"_s),
            })});
        const SeenStats seen = {
            {u"Rings"_s, {}},
            {u"Body Armours"_s, {{u"Gain #% of Damage as Extra Fire Damage"_s, 1}}},
            {u"Gloves"_s, {}},
        };
        const StatCatalog catalog = buildCatalog(StatIndex::fromJson(data), seen);
        QCOMPARE(indexOf(catalog, u"Adds # to # Physical Damage"_s), 0);
        QCOMPARE(indexOf(catalog, u"Gain #% of Damage as Extra Fire Damage"_s), 1);
        QCOMPARE(indexOf(catalog, u"Leech #% of Physical Attack Damage as Life"_s), 2);
        // every class you've copied, whether or not its stats are known
        QCOMPARE(catalog.classes, (QStringList{u"Body Armours"_s, u"Gloves"_s, u"Rings"_s}));
    }

    void nothingFetched()
    {
        const StatCatalog catalog = buildCatalog({}, seenStats());
        QVERIFY(catalog.entries.isEmpty());
        QCOMPARE(catalog.classes, (QStringList{u"Gloves"_s, u"Helmets"_s}));
    }

    void typeLabels()
    {
        QCOMPARE(statTypeLabel(u"pseudo"_s), u"Total"_s);
        QCOMPARE(statTypeLabel(u"explicit"_s), u"Explicit"_s);
        QCOMPARE(statTypeLabel(u"augment"_s), u"Rune"_s);
        QCOMPARE(statTypeLabel(u"desecrated"_s), u"Desecrated"_s);
        QCOMPARE(statTypeLabel(QString()), QString());
    }

    void itemKeys()
    {
        QCOMPARE(itemKeyFromText(u"+#% to Fire Resistance"_s), u"#% to Fire Resistance"_s);
        QCOMPARE(itemKeyFromText(u"# to maximum Energy Shield (Local)"_s), u"# to maximum Energy Shield"_s);
        QCOMPARE(itemKeyFromText(u"+# to Level of all Minion Skills"_s),
                 u"# to Level of all Minion Skills"_s);
        QCOMPARE(itemKeyFromText(u"Adds # to # Physical Damage"_s), u"Adds # to # Physical Damage"_s);
    }
};

QTEST_GUILESS_MAIN(TestCatalog)
#include "tst_catalog.moc"
