// SPDX-License-Identifier: MIT
#include "app/accountcharacters.h"
#include "app/poeaccount.h"
#include "fakenetwork.h"
#include "fakesecretstore.h"
#include "services/poeauth.h"
#include "services/poeclient.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
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

// A linked account whose website answers the token address with a token and
// the API with what's in `characters`.
struct Linked
{
    FakeSecretStore store;
    FakeNetwork network;
    PoeAuth auth{network, config};
    PoeAccount account{true, &store, &auth};
    PoeClient client{&network, config, &account};
    AccountCharacters characters{&client, &account};
    FakeAnswer characterAnswer{.body = list};
    int asked = 0;

    explicit Linked(bool linked = true)
    {
        const QDateTime now = QDateTime::currentDateTimeUtc();
        account.setClock([now] { return now; });
        network.route = [this](const QNetworkRequest &request) -> std::optional<FakeAnswer> {
            if (request.url().host() == u"api.pathofexile.com"_s) {
                ++asked;
                return characterAnswer;
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
