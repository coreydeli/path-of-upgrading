// SPDX-License-Identifier: MIT
#include "platform/gamewindow.h"

#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

class TestGameWindow : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void windows_data()
    {
        QTest::addColumn<QStringList>("classes");
        QTest::addColumn<QString>("program");
        QTest::addColumn<bool>("isGame");

        const QStringList steam = {u"steam_app_2694490"_s, u"steam_app_2694490"_s};
        // with the program as /proc has it, cut to 15 characters
        QTest::newRow("Steam") << steam << u"PathOfExileStea"_s << true;
        // from inside a Flatpak, which can't see the process
        QTest::newRow("Steam, process unseen") << steam << QString() << true;
        const QStringList wine = {u"pathofexile.exe"_s, u"pathofexile.exe"_s};
        QTest::newRow("Wine") << wine << u"PathOfExile.exe"_s << true;
        QTest::newRow("Wine, process unseen") << wine << QString() << true;
        // a launcher that gives Proton no Steam id
        QTest::newRow("Proton without an id")
            << QStringList{u"steam_app_default"_s, u"steam_app_default"_s} << u"PathOfExile.exe"_s << true;
        QTest::newRow("another Steam game")
            << QStringList{u"steam_app_1091500"_s, u"steam_app_1091500"_s} << u"Cyberpunk2077.e"_s << false;
        QTest::newRow("browser") << QStringList{u"Navigator"_s, u"firefox"_s} << u"firefox"_s << false;
        QTest::newRow("nothing to go on") << QStringList() << QString() << false;
    }

    void windows()
    {
        QFETCH(QStringList, classes);
        QFETCH(QString, program);
        QFETCH(bool, isGame);
        QCOMPARE(isGameWindow({.classes = classes, .program = program}), isGame);
    }
};

QTEST_GUILESS_MAIN(TestGameWindow)
#include "tst_gamewindow.moc"
