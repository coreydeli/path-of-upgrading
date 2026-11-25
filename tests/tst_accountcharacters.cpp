// SPDX-License-Identifier: MIT
#include "app/accountcharacters.h"
#include "app/activecharacter.h"
#include "app/characterlist.h"
#include "app/poeaccount.h"
#include "app/settingsstore.h"
#include "fakenetwork.h"
#include "fakesecretstore.h"
#include "services/characterstore.h"
#include "services/pobengine.h"
#include "services/poeauth.h"
#include "services/poeclient.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTimeZone>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

// made-up
const PoeConfig config{.clientId = u"examplepou"_s, .contact = u"example.invalid"_s, .ports = {0}};

const QByteArray list =
    R"({"characters":[)"
    R"({"id":"aa","name":"FakeOne","class":"Stormweaver","league":"Standard","level":92,"current":true},)"
    R"({"id":"bb","name":"FakeTwo","class":"Ranger","league":"HC Fake League","level":7},)"
    R"({"id":"cc","name":"FakeGone","class":"Witch","level":3,"deleted":true}]})";

// what FakeOne wears, made up
const QByteArray wearing =
    R"({"character":{"id":"aa","name":"FakeOne","class":"Stormweaver","level":92,"equipment":[)"
    R"({"inventoryId":"Helm","name":"Fake Crown","typeLine":"Iron Hat","baseType":"Iron Hat","rarity":"Rare",)"
    R"("explicitMods":["+45 to maximum Life"]},)"
    R"({"inventoryId":"Belt","name":"Fake Sash","typeLine":"Leather Belt","baseType":"Leather Belt",)"
    R"("rarity":"Rare","explicitMods":["+30 to maximum Life"]}]}})";

// A linked account whose website answers the token address with a token and
// the API with what's in `characters`.
struct Linked
{
    FakeSecretStore store;
    FakeNetwork network;
    PoeAuth auth{network, config};
    PoeAccount account{true, &store, &auth};
    PoeClient client{&network, config, &account};
    QTemporaryDir dir;
    SettingsStore settings{dir.filePath(u"settings.conf"_s)};
    CharacterStore saved{dir.filePath(u"characters"_s)};
    PobEngine engine;
    ActiveCharacter active{&saved, &engine};
    CharacterList yours{&saved, &active, &settings};
    AccountCharacters characters{&client, &account, &yours};
    FakeAnswer characterAnswer{.body = list};
    FakeAnswer oneAnswer{.body = wearing};
    int asked = 0;

    explicit Linked(bool linked = true)
    {
        const QDateTime now = QDateTime::currentDateTimeUtc();
        account.setClock([now] { return now; });
        network.route = [this](const QNetworkRequest &request) -> std::optional<FakeAnswer> {
            if (request.url().host() == u"api.pathofexile.com"_s) {
                ++asked;
                return request.url().path().count(u'/') > 2 ? oneAnswer : characterAnswer;
            }
            const QByteArray body = QJsonDocument(QJsonObject{{u"access_token"_s, u"fake-access"_s},
                                                              {u"refresh_token"_s, u"fake-refresh-2"_s},
                                                              {u"expires_in"_s, 36000},
                                                              {u"token_type"_s, u"bearer"_s}})
                                        .toJson(QJsonDocument::Compact);
            return FakeAnswer{.body = body};
        };
        if (linked) {
            const QJsonObject kept{{u"refresh"_s, u"fake-refresh"_s},
                                   {u"until"_s, now.addDays(3).toString(Qt::ISODate)}};
            store.secrets.insert(PoeAccount::secretName(),
                                 QString::fromUtf8(QJsonDocument(kept).toJson(QJsonDocument::Compact)));
            account.load();
            QTRY_VERIFY(account.isLinked());
            QTRY_VERIFY(!account.isBusy());
        }
    }
};

} // namespace

class TestAccountCharacters : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init() { QTest::failOnWarning(); }

    void startsEmpty()
    {
        Linked l;
        QCOMPARE(l.characters.rowCount(), 0);
        QVERIFY(!l.characters.isLoaded());
        QVERIFY(!l.characters.isLoading());
        QVERIFY(l.characters.error().isEmpty());
    }

    void listsTheCharactersWithoutTheDeletedOnes()
    {
        Linked l;
        l.characters.refresh();
        QVERIFY(l.characters.isLoading());
        QTRY_VERIFY(l.characters.isLoaded());
        QVERIFY(!l.characters.isLoading());
        QCOMPARE(l.characters.rowCount(), 2);
        const auto at = [&l](int row, int role) {
            return l.characters.index(row).data(role);
        };
        QCOMPARE(at(0, AccountCharacters::NameRole).toString(), u"FakeOne"_s);
        QCOMPARE(at(0, AccountCharacters::SummaryRole).toString(), u"Level 92 Stormweaver, Standard"_s);
        QCOMPARE(at(0, AccountCharacters::CurrentRole).toBool(), true);
        // every league is there, as the website gave them
        QCOMPARE(at(1, AccountCharacters::SummaryRole).toString(), u"Level 7 Ranger, HC Fake League"_s);
        QCOMPARE(l.characters.at(1).id, u"bb"_s);
        QCOMPARE(l.characters.at(5).name, QString());
    }

    void anAccountWithNoCharactersIsLoadedAndEmpty()
    {
        Linked l;
        l.characterAnswer.body = R"({"characters":[]})";
        l.characters.refresh();
        QTRY_VERIFY(l.characters.isLoaded());
        QCOMPARE(l.characters.rowCount(), 0);
        QVERIFY(l.characters.error().isEmpty());
    }

    void asksOnceWhileItIsAsking()
    {
        Linked l;
        l.characters.refresh();
        l.characters.refresh();
        QTRY_VERIFY(l.characters.isLoaded());
        QCOMPARE(l.asked, 1);
    }

    void asksNothingWithoutALink()
    {
        Linked l(false);
        l.characters.refresh();
        QTest::qWait(50);
        QVERIFY(!l.characters.isLoading());
        QCOMPARE(l.network.sent.size(), 0);
    }

    void sayHowLongToWait()
    {
        Linked l;
        l.characterAnswer = FakeAnswer{.status = 429, .headers = {{"Retry-After", "45"}}};
        l.characters.refresh();
        QTRY_VERIFY(!l.characters.isLoading());
        QVERIFY(!l.characters.isLoaded());
        QVERIFY(l.characters.error().contains(u"45 seconds"_s));
        QCOMPARE(l.asked, 1);
    }

    void aFailedAskKeepsTheLastList()
    {
        Linked l;
        l.characters.refresh();
        QTRY_VERIFY(l.characters.isLoaded());
        l.characterAnswer = FakeAnswer{.status = 503};
        l.characters.refresh();
        QTRY_VERIFY(!l.characters.error().isEmpty());
        QCOMPARE(l.characters.rowCount(), 2);
    }

    void importsACharacterAndSaysWhatItWears()
    {
        Linked l;
        l.yours.load();
        l.characters.refresh();
        QTRY_VERIFY(l.characters.isLoaded());
        QVERIFY(!l.characters.index(0).data(AccountCharacters::ImportedRole).toBool());
        QSignalSpy imported(&l.characters, &AccountCharacters::imported);
        l.characters.importCharacter(0);
        QVERIFY(l.characters.isImporting());
        QTRY_VERIFY(!l.characters.isImporting());
        QCOMPARE(imported.size(), 1);
        QCOMPARE(l.characters.message(), u"FakeOne now wears 2 items from the game."_s);
        // one request for the list, one for the character
        QCOMPARE(l.asked, 2);
        const QString id = l.yours.linkedTo(u"aa"_s);
        QVERIFY(!id.isEmpty());
        QCOMPARE(imported.first().first().toString(), id);
        QCOMPARE(l.yours.find(id)->gear.size(), 2);
        QVERIFY(l.characters.index(0).data(AccountCharacters::ImportedRole).toBool());
        QVERIFY(!l.characters.index(1).data(AccountCharacters::ImportedRole).toBool());

        // and again updates the same one
        l.oneAnswer.body = R"({"character":{"id":"aa","name":"FakeOne","level":93,"equipment":[]}})";
        l.characters.importCharacter(0);
        QTRY_VERIFY(!l.characters.isImporting());
        QCOMPARE(l.characters.message(), u"FakeOne now wears 0 items from the game."_s);
        QCOMPARE(l.yours.count(), 1);
        QCOMPARE(l.yours.find(id)->gear.size(), 0);
    }

    void importingSaysWhyItFailed()
    {
        Linked l;
        l.yours.load();
        l.characters.refresh();
        QTRY_VERIFY(l.characters.isLoaded());
        l.oneAnswer =
            FakeAnswer{.status = 404, .body = R"({"error":{"code":2,"message":"Resource not found"}})"};
        l.characters.importCharacter(0);
        QTRY_VERIFY(!l.characters.isImporting());
        QCOMPARE(l.characters.message(), u"That character isn't on your account any more."_s);
        QCOMPARE(l.yours.count(), 0);

        l.oneAnswer = FakeAnswer{.body = R"({"character":null})"};
        l.characters.importCharacter(0);
        QTRY_VERIFY(!l.characters.isImporting());
        QCOMPARE(l.characters.message(), u"FakeOne isn't on your account any more."_s);
        QCOMPARE(l.yours.count(), 0);
    }

    void importsNothingForARowThatIsntThere()
    {
        Linked l;
        l.characters.importCharacter(0);
        l.characters.importCharacter(-1);
        QVERIFY(!l.characters.isImporting());
        QCOMPARE(l.network.sent.size(), 0);
    }

    void unlinkingEmptiesTheList()
    {
        Linked l;
        l.characters.refresh();
        QTRY_VERIFY(l.characters.isLoaded());
        QTRY_VERIFY(!l.account.isBusy());
        l.account.unlink();
        QTRY_COMPARE(l.characters.rowCount(), 0);
        QVERIFY(!l.characters.isLoaded());
    }
};

QTEST_GUILESS_MAIN(TestAccountCharacters)
#include "tst_accountcharacters.moc"
