// SPDX-License-Identifier: MIT
#include "faketray.h"
#include "platform/traynotifier.h"

#include <QTest>

using namespace pou;
using namespace Qt::Literals::StringLiterals;

class TestTrayNotifier : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void fromTheIcon()
    {
        FakeTray tray;
        TrayNotifier notifier(&tray);
        notifier.notify(Notifier::Event::HotkeysUnavailable, u"No hotkeys"_s, u"Something has them."_s);
        QCOMPARE(tray.messages, QStringList{u"No hotkeys\nSomething has them."_s});
    }

    void trayGone()
    {
        auto tray = std::make_unique<FakeTray>();
        TrayNotifier notifier(tray.get());
        tray.reset();
        notifier.notify(Notifier::Event::HotkeysUnavailable, u"No hotkeys"_s, u"Something has them."_s);
    }
};

QTEST_GUILESS_MAIN(TestTrayNotifier)
#include "tst_traynotifier.moc"
