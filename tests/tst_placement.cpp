// SPDX-License-Identifier: MIT
#include "core/placement.h"

#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

using Mouse = std::optional<QPoint>;

class TestPlacement : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void popup_data()
    {
        QTest::addColumn<PopupPosition>("where");
        QTest::addColumn<QSize>("popup");
        QTest::addColumn<Mouse>("mouse");
        QTest::addColumn<QPoint>("expected");

        const QSize popup(400, 300);
        QTest::newRow("right of the mouse")
            << PopupPosition::Cursor << popup << Mouse(QPoint(500, 500)) << QPoint(524, 400);
        QTest::newRow("left of the mouse by the right edge")
            << PopupPosition::Cursor << popup << Mouse(QPoint(1700, 500)) << QPoint(1276, 400);
        QTest::newRow("mouse near the top")
            << PopupPosition::Cursor << popup << Mouse(QPoint(500, 50)) << QPoint(524, 4);
        QTest::newRow("mouse near the bottom")
            << PopupPosition::Cursor << popup << Mouse(QPoint(500, 1050)) << QPoint(524, 776);
        QTest::newRow("mouse we can't see") << PopupPosition::Cursor << popup << Mouse() << QPoint(760, 16);
        QTest::newRow("left side") << PopupPosition::Left << popup << Mouse(QPoint(500, 500))
                                   << QPoint(16, 129);
        QTest::newRow("right side") << PopupPosition::Right << popup << Mouse(QPoint(500, 500))
                                    << QPoint(1504, 129);
        QTest::newRow("top") << PopupPosition::Top << popup << Mouse(QPoint(500, 500)) << QPoint(760, 16);
        QTest::newRow("bigger than the screen")
            << PopupPosition::Cursor << QSize(2000, 1200) << Mouse(QPoint(500, 500)) << QPoint(4, 4);
        QTest::newRow("no room on either side")
            << PopupPosition::Cursor << QSize(1000, 300) << Mouse(QPoint(960, 500)) << QPoint(4, 400);
    }

    void popup()
    {
        QFETCH(PopupPosition, where);
        QFETCH(QSize, popup);
        QFETCH(Mouse, mouse);
        QFETCH(QPoint, expected);

        QCOMPARE(placePopup(where, popup, QSize(1920, 1080), mouse), expected);
    }

    void ids_data()
    {
        QTest::addColumn<PopupPosition>("position");
        QTest::addColumn<QString>("id");
        QTest::newRow("cursor") << PopupPosition::Cursor << u"cursor"_s;
        QTest::newRow("left") << PopupPosition::Left << u"left"_s;
        QTest::newRow("right") << PopupPosition::Right << u"right"_s;
        QTest::newRow("top") << PopupPosition::Top << u"top"_s;
    }

    void ids()
    {
        QFETCH(PopupPosition, position);
        QFETCH(QString, id);
        QCOMPARE(popupPositionId(position), id);
        QCOMPARE(popupPositionFromId(id), position);
    }

    void unknownId()
    {
        QCOMPARE(popupPositionFromId(u"middle"), PopupPosition::Cursor);
        QCOMPARE(popupPositionFromId(u""), PopupPosition::Cursor);
    }
};

QTEST_GUILESS_MAIN(TestPlacement)
#include "tst_placement.moc"
