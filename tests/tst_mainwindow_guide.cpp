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

class TestMainWindowGuide : public QObject
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

    void setupGuideOpensOnTheFirstRun()
    {
        Window w(true, false, {}, std::nullopt);
        QObject *guide = w.guide();
        QVERIFY(guide);
        QTRY_VERIFY(guide->property("opened").toBool());
        QCOMPARE(guide->property("title").toString(), u"Welcome to Path of Upgrading"_s);
        QVERIFY(isVisible(w.inGuide(u"welcomeStep"_s)));
        QVERIFY(!isVisible(w.inGuide(u"leagueStep"_s)));
        QCOMPARE(text(w.inGuide(u"nextButton"_s)), u"Set It Up"_s);
        QVERIFY(isVisible(w.inGuide(u"skipButton"_s)));
        QVERIFY(!isVisible(w.inGuide(u"backButton"_s)));
        // it's shown, and only recorded when it's done
        QVERIFY(!w.settings().setupDone);
    }

    void setupGuideStaysAwayFromThoseWhoUseTheApp()
    {
        // one window at a time: on Debian's Qt 6.8, a second window made beside
        // the first now and then fails on Kirigami's drawer
        {
            Settings settings;
            settings.trade.league.name = u"Standard"_s;
            Window w(true, false, {}, settings);
            QVERIFY(!w.guide()->property("visible").toBool());
            QVERIFY(!w.app->onboarding()->isPending());
        }
        // and the usual window is the same
        Window done;
        QVERIFY(!done.guide()->property("visible").toBool());
    }

    void setupGuideWalksThroughTheSteps()
    {
        Window w(true, false, {}, std::nullopt);
        QObject *guide = w.guide();
        QTRY_VERIFY(guide->property("opened").toBool());
        QObject *next = w.inGuide(u"nextButton"_s);

        QVERIFY(click(next));
        QCOMPARE(guide->property("title").toString(), u"Pick Your League"_s);
        QVERIFY(isVisible(w.inGuide(u"leagueStep"_s)));
        QVERIFY(!isVisible(w.inGuide(u"welcomeStep"_s)));
        QCOMPARE(text(next), u"Next"_s);
        QVERIFY(isVisible(w.inGuide(u"backButton"_s)));

        QVERIFY(click(next));
        QCOMPARE(guide->property("title").toString(), u"Searching the Trade Site"_s);
        QVERIFY(isVisible(w.inGuide(u"tradeStep"_s)));

        QVERIFY(click(next));
        QCOMPARE(guide->property("title").toString(), u"Your Character"_s);
        QVERIFY(isVisible(w.inGuide(u"characterStep"_s)));

        QVERIFY(click(next));
        QCOMPARE(guide->property("title").toString(), u"Pick a Look"_s);
        QVERIFY(isVisible(w.inGuide(u"lookStep"_s)));

        QVERIFY(click(next));
        QCOMPARE(guide->property("title").toString(), u"You're All Set"_s);
        QVERIFY(isVisible(w.inGuide(u"doneStep"_s)));
        QCOMPARE(text(next), u"Finish"_s);
        QVERIFY(!isVisible(w.inGuide(u"skipButton"_s)));
        QCOMPARE(text(w.inGuide(u"hotkeyText"_s)),
                 u"Hover over an item in Path of Exile 2 and press %1."_s.arg(
                     QKeySequence(u"F2"_s).toString(QKeySequence::NativeText)));
        QVERIFY(!w.settings().setupDone);

        QVERIFY(click(next));
        QTRY_VERIFY(!guide->property("visible").toBool());
        QVERIFY(w.settings().setupDone);
        QVERIFY(!w.app->onboarding()->isPending());
        QVERIFY(SettingsStore(w.dir.filePath(u"settings.conf"_s)).settings().setupDone);
    }

    void setupGuideGoesBack()
    {
        Window w(true, false, {}, std::nullopt);
        QTRY_VERIFY(w.guide()->property("opened").toBool());
        QVERIFY(click(w.inGuide(u"nextButton"_s)));
        QVERIFY(click(w.inGuide(u"nextButton"_s)));
        QCOMPARE(w.guide()->property("title").toString(), u"Searching the Trade Site"_s);
        QVERIFY(click(w.inGuide(u"backButton"_s)));
        QCOMPARE(w.guide()->property("title").toString(), u"Pick Your League"_s);
        QVERIFY(click(w.inGuide(u"backButton"_s)));
        QVERIFY(isVisible(w.inGuide(u"welcomeStep"_s)));
        QVERIFY(!isVisible(w.inGuide(u"backButton"_s)));
    }

    void setupGuideSkip()
    {
        Window w(true, false, {}, std::nullopt);
        QObject *guide = w.guide();
        QTRY_VERIFY(guide->property("opened").toBool());
        const Settings before = w.settings();
        QVERIFY(click(w.inGuide(u"skipButton"_s)));
        QTRY_VERIFY(!guide->property("visible").toBool());
        QVERIFY(w.settings().setupDone);
        QVERIFY(!w.app->onboarding()->isPending());
        // nothing else was changed
        QCOMPARE(w.settings().trade, before.trade);
        QCOMPARE(w.settings().character, before.character);
    }

    void setupGuideSkipFromALaterStep()
    {
        Window w(true, false, {}, std::nullopt);
        QObject *guide = w.guide();
        QTRY_VERIFY(guide->property("opened").toBool());
        QVERIFY(click(w.inGuide(u"nextButton"_s)));
        QVERIFY(click(w.inGuide(u"nextButton"_s)));
        QVERIFY(click(w.inGuide(u"skipButton"_s)));
        QTRY_VERIFY(!guide->property("visible").toBool());
        QVERIFY(w.settings().setupDone);
    }

    void setupGuideClosedWithEscape()
    {
        Window w(true, false, {}, std::nullopt);
        QObject *guide = w.guide();
        QTRY_VERIFY(guide->property("opened").toBool());
        QVERIFY(QMetaObject::invokeMethod(guide, "close"));
        QTRY_VERIFY(!guide->property("visible").toBool());
        QVERIFY(w.settings().setupDone);
    }

    void setupGuideLeague()
    {
        Window w(true, false, {}, std::nullopt);
        QTRY_VERIFY(w.guide()->property("opened").toBool());
        // the site's leagues come while trade searches are on
        QSignalSpy refreshed(&w.app->rates(), &RatesStore::refreshed);
        Settings on = w.settings();
        on.trade.enabled = true;
        QVERIFY(w.app->settings().set(on));
        QVERIFY(refreshed.wait());
        QVERIFY(click(w.inGuide(u"nextButton"_s)));

        QObject *league = w.inGuide(u"setupLeagueBox"_s);
        QCOMPARE(league->property("count").toInt(), 3);
        QVERIFY(activate(league, 2));
        QCOMPARE(w.settings().trade.league.name, u"Standard"_s);

        QVERIFY(toggle(w.inGuide(u"setupHardcoreSwitch"_s)));
        QVERIFY(w.settings().trade.league.hardcore);
        QVERIFY(toggle(w.inGuide(u"setupSoloSwitch"_s)));
        QVERIFY(w.settings().trade.league.soloSelfFound);
        QVERIFY(w.inGuide(u"setupSoloSwitch"_s)
                    ->property("description")
                    .toString()
                    .contains(u"turns trade searches off"_s));
        // the same as the Trade page has
        QVERIFY(w.show(u"tradeAction"_s));
        QVERIFY(w.find(u"soloSwitch"_s)->property("checked").toBool());
        QVERIFY(w.find(u"hardcoreSwitch"_s)->property("checked").toBool());
    }

    void setupGuideLeavesOutTradeInSoloSelfFound()
    {
        Window w(true, false, {}, std::nullopt);
        QObject *guide = w.guide();
        QTRY_VERIFY(guide->property("opened").toBool());
        QObject *next = w.inGuide(u"nextButton"_s);
        QVERIFY(click(next));
        QVERIFY(toggle(w.inGuide(u"setupSoloSwitch"_s)));

        QVERIFY(click(next));
        QCOMPARE(guide->property("title").toString(), u"Your Character"_s);
        QVERIFY(!isVisible(w.inGuide(u"tradeStep"_s)));
        QVERIFY(click(w.inGuide(u"backButton"_s)));
        QCOMPARE(guide->property("title").toString(), u"Pick Your League"_s);

        // and back in when it's turned off
        QVERIFY(toggle(w.inGuide(u"setupSoloSwitch"_s)));
        QVERIFY(click(next));
        QCOMPARE(guide->property("title").toString(), u"Searching the Trade Site"_s);
        QVERIFY(isVisible(w.inGuide(u"tradeStep"_s)));
    }

    void setupGuideTradeSwitch()
    {
        Window w(true, false, {}, std::nullopt);
        QTRY_VERIFY(w.guide()->property("opened").toBool());
        QVERIFY(click(w.inGuide(u"nextButton"_s)));
        QVERIFY(click(w.inGuide(u"nextButton"_s)));
        QObject *trade = w.inGuide(u"setupTradeSwitch"_s);
        const bool before = w.settings().trade.enabled;
        QCOMPARE(trade->property("checked").toBool(), before);
        QVERIFY(toggle(trade));
        QCOMPARE(w.settings().trade.enabled, !before);
        QVERIFY(toggle(trade));
        QCOMPARE(w.settings().trade.enabled, before);
    }

    void setupGuideSavesTheSessionId()
    {
        Window w(true, false, {}, std::nullopt);
        QTRY_VERIFY(w.guide()->property("opened").toBool());
        QVERIFY(click(w.inGuide(u"nextButton"_s)));
        QVERIFY(click(w.inGuide(u"nextButton"_s)));
        // a beta asks the keyring at the start, and the field waits for it
        QVERIFY(w.keyringAnswered());
        // nothing is in the keyring until you type one
        QCOMPARE(w.keyring->writes, 0);
        QObject *field = w.inGuide(u"setupSessionField"_s);
        QObject *save = w.inGuide(u"setupSaveButton"_s);
        QVERIFY(!save->property("enabled").toBool());
        QVERIFY(field->setProperty("text", fakeId));
        QVERIFY(save->property("enabled").toBool());
        QVERIFY(click(save));
        QVERIFY(w.keyringAnswered());
        QCOMPARE(w.keyring->secrets.value(u"POESESSID"_s), fakeId);
        QCOMPARE(text(w.inGuide(u"setupSessionStatus"_s)), u"A session ID is saved."_s);
        // which isn't shown again
        QCOMPARE(text(field), QString());
    }

    void setupGuideSessionIdRefused()
    {
        Window w(true, false, {}, std::nullopt);
        QTRY_VERIFY(w.guide()->property("opened").toBool());
        QVERIFY(click(w.inGuide(u"nextButton"_s)));
        QVERIFY(click(w.inGuide(u"nextButton"_s)));
        QVERIFY(w.keyringAnswered());
        w.keyring->failure = u"The keyring is locked."_s;
        QVERIFY(w.inGuide(u"setupSessionField"_s)->setProperty("text", fakeId));
        QVERIFY(click(w.inGuide(u"setupSaveButton"_s)));
        QVERIFY(w.keyringAnswered());
        QVERIFY(isVisible(w.inGuide(u"setupSessionError"_s)));
        QCOMPARE(text(w.inGuide(u"setupSessionError"_s)), u"The keyring is locked."_s);
    }

    void setupGuideAddsACharacter()
    {
        Window w(true, false, {}, std::nullopt);
        QTRY_VERIFY(w.guide()->property("opened").toBool());
        for (int i = 0; i < 3; ++i)
            QVERIFY(click(w.inGuide(u"nextButton"_s)));
        QCOMPARE(w.guide()->property("title").toString(), u"Your Character"_s);

        QObject *add = w.inGuide(u"setupAddButton"_s);
        QObject *name = w.inGuide(u"setupNameField"_s);
        QVERIFY(!add->property("enabled").toBool());
        QVERIFY(name->setProperty("text", u" Wren "_s));
        QVERIFY(click(add));
        QCOMPARE(w.app->characterList()->rowCount(), 1);
        QCOMPARE(w.settings().character, w.app->characterList()->current());
        QCOMPARE(text(name), QString());
        QVERIFY(w.inGuide(u"setupCharacter:Wren"_s));
    }

    void setupGuideImportsABuild()
    {
        Window w(true, false, {}, std::nullopt);
        QTRY_VERIFY(w.guide()->property("opened").toBool());
        for (int i = 0; i < 3; ++i)
            QVERIFY(click(w.inGuide(u"nextButton"_s)));
        QObject *code = w.inGuide(u"setupCodeField"_s);
        QVERIFY(code->setProperty("text", ranger));
        QVERIFY(click(w.inGuide(u"setupImportButton"_s)));
        QCOMPARE(text(w.inGuide(u"setupImportMessage"_s)),
                 u"Imported the level 40 Ranger build for Ranger. The build has no items."_s);
        QCOMPARE(text(code), QString());
        QCOMPARE(w.inGuide(u"setupCharacter:Ranger"_s)->property("description").toString(),
                 u"Level 40 Ranger"_s);
    }

    void setupGuideImportFails()
    {
        Window w(true, false, {}, std::nullopt);
        QTRY_VERIFY(w.guide()->property("opened").toBool());
        for (int i = 0; i < 3; ++i)
            QVERIFY(click(w.inGuide(u"nextButton"_s)));
        QObject *code = w.inGuide(u"setupCodeField"_s);
        QVERIFY(code->setProperty("text", u"nonsense"_s));
        QVERIFY(QMetaObject::invokeMethod(code, "accepted"));
        QCOMPARE(text(w.inGuide(u"setupImportMessage"_s)),
                 u"That doesn't look like a Path of Building code or a link to one."_s);
        QCOMPARE(text(code), u"nonsense"_s);
        QCOMPARE(w.app->characterList()->rowCount(), 0);
        // and it can be left for later
        QVERIFY(click(w.inGuide(u"nextButton"_s)));
        QVERIFY(isVisible(w.inGuide(u"lookStep"_s)));
        QVERIFY(click(w.inGuide(u"nextButton"_s)));
        QVERIFY(isVisible(w.inGuide(u"doneStep"_s)));
    }

    void setupGuideListsSavedBuilds()
    {
        Window w(true, false, {}, std::nullopt);
        QTRY_VERIFY(w.guide()->property("opened").toBool());
        const QString folder = w.pob.folder.path();
        QVERIFY(makePob(folder));
        w.app->pobOptions()->setLocation(folder);
        QVERIFY(writeFile(w.app->buildImporter()->buildsFolder() + u"/Leveling.xml"_s, ranger.toUtf8()));
        for (int i = 0; i < 3; ++i)
            QVERIFY(click(w.inGuide(u"nextButton"_s)));
        QObject *saved = w.inGuide(u"setupSaved:Leveling"_s);
        QVERIFY(saved);
        QVERIFY(click(saved));
        QCOMPARE(text(w.inGuide(u"setupImportMessage"_s)),
                 u"Imported the level 40 Ranger build for Leveling. The build has no items."_s);
    }

    void setupGuideShowsTheHotkeyItHas()
    {
        Window w(true, false, {}, std::nullopt);
        QTRY_VERIFY(w.guide()->property("opened").toBool());
        QVERIFY(w.app->overlayOptions()->setCheckKey(QKeySequence(u"F7"_s)));
        for (int i = 0; i < 4; ++i)
            QVERIFY(click(w.inGuide(u"nextButton"_s)));
        QCOMPARE(text(w.inGuide(u"hotkeyText"_s)),
                 u"Hover over an item in Path of Exile 2 and press %1."_s.arg(
                     QKeySequence(u"F7"_s).toString(QKeySequence::NativeText)));
    }

    void setupGuideSaysWhenThereIsNoHotkey()
    {
        Window w(false, false, {}, std::nullopt);
        QTRY_VERIFY(w.guide()->property("opened").toBool());
        for (int i = 0; i < 4; ++i)
            QVERIFY(click(w.inGuide(u"nextButton"_s)));
        QCOMPARE(text(w.inGuide(u"hotkeyText"_s)),
                 u"You haven't set a key to check an item. Choose one on the General page."_s);
    }

    // as --setup asks
    void setupGuideComesBack()
    {
        Window w;
        QObject *guide = w.guide();
        QVERIFY(!guide->property("visible").toBool());
        w.app->onboarding()->ask();
        QTRY_VERIFY(guide->property("opened").toBool());
        QVERIFY(isVisible(w.inGuide(u"welcomeStep"_s)));
        QVERIFY(click(w.inGuide(u"nextButton"_s)));
        QVERIFY(click(w.inGuide(u"skipButton"_s)));
        QTRY_VERIFY(!guide->property("visible").toBool());

        // from the start again
        w.app->onboarding()->ask();
        QTRY_VERIFY(guide->property("opened").toBool());
        QVERIFY(isVisible(w.inGuide(u"welcomeStep"_s)));
    }

    // The release GitHub will say is out, for the next look.
    static void releaseIsOut(Window &w, const QString &version, bool tradeAsked = true)
    {
        // the trade site's and poe.ninja's answers go first
        if (tradeAsked)
            QTRY_VERIFY_WITH_TIMEOUT(w.network->answers.isEmpty(), 5000);
        w.network->answers = {{.body = fakeReleases({fakeRelease(version)})}};
        w.app->updateChecker().check();
        QTRY_VERIFY_WITH_TIMEOUT(!w.app->updateChecker().isChecking(), 5000);
    }

    void updateDialogOffersTheRelease()
    {
        Window w;
        QObject *dialog = w.window->findChild<QObject *>(u"updateDialog"_s);
        QVERIFY(!isVisible(dialog));
        releaseIsOut(w, u"99.0.0"_s);
        QTRY_VERIFY(dialog->property("opened").toBool());
        QCOMPARE(dialog->property("title").toString(), u"There's a New Version"_s);
        QVERIFY(button(dialog, u"Update Now"_s));
        QVERIFY(button(dialog, u"Later"_s));
        QVERIFY(button(dialog, u"Skip This Version"_s));
        // a line of notes needs no bar
        QVERIFY(isVisible(inDialog(dialog, u"updateNotes"_s)));
        QVERIFY(!isVisible(inDialog(dialog, u"updateNotesBar"_s)));

        QVERIFY(click(button(dialog, u"Update Now"_s)));
        QCOMPARE(w.opener->opened, QList<QUrl>{QUrl(releaseBase + u"/tag/v99.0.0"_s)});
        QTRY_VERIFY(!isVisible(dialog));
    }

    // Long notes scroll, with the bar up and the text clear of it, in line
    // with the text above.
    void updateDialogNotesScroll()
    {
        Window w;
        QTRY_VERIFY_WITH_TIMEOUT(w.network->answers.isEmpty(), 5000);
        QJsonObject release = fakeRelease(u"99.0.0"_s);
        QStringList lines;
        for (int i = 1; i <= 80; ++i)
            lines << u"Line %1 of what is new."_s.arg(i);
        release[u"body"_s] = lines.join(u'\n');
        w.network->answers = {{.body = fakeReleases({release})}};
        w.app->updateChecker().check();
        QObject *dialog = w.window->findChild<QObject *>(u"updateDialog"_s);
        QTRY_VERIFY_WITH_TIMEOUT(dialog->property("opened").toBool(), 5000);

        QObject *notes = inDialog(dialog, u"updateNotes"_s);
        QObject *bar = inDialog(dialog, u"updateNotesBar"_s);
        QObject *text = inDialog(dialog, u"updateNotesText"_s);
        QObject *intro = inDialog(dialog, u"updateIntro"_s);
        QVERIFY(notes && bar && text && intro);
        QTRY_VERIFY(isVisible(bar));
        QVERIFY(text->property("implicitHeight").toDouble() > notes->property("height").toDouble());
        QVERIFY(text->property("rightPadding").toDouble() >= bar->property("width").toDouble());
        QVERIFY(intro->property("leftPadding").toDouble() > 0);
        QCOMPARE(notes->property("x").toDouble(),
                 intro->property("x").toDouble() + intro->property("leftPadding").toDouble());
    }

    void installDialogShowsTheCommand()
    {
        Window w;
        w.app->updates()->setInstallKind(InstallKind::Debian);
        QObject *install = w.window->findChild<QObject *>(u"installDialog"_s);
        QVERIFY(!isVisible(install));
        releaseIsOut(w, u"99.0.0"_s);
        QObject *dialog = w.window->findChild<QObject *>(u"updateDialog"_s);
        QTRY_VERIFY(dialog->property("opened").toBool());

        const Release release = readReleases(fakeReleases({fakeRelease(u"99.0.0"_s)})).first();
        const QByteArray package = "pretend this is a package";
        const QByteArray sum = QCryptographicHash::hash(package, QCryptographicHash::Sha256).toHex();
        w.network->answers = {
            {.body = sum + "  " + assetFor(release, InstallKind::Debian)->name.toUtf8() + "\n"},
            {.body = package}};
        QVERIFY(click(button(dialog, u"Update Now"_s)));
        QTRY_VERIFY_WITH_TIMEOUT(install->property("opened").toBool(), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(w.app->updates()->stage(), UpdateStage::Manual, 5000);
        QVERIFY(w.opener->opened.isEmpty());

        QObject *command = w.window->findChild<QObject *>(u"installCommand"_s);
        QVERIFY(command);
        QVERIFY(command->property("text").toString().startsWith(u"sudo apt install '"_s));
        QVERIFY(button(install, u"Close"_s));

        QObject *copy = w.window->findChild<QObject *>(u"copyCommandButton"_s);
        QVERIFY(click(copy));
        QCOMPARE(w.opener->copied, QStringList{w.app->updates()->command()});
        QObject *folder = w.window->findChild<QObject *>(u"openFolderButton"_s);
        QVERIFY(click(folder));
        QCOMPARE(w.opener->opened.size(), 1);
        QVERIFY(w.opener->opened.at(0).isLocalFile());

        QVERIFY(click(button(install, u"Close"_s)));
        QTRY_VERIFY(!isVisible(install));
        QCOMPARE(w.app->updates()->stage(), UpdateStage::Idle);
    }

    void installDialogRestartsAfterAPackage()
    {
        // an installer that says it worked
        class Installed : public PackageInstaller
        {
        public:
            After after() const override { return After::Restart; }
            void install(const QString &, const Done &done) override { done({}); }
        };
        Window w;
        w.app->updates()->setInstallKind(InstallKind::Debian);
        // this is the dialog's own Update Now, not the app taking it by itself
        w.app->updates()->setInstallByItself(false);
        QList<PackageInstaller::After> finished;
        w.app->setUpdateInstaller(std::make_unique<Installed>(), [&](PackageInstaller::After a) {
            finished << a;
            return true;
        });
        releaseIsOut(w, u"99.0.0"_s);
        QObject *dialog = w.window->findChild<QObject *>(u"updateDialog"_s);
        QTRY_VERIFY(dialog->property("opened").toBool());

        const Release release = readReleases(fakeReleases({fakeRelease(u"99.0.0"_s)})).first();
        const QByteArray package = "pretend this is a package";
        const QByteArray sum = QCryptographicHash::hash(package, QCryptographicHash::Sha256).toHex();
        w.network->answers = {
            {.body = sum + "  " + assetFor(release, InstallKind::Debian)->name.toUtf8() + "\n"},
            {.body = package}};
        QVERIFY(w.show(u"updatesAction"_s));
        QVERIFY(click(button(dialog, u"Update Now"_s)));
        // with nothing to press
        QTRY_COMPARE_WITH_TIMEOUT(finished, QList<PackageInstaller::After>{PackageInstaller::After::Restart},
                                  5000);
        QCOMPARE(w.settings().updates.reopenPage, u"updatesAction"_s);
    }

    // after an update restarted the app
    void windowOpensWhereTheUpdateLeftIt()
    {
        Settings saved = used();
        saved.updates.reopenPage = u"appearanceAction"_s;
        Window w(true, false, {}, saved);
        QTRY_VERIFY(w.window->findChild<QObject *>(u"appearanceAction"_s)->property("checked").toBool());
        QCOMPARE(w.app->updates()->shownPage(), u"appearanceAction"_s);
        QVERIFY(w.settings().updates.reopenPage.isEmpty());
        QVERIFY(w.show(u"gearAction"_s));
        QCOMPARE(w.app->updates()->shownPage(), u"gearAction"_s);
    }

    void updateDialogLaterAndSkip()
    {
        Window w;
        QObject *dialog = w.window->findChild<QObject *>(u"updateDialog"_s);
        releaseIsOut(w, u"99.0.0"_s);
        QTRY_VERIFY(dialog->property("opened").toBool());
        QVERIFY(click(button(dialog, u"Later"_s)));
        QTRY_VERIFY(!isVisible(dialog));
        QVERIFY(w.settings().updates.skipped.isEmpty());
        QVERIFY(w.opener->opened.isEmpty());

        // Check Now offers it again, and Skip keeps it from coming back
        w.network->answers = {{.body = fakeReleases({fakeRelease(u"99.0.0"_s)})}};
        w.app->updates()->checkNow();
        QTRY_VERIFY_WITH_TIMEOUT(!w.app->updateChecker().isChecking(), 5000);
        QTRY_VERIFY(dialog->property("opened").toBool());
        QVERIFY(click(button(dialog, u"Skip This Version"_s)));
        QTRY_VERIFY(!isVisible(dialog));
        QCOMPARE(w.settings().updates.skipped, u"99.0.0"_s);
        QVERIFY(w.opener->opened.isEmpty());
    }

    void updateDialogWaitsForTheWindowAndTheSetup()
    {
        Window w(true, true, {}, std::nullopt);
        QObject *dialog = w.window->findChild<QObject *>(u"updateDialog"_s);
        // a first run doesn't ask the trade site
        releaseIsOut(w, u"99.0.0"_s, false);
        QVERIFY(w.app->updates()->isPending());
        QVERIFY(!isVisible(dialog));
        w.window->show();
        // the setup guide has the window first
        QTRY_VERIFY(w.guide()->property("opened").toBool());
        QVERIFY(!dialog->property("opened").toBool());
        QVERIFY(click(w.inGuide(u"skipButton"_s)));
        QTRY_VERIFY(dialog->property("opened").toBool());
    }

    void updatesPageInTheSettings()
    {
        Window w;
        QObject *action = w.window->findChild<QObject *>(u"updatesAction"_s);
        QCOMPARE(text(action), u"Updates"_s);
        // after Appearance, and not on General
        QVERIFY(w.show(u"overlayAction"_s));
        QVERIFY(!w.page()->findChild<QObject *>(u"checkForUpdatesSwitch"_s));
        QVERIFY(!w.page()->findChild<QObject *>(u"checkNowButton"_s));
        QVERIFY(w.show(u"updatesAction"_s));
        QObject *launch = w.find(u"checkForUpdatesSwitch"_s);
        QObject *betas = w.find(u"includeBetasSwitch"_s);
        QCOMPARE(launch->property("checked").toBool(), true);
        QCOMPARE(launch->property("text").toString(), u"Check for updates when the app starts"_s);
        QCOMPARE(betas->property("text").toString(), u"Include beta builds"_s);
        QCOMPARE(betas->property("checked").toBool(), w.settings().updates.includeBetas);

        QVERIFY(toggle(launch));
        QVERIFY(!w.settings().updates.checkOnLaunch);
        QVERIFY(toggle(betas));
        QCOMPARE(w.settings().updates.includeBetas, betas->property("checked").toBool());

        QObject *itself = w.find(u"installByItselfSwitch"_s);
        QCOMPARE(itself->property("checked").toBool(), true);
        QVERIFY(toggle(itself));
        QVERIFY(!w.settings().updates.installByItself);

        QObject *now = w.find(u"checkNowButton"_s);
        QCOMPARE(text(now), u"Check Now"_s);
        QTRY_VERIFY_WITH_TIMEOUT(w.network->answers.isEmpty(), 5000);
        w.network->answers = {{.body = fakeReleases({fakeRelease(u"0.0.1"_s)})}};
        QVERIFY(click(now));
        QTRY_VERIFY_WITH_TIMEOUT(w.app->updates()->status() == u"You're on the newest version."_s, 5000);
    }

    // in the background, as it is at login
    void setupGuideWaitsForTheWindow()
    {
        Window w(true, true, {}, std::nullopt);
        QObject *guide = w.guide();
        QVERIFY(w.app->onboarding()->isPending());
        QVERIFY(!guide->property("visible").toBool());
        w.window->show();
        QTRY_VERIFY(guide->property("opened").toBool());
        QVERIFY(isVisible(w.inGuide(u"welcomeStep"_s)));
    }

    void setupGuideAskedWhileHidden()
    {
        Window w(true, true);
        QVERIFY(!w.app->onboarding()->isPending());
        w.app->onboarding()->ask();
        QVERIFY(!w.guide()->property("visible").toBool());
        w.window->show();
        QTRY_VERIFY(w.guide()->property("opened").toBool());
    }
};

QTEST_MAIN(TestMainWindowGuide)
#include "tst_mainwindow_guide.moc"
