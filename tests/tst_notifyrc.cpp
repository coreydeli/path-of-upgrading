// SPDX-License-Identifier: MIT
#include "platform/notifier.h"

#include <KConfig>
#include <KConfigGroup>
#include <QFile>
#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

// data/path-of-upgrading.notifyrc is what Plasma lists the notifications by,
// read the way KNotification reads it.
class TestNotifyrc : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init() { QTest::failOnWarning(); }

    void belongsToTheApp()
    {
        QVERIFY(QFile::exists(QStringLiteral(NOTIFYRC)));
        const KConfig rc(QStringLiteral(NOTIFYRC), KConfig::SimpleConfig);
        const KConfigGroup global = rc.group(u"Global"_s);
        QCOMPARE(global.readEntry(u"Name"_s, QString()), u"Path of Upgrading"_s);
        QCOMPARE(global.readEntry(u"DesktopEntry"_s, QString()), QStringLiteral(APP_ID));
        QCOMPARE(global.readEntry(u"IconName"_s, QString()), QStringLiteral(APP_ID));
    }

    void listsEveryEvent()
    {
        const KConfig rc(QStringLiteral(NOTIFYRC), KConfig::SimpleConfig);
        for (const Notifier::Event event : {Notifier::Event::HotkeysUnavailable}) {
            const QString name = u"Event/"_s + eventId(event);
            QVERIFY2(rc.hasGroup(name), qPrintable(name));
            const KConfigGroup group = rc.group(name);
            QVERIFY2(!group.readEntry(u"Name"_s, QString()).isEmpty(), qPrintable(name));
            // shown as a popup, which is what these are for
            QCOMPARE(group.readEntry(u"Action"_s, QString()), u"Popup"_s);
        }
    }
};

QTEST_GUILESS_MAIN(TestNotifyrc)
#include "tst_notifyrc.moc"
