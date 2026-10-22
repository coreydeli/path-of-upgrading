// SPDX-License-Identifier: MIT
#include "core/ignore.h"

#include <QJsonObject>
#include <QTest>

using namespace pou;
using namespace Qt::Literals::StringLiterals;

class TestIgnore : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void presetsAndSingleStats()
    {
        const QList<IgnoreRule> rules = {
            {u"eleres"_s, {}, IgnoreRule::Mode::Ignore},
            {{}, u"#% increased Rarity of Items found"_s, IgnoreRule::Mode::Exclude},
        };
        QVERIFY(isIgnored(rules, u"#% to Cold Resistance"_s));
        QVERIFY(!isIgnored(rules, u"#% to Chaos Resistance"_s));
        QCOMPARE(ruleFor(rules, u"#% to Fire Resistance"_s), &rules.first());
        // the same stat in trade-site wording still counts
        QCOMPARE(ruleFor(rules, u"+#% increased Rarity of Items found (Local)"_s), &rules.last());
        QVERIFY(!isIgnored({}, u"# to maximum Life"_s));
    }

    void excludedKeysOnlyFromExcludeRules()
    {
        const QList<IgnoreRule> rules = {
            {u"life"_s, {}, IgnoreRule::Mode::Ignore},
            {u"chaosres"_s, {}, IgnoreRule::Mode::Exclude},
            {u"res"_s, {}, IgnoreRule::Mode::Exclude},
        };
        QCOMPARE(excludedKeys(rules),
                 QStringList({u"#% to Chaos Resistance"_s, u"#% to Fire Resistance"_s,
                              u"#% to Cold Resistance"_s, u"#% to Lightning Resistance"_s}));
    }

    void describesRules()
    {
        const QList<IgnoreRule> rules = {
            {u"res"_s, {}, IgnoreRule::Mode::Exclude},
            {{}, u"# to Strength"_s, IgnoreRule::Mode::Ignore},
        };
        QCOMPARE(describe(rules), QStringList({u"All resistances (excluded)"_s, u"# to Strength"_s}));
    }

    void jsonRoundTripDropsBadRules()
    {
        const QJsonArray saved = {
            QJsonObject{{u"preset"_s, u"res"_s}, {u"mode"_s, u"exclude"_s}},
            QJsonObject{{u"key"_s, u"# to Strength"_s}},
            QJsonObject{{u"preset"_s, u"nonsense"_s}},
            QJsonObject{{u"key"_s, QString()}},
            u"junk"_s,
        };
        const QList<IgnoreRule> rules = ignoreRulesFromJson(saved);
        QCOMPARE(rules.size(), 2);
        QCOMPARE(rules.at(0).mode, IgnoreRule::Mode::Exclude);
        QCOMPARE(rules.at(1).key, u"# to Strength"_s);
        QCOMPARE(rules.at(1).mode, IgnoreRule::Mode::Ignore);
        QCOMPARE(ignoreRulesFromJson(toJson(rules)), rules);
    }
};

QTEST_GUILESS_MAIN(TestIgnore)
#include "tst_ignore.moc"
