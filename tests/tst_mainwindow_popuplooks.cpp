// SPDX-License-Identifier: MIT
#include "app/app.h"
#include "app/fonts.h"
#include "core/parser.h"
#include "core/sessionguide.h"
#include "core/slots.h"
#include "core/weights.h"
#include "fakegameinput.h"
#include "fakegamewatcher.h"
#include "fakeglobalshortcuts.h"
#include "fakenetwork.h"
#include "fakeopener.h"
#include "fakepob.h"
#include "fakesecretstore.h"
#include "itemtexts.h"
#include "mainwindowtest.h"
#include "services/datafiles.h"
#include "stylefortests.h"
#include "tradefixtures.h"
#include "updatetexts.h"

#include <KLocalizedQmlContext>
#include <QAccessible>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFontDatabase>
#include <QJSEngine>
#include <QJSValue>
#include <QJsonArray>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLocale>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QSet>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QtQuickTest/quicktest.h>
#include <functional>
#include <memory>
#include <optional>

Q_IMPORT_QML_PLUGIN(PathOfUpgradingPlugin)

using namespace Qt::Literals::StringLiterals;
using namespace pou;
using namespace pou::windowtest;

class TestMainWindowPopupLooks : public QObject
{
    Q_OBJECT

public:
    // The window isn't for the screen. Kirigami would ask the desktop portal
    // for the look of the desktop, which starts it.
    static void initMain()
    {
        qputenv("QT_QPA_PLATFORM", "offscreen");
        // the app's own controls, as the program has
        useStyleInTests();
        qputenv("QT_NO_XDG_DESKTOP_PORTAL", "1");
    }

private Q_SLOTS:
    void initTestCase()
    {
        // so the desktop's color settings are neither read nor written
        QStandardPaths::setTestModeEnabled(true);
        // a container can have no fonts at all, and the font pickers need one
        QVERIFY(addBundledFonts());
    }

    void init()
    {
        // Debian 13's Qt 6.8 now and then warns from its own animation timer that the
        // driver isn't running (Arch and Windows never do), so only that one passes there
#if QT_VERSION < QT_VERSION_CHECK(6, 9, 0)
        QTest::failOnWarning(
            QRegularExpression(u"^(?!QUnifiedTimer::stopAnimationDriver: driver is not running$)"_s));
#else
        QTest::failOnWarning();
#endif
    }

    // every look, light and dark where it has both
    void thePopupLoadsUnderEachDesign_data()
    {
        QTest::addColumn<QString>("design");
        QTest::addColumn<QString>("brightness");
        for (const DesignInfo &info : designs()) {
            if (!info.hasLight) {
                QTest::newRow(qPrintable(info.id)) << info.id << u"desktop"_s;
                continue;
            }
            QTest::newRow(qPrintable(info.id + u" light"_s)) << info.id << u"light"_s;
            QTest::newRow(qPrintable(info.id + u" dark"_s)) << info.id << u"dark"_s;
        }
    }

    // the popup is another window on the same engine
    void thePopupLoadsUnderEachDesign()
    {
        QFETCH(QString, design);
        QFETCH(QString, brightness);
        Window w(true, false, QStringLiteral(COLORS));
        w.play({{u"Helmet"_s, item(itemtexts::helmetOld)}});
        w.app->appearance()->setBrightness(brightness);
        w.app->appearance()->setDesign(design);
        QVERIFY(w.shown());
        w.app->history().add(item(itemtexts::helmetNew));
        QVERIFY(w.show(u"checkedAction"_s));
        ItemPopup &popup = w.app->checker().popup();
        QVERIFY(click(w.find(u"checked:0"_s)->findChild<QObject *>(u"checkAgainButton"_s)));
        QVERIFY(popup.isVisible());
        QVERIFY(QQuickTest::qWaitForPolish(popup.window()));
    }
};

QTEST_MAIN(TestMainWindowPopupLooks)
#include "tst_mainwindow_popuplooks.moc"
