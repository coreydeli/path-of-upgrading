// SPDX-License-Identifier: MIT
#include "app/itemjudge.h"
#include "core/parser.h"
#include "itemtexts.h"

#include <QSignalSpy>
#include <QTest>

using namespace pou;

namespace {

Item item(const QString &text)
{
    return parseItem(text).value();
}

const Item helmetNew = item(itemtexts::helmetNew);
const Item helmetOld = item(itemtexts::helmetOld);

Character wearing(const Equipped &gear)
{
    Character c;
    c.gear = gear;
    return c;
}

} // namespace

class TestItemJudge : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void judgesForTheCharacter()
    {
        ItemJudge judge;
        judge.setCharacter(wearing({{u"Helmet"_s, helmetOld}}));
        QSignalSpy checked(&judge, &ItemJudge::checked);
        judge.check(helmetNew);
        QCOMPARE(checked.size(), 1);

        const ItemCheck check = checked.first().at(0).value<ItemCheck>();
        QCOMPARE(check.item.name, u"Doom Visage"_s);
        QCOMPARE(check.evaluation.comparison.targetSlot, u"Helmet"_s);
        QCOMPARE(check.evaluation.comparison.replacing, QStringList{u"Grim Crown"_s});
        QCOMPARE(judge.current().item.name, u"Doom Visage"_s);
    }

    void yourPriorities()
    {
        const Item spirit =
            item(u"Item Class: Amulets\nRarity: Rare\nGale Charm\nSolar Amulet\n--------\n+30 to Spirit"_s);
        const Item life = item(
            u"Item Class: Amulets\nRarity: Rare\nDoom Charm\nGold Amulet\n--------\n+64 to maximum Life"_s);
        ItemJudge judge;
        judge.setCharacter(wearing({{u"Amulet"_s, spirit}}));
        judge.check(life);
        QVERIFY(!judge.current().evaluation.risks.isEmpty());

        judge.setPriorities({.spirit = false});
        judge.check(life);
        QVERIFY(judge.current().evaluation.risks.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestItemJudge)
#include "tst_itemjudge.moc"
