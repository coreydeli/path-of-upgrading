// SPDX-License-Identifier: MIT
#include "app/words.h"

#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

class TestWords : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void slotsReadAsTheGameSaysThem()
    {
        const Words words;
        QCOMPARE(words.slot(u"Offhand"_s), u"Off Hand"_s);
        QCOMPARE(words.slot(u"Offhand 2"_s), u"Off Hand 2"_s);
        QCOMPARE(words.slot(u"Helmet"_s), u"Helmet"_s);
    }
};

QTEST_GUILESS_MAIN(TestWords)
#include "tst_words.moc"
