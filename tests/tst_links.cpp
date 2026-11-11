// SPDX-License-Identifier: MIT
#include "core/links.h"

#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

class TestLinks : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void theProject()
    {
        QCOMPARE(repositoryUrl(), u"https://github.com/coreydeli/path-of-upgrading"_s);
        QCOMPARE(releasesUrl(), u"https://github.com/coreydeli/path-of-upgrading/releases"_s);
        QCOMPARE(issuesUrl(), u"https://github.com/coreydeli/path-of-upgrading/issues"_s);
        QCOMPARE(wikiUrl(), u"https://github.com/coreydeli/path-of-upgrading/wiki"_s);
    }

    void wikiPages()
    {
        QCOMPARE(wikiPageUrl(u"Trade-Search"_s),
                 u"https://github.com/coreydeli/path-of-upgrading/wiki/Trade-Search"_s);
    }
};

QTEST_APPLESS_MAIN(TestLinks)
#include "tst_links.moc"
