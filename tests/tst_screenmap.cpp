// SPDX-License-Identifier: MIT
#include "platform/screenmap.h"

#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

class TestScreenMap : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void points_data()
    {
        QTest::addColumn<QList<NamedScreen>>("monitors");
        QTest::addColumn<QList<NamedScreen>>("screens");
        QTest::addColumn<QPoint>("point");
        QTest::addColumn<std::optional<ScreenPoint>>("onScreen");

        const QList<NamedScreen> wide = {{u"DP-4"_s, QRect(0, 0, 7680, 2160)}};
        QTest::newRow("same pixels") << wide << wide << QPoint(1000, 500)
                                     << std::optional(ScreenPoint{u"DP-4"_s, QPoint(1000, 500)});

        // at 150%, with X11 programs scaling themselves
        QTest::newRow("scaled") << QList<NamedScreen>{{u"DP-1"_s, QRect(0, 0, 3840, 2160)}}
                                << QList<NamedScreen>{{u"DP-1"_s, QRect(0, 0, 2560, 1440)}}
                                << QPoint(1920, 1080)
                                << std::optional(ScreenPoint{u"DP-1"_s, QPoint(1280, 720)});

        // a laptop at 200% with a monitor at 100% to its right
        const QList<NamedScreen> monitors = {{u"eDP-1"_s, QRect(0, 0, 2880, 1800)},
                                             {u"DP-2"_s, QRect(2880, 0, 3840, 2160)}};
        const QList<NamedScreen> screens = {{u"eDP-1"_s, QRect(0, 0, 1440, 900)},
                                            {u"DP-2"_s, QRect(1440, 0, 3840, 2160)}};
        QTest::newRow("laptop") << monitors << screens << QPoint(1440, 900)
                                << std::optional(ScreenPoint{u"eDP-1"_s, QPoint(720, 450)});
        QTest::newRow("monitor beside it") << monitors << screens << QPoint(2980, 50)
                                           << std::optional(ScreenPoint{u"DP-2"_s, QPoint(100, 50)});
        QTest::newRow("off both") << monitors << screens << QPoint(-5, 0) << std::optional<ScreenPoint>();
        QTest::newRow("names that don't match")
            << monitors << QList<NamedScreen>{{u"HDMI-A-1"_s, QRect(0, 0, 1920, 1080)}, screens.at(1)}
            << QPoint(10, 10) << std::optional<ScreenPoint>();

        QTest::newRow("old Xwayland")
            << QList<NamedScreen>{{u"XWAYLAND0"_s, QRect(0, 0, 1920, 1080)}}
            << QList<NamedScreen>{{u"DP-1"_s, QRect(0, 0, 1920, 1080)}} << QPoint(10, 10)
            << std::optional(ScreenPoint{u"DP-1"_s, QPoint(10, 10)});
        // with more than one, there's no telling which
        QTest::newRow("old Xwayland, two monitors")
            << QList<NamedScreen>{{u"XWAYLAND0"_s, QRect(0, 0, 1920, 1080)},
                                  {u"XWAYLAND1"_s, QRect(1920, 0, 1920, 1080)}}
            << QList<NamedScreen>{{u"DP-1"_s, QRect(0, 0, 1920, 1080)}} << QPoint(10, 10)
            << std::optional<ScreenPoint>();
        QTest::newRow("old Xwayland, two screens")
            << QList<NamedScreen>{{u"XWAYLAND0"_s, QRect(0, 0, 3840, 1080)}}
            << QList<NamedScreen>{{u"DP-1"_s, QRect(0, 0, 1920, 1080)},
                                  {u"DP-2"_s, QRect(1920, 0, 1920, 1080)}}
            << QPoint(10, 10) << std::optional<ScreenPoint>();
    }

    void points()
    {
        QFETCH(QList<NamedScreen>, monitors);
        QFETCH(QList<NamedScreen>, screens);
        QFETCH(QPoint, point);
        QFETCH(std::optional<ScreenPoint>, onScreen);
        QCOMPARE(mapToScreen(point, monitors, screens), onScreen);
    }
};

QTEST_GUILESS_MAIN(TestScreenMap)
#include "tst_screenmap.moc"
