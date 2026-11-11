// SPDX-License-Identifier: MIT
#include "app/help.h"
#include "fakeopener.h"

#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

QStringList pagesOf(const Help &help)
{
    QStringList pages;
    for (const QVariant &topic : help.topics())
        pages << topic.toMap().value(u"page"_s).toString();
    return pages;
}

} // namespace

class TestHelp : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init() { QTest::failOnWarning(); }

    void opensTheWikiWhenAsked()
    {
        FakeOpener opener;
        Help help(&opener, true);
        QVERIFY(opener.opened.isEmpty());
        help.openWiki();
        QCOMPARE(opener.opened, QList<QUrl>{QUrl(u"https://github.com/coreydeli/path-of-upgrading/wiki"_s)});
    }

    void topicsGoToTheirPages()
    {
        FakeOpener opener;
        Help help(&opener, true);
        const QStringList pages = pagesOf(help);
        QVERIFY(pages.contains(u"Hotkeys-and-the-Overlay"_s));
        const qsizetype at = pages.indexOf(u"Troubleshooting"_s);
        QVERIFY(at >= 0);
        help.openTopic(int(at));
        QCOMPARE(opener.opened,
                 QList<QUrl>{QUrl(u"https://github.com/coreydeli/path-of-upgrading/wiki/Troubleshooting"_s)});

        // there's no such topic
        help.openTopic(-1);
        help.openTopic(int(pages.size()));
        QCOMPARE(opener.opened.size(), 1);
    }

    void everyTopicSaysWhatItIs()
    {
        FakeOpener opener;
        Help help(&opener, true);
        for (const QVariant &topic : help.topics()) {
            const QVariantMap map = topic.toMap();
            QVERIFY(!map.value(u"title"_s).toString().isEmpty());
            QVERIFY(!map.value(u"what"_s).toString().isEmpty());
        }
    }

    void leavesOutTradeWithoutIt()
    {
        FakeOpener opener;
        QVERIFY(pagesOf(Help(&opener, true)).contains(u"Trade-Search"_s));
        QVERIFY(pagesOf(Help(&opener, true)).contains(u"Trade-Results"_s));
        const QStringList pages = pagesOf(Help(&opener, false));
        QVERIFY(!pages.contains(u"Trade-Search"_s));
        QVERIFY(!pages.contains(u"Trade-Results"_s));
        QVERIFY(pages.contains(u"The-Popup"_s));
    }

    void withoutAnOpener()
    {
        Help help(nullptr, true);
        help.openWiki();
        help.openTopic(0);
    }
};

QTEST_MAIN(TestHelp)
#include "tst_help.moc"
