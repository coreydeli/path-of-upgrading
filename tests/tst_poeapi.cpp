// SPDX-License-Identifier: MIT
#include "core/poeapi.h"

#include <QTest>

using namespace Qt::Literals::StringLiterals;
using namespace pou;
using Kind = PoeFailure::Kind;

class TestPoeApi : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void addresses()
    {
        QCOMPARE(charactersUrl().toString(), u"https://api.pathofexile.com/character/poe2"_s);
        QCOMPARE(characterUrl(u"Made Up"_s).toString(QUrl::FullyEncoded),
                 u"https://api.pathofexile.com/character/poe2/Made%20Up"_s);
        // a name with letters outside ASCII is sent as UTF-8
        QCOMPARE(characterUrl(u"Zoë"_s).toString(QUrl::FullyEncoded),
                 u"https://api.pathofexile.com/character/poe2/Zo%C3%AB"_s);
    }

    void listsCharacters()
    {
        const auto list = readCharacterList(
            R"({"characters":[)"
            R"({"id":"aa","name":"FakeOne","class":"Witch","league":"Standard","level":42,"current":true},)"
            R"({"id":"bb","name":"FakeTwo","class":"Ranger","league":"HC Fake League","level":7}]})");
        QCOMPARE(list.size(), 2);
        QCOMPARE(list[0].id, u"aa"_s);
        QCOMPARE(list[0].name, u"FakeOne"_s);
        QCOMPARE(list[0].className, u"Witch"_s);
        QCOMPARE(list[0].league, u"Standard"_s);
        QCOMPARE(list[0].level, 42);
        QVERIFY(list[0].current);
        QCOMPARE(list[1].league, u"HC Fake League"_s);
        QVERIFY(!list[1].current);
    }

    void leavesOutDeletedAndExpired()
    {
        const auto list =
            readCharacterList(R"({"characters":[{"name":"Kept","level":1},{"name":"Gone","deleted":true},)"
                              R"({"name":"Old","expired":true},{"name":"","level":3},{"level":4}]})");
        QCOMPARE(list.size(), 1);
        QCOMPARE(list[0].name, u"Kept"_s);
    }

    void listsNothingFromJunk()
    {
        QVERIFY(readCharacterList("not json").isEmpty());
        QVERIFY(readCharacterList("{}").isEmpty());
        QVERIFY(readCharacterList(R"({"characters":"x"})").isEmpty());
    }

    void readsOneCharacter()
    {
        const auto one = readCharacter(R"({"character":{"name":"FakeOne","level":9,"equipment":[]}})");
        QVERIFY(one);
        QCOMPARE(one->value(u"name"_s).toString(), u"FakeOne"_s);
        QVERIFY(one->contains(u"equipment"_s));
        QVERIFY(!readCharacter(R"({"character":{"level":9}})"));
        QVERIFY(!readCharacter(R"({"error":{"code":1,"message":"x"}})"));
        QVERIFY(!readCharacter({}));
    }

    void retryAfter()
    {
        QCOMPARE(readRetryAfter(u"37"), 37);
        QCOMPARE(readRetryAfter(u" 5 "), 5);
        QCOMPARE(readRetryAfter(u""), 0);
        QCOMPARE(readRetryAfter(u"0"), 0);
        QCOMPARE(readRetryAfter(u"-3"), 0);
        // a date isn't a number of seconds
        QCOMPARE(readRetryAfter(u"Wed, 21 Oct 2026 07:28:00 GMT"), 0);
    }

    void rateLimited()
    {
        const PoeFailure failure =
            poeFailure(429, R"({"error":{"code":3,"message":"Rate limit exceeded"}})", u"30");
        QCOMPARE(failure.kind, Kind::RateLimited);
        QCOMPARE(failure.waitSeconds, 30);
        QCOMPARE(failure.text, u"The website says to wait 30 seconds before asking again."_s);
        QCOMPARE(poeFailure(429, {}, u"1").text, u"The website says to wait 1 second before asking again."_s);
        // no time given
        const PoeFailure unknown = poeFailure(429, {}, u"");
        QCOMPARE(unknown.kind, Kind::RateLimited);
        QCOMPARE(unknown.waitSeconds, 0);
        QCOMPARE(unknown.text, u"The website says to wait before asking again."_s);
        // the error code says it even when the status doesn't
        QCOMPARE(poeFailure(400, R"({"error":{"code":3}})", u"9").kind, Kind::RateLimited);
    }

    void signedOut()
    {
        const PoeFailure failure = poeFailure(401, R"({"error":{"code":1,"message":"Unauthorized"}})", u"");
        QCOMPARE(failure.kind, Kind::SignedOut);
        QCOMPARE(failure.text, u"Your link ran out. Link your account again."_s);
    }

    void refused() { QCOMPARE(poeFailure(403, {}, u"").kind, Kind::Refused); }

    void notFound()
    {
        const PoeFailure failure = poeFailure(404, {}, u"");
        QCOMPARE(failure.kind, Kind::NotFound);
        QCOMPARE(failure.text, u"That character isn't on your account any more."_s);
    }

    void unavailable()
    {
        const QString text = u"The website isn't answering properly right now. Try again in a minute."_s;
        QCOMPARE(poeFailure(503, {}, u"").text, text);
        QCOMPARE(poeFailure(500, {}, u"").kind, Kind::Unavailable);
        QCOMPARE(poeFailure(400, R"({"error":{"code":7}})", u"").kind, Kind::Unavailable);
    }

    void anythingElse()
    {
        const PoeFailure failure = poeFailure(418, "not json", u"");
        QCOMPARE(failure.kind, Kind::Other);
        QVERIFY(!failure.text.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestPoeApi)
#include "tst_poeapi.moc"
