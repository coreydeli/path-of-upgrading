// SPDX-License-Identifier: MIT
#include "core/poeoauth.h"

#include <QTest>
#include <QUrlQuery>

using namespace Qt::Literals::StringLiterals;
using namespace pou;

namespace {

// made-up
const PoeConfig config{.clientId = u"examplepou"_s, .contact = u"example.invalid"_s, .ports = {49082}};

QUrl answer(const QString &query)
{
    return QUrl(u"http://127.0.0.1:49082/callback?"_s + query);
}

} // namespace

class TestPoeOauth : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void portsFromTheOption()
    {
        QCOMPARE(parsePorts(u"49082,49083, 49084"), (QList<quint16>{49082, 49083, 49084}));
        // not ports, too low, too high, repeated
        QCOMPARE(parsePorts(u"x,80,70000,49082,49082,,-1"), (QList<quint16>{49082}));
        QVERIFY(parsePorts(u"").isEmpty());
    }

    void needsEverything()
    {
        QVERIFY(config.complete());
        QVERIFY(!PoeConfig{}.complete());
        PoeConfig noId = config;
        noId.clientId.clear();
        QVERIFY(!noId.complete());
        PoeConfig noContact = config;
        noContact.contact.clear();
        QVERIFY(!noContact.complete());
        PoeConfig noPorts = config;
        noPorts.ports.clear();
        QVERIFY(!noPorts.complete());
    }

    void challengeFollowsTheRfcExample()
    {
        // the verifier and challenge from RFC 7636, appendix B
        QCOMPARE(pkceChallenge(u"dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk"_s),
                 u"E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM"_s);
        const Pkce pkce =
            pkceFrom(QByteArray::fromHex("7418dfb49799e0254ffa607dd8adbbba16d4254d69d6bff05b58055853848d79"));
        QCOMPARE(pkce.verifier, u"dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk"_s);
        QCOMPARE(pkce.challenge, u"E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM"_s);
    }

    void newOnesDiffer()
    {
        const Pkce a = newPkce();
        const Pkce b = newPkce();
        QVERIFY(a.verifier != b.verifier);
        // 43 to 128 characters of the unreserved set
        QVERIFY(a.verifier.size() >= 43 && a.verifier.size() <= 128);
        QCOMPARE(a.challenge, pkceChallenge(a.verifier));
        QVERIFY(newState() != newState());
        QVERIFY(newState().size() >= 16);
    }

    void authorizeAddress()
    {
        const QUrl url = authorizeUrl(config, 49082, u"st4te"_s, u"chal"_s);
        QCOMPARE(url.host(), u"www.pathofexile.com"_s);
        QCOMPARE(url.path(), u"/oauth/authorize"_s);
        const QUrlQuery query(url);
        QCOMPARE(query.queryItemValue(u"client_id"_s), u"examplepou"_s);
        QCOMPARE(query.queryItemValue(u"response_type"_s), u"code"_s);
        QCOMPARE(query.queryItemValue(u"scope"_s), u"account:characters"_s);
        QCOMPARE(query.queryItemValue(u"state"_s), u"st4te"_s);
        QCOMPARE(query.queryItemValue(u"code_challenge"_s), u"chal"_s);
        QCOMPARE(query.queryItemValue(u"code_challenge_method"_s), u"S256"_s);
        QCOMPARE(
            QUrl::fromPercentEncoding(query.queryItemValue(u"redirect_uri"_s, QUrl::FullyEncoded).toUtf8()),
            u"http://127.0.0.1:49082/callback"_s);
    }

    void redirectAddress() { QCOMPARE(redirectUrl(49083).toString(), u"http://127.0.0.1:49083/callback"_s); }

    void readsTheCode()
    {
        const RedirectAnswer got = readRedirect(answer(u"code=abc123&state=st4te"_s), u"st4te"_s);
        QCOMPARE(got.kind, RedirectAnswer::Kind::Code);
        QCOMPARE(got.code, u"abc123"_s);
    }

    void readsADenial()
    {
        QCOMPARE(readRedirect(answer(u"error=access_denied&state=st4te"_s), u"st4te"_s).kind,
                 RedirectAnswer::Kind::Denied);
    }

    void ignoresAnotherSignIn()
    {
        QCOMPARE(readRedirect(answer(u"code=abc123&state=other"_s), u"st4te"_s).kind,
                 RedirectAnswer::Kind::WrongState);
        QCOMPARE(readRedirect(answer(u"code=abc123"_s), u"st4te"_s).kind, RedirectAnswer::Kind::WrongState);
        // a denial that isn't ours isn't acted on either
        QCOMPARE(readRedirect(answer(u"error=access_denied&state=other"_s), u"st4te"_s).kind,
                 RedirectAnswer::Kind::WrongState);
        QVERIFY(readRedirect(answer(u"code=abc123&state=other"_s), u"st4te"_s).code.isEmpty());
    }

    void readsNothingUseful()
    {
        QCOMPARE(readRedirect(answer(u"state=st4te"_s), u"st4te"_s).kind, RedirectAnswer::Kind::Unreadable);
        QCOMPARE(readRedirect(answer(u"error=server_error&state=st4te"_s), u"st4te"_s).kind,
                 RedirectAnswer::Kind::Unreadable);
    }

    void exchangeBody()
    {
        const QUrlQuery body(QString::fromUtf8(codeExchangeBody(config, u"abc 123"_s, u"ver"_s, 49082)));
        QCOMPARE(body.queryItemValue(u"grant_type"_s), u"authorization_code"_s);
        QCOMPARE(body.queryItemValue(u"client_id"_s), u"examplepou"_s);
        QCOMPARE(body.queryItemValue(u"code"_s), u"abc 123"_s);
        QCOMPARE(body.queryItemValue(u"code_verifier"_s), u"ver"_s);
        QCOMPARE(body.queryItemValue(u"scope"_s), u"account:characters"_s);
        QCOMPARE(
            QUrl::fromPercentEncoding(body.queryItemValue(u"redirect_uri"_s, QUrl::FullyEncoded).toUtf8()),
            u"http://127.0.0.1:49082/callback"_s);
        // no secret to send for a public client
        QVERIFY(!body.hasQueryItem(u"client_secret"_s));
    }

    void refreshRequestBody()
    {
        const QUrlQuery body(QString::fromUtf8(refreshBody(config, u"refresh+me"_s)));
        QCOMPARE(body.queryItemValue(u"grant_type"_s), u"refresh_token"_s);
        QCOMPARE(body.queryItemValue(u"client_id"_s), u"examplepou"_s);
        QCOMPARE(
            QUrl::fromPercentEncoding(body.queryItemValue(u"refresh_token"_s, QUrl::FullyEncoded).toUtf8()),
            u"refresh+me"_s);
    }

    void readsTokens()
    {
        const auto tokens =
            readTokens(R"({"access_token":"fake-access","expires_in":36000,"token_type":"bearer",)"
                       R"("scope":"account:characters","refresh_token":"fake-refresh"})");
        QVERIFY(tokens);
        QCOMPARE(tokens->access, u"fake-access"_s);
        QCOMPARE(tokens->refresh, u"fake-refresh"_s);
        QCOMPARE(tokens->expiresIn, 36000);
    }

    void refusesHalfAnAnswer()
    {
        QVERIFY(!readTokens(R"({"access_token":"a","expires_in":10})"));
        QVERIFY(!readTokens(R"({"refresh_token":"r","expires_in":10})"));
        QVERIFY(!readTokens(R"({"access_token":"a","refresh_token":"r"})"));
        QVERIFY(!readTokens("not json"));
        QVERIFY(!readTokens({}));
    }

    void explainsRefusals()
    {
        const QByteArray gone = R"({"error":"invalid_grant","error_description":"expired"})";
        QVERIFY(needsNewLink(gone));
        QCOMPARE(tokenErrorText(gone), u"Your link ran out. Link your account again."_s);
        QVERIFY(needsNewLink(R"({"error":"invalid_token"})"));

        const QByteArray other = R"({"error":"invalid_client"})";
        QVERIFY(!needsNewLink(other));
        QCOMPARE(tokenErrorText(other), u"The website turned the sign in down. Try linking again."_s);
        QCOMPARE(tokenErrorText(R"({"error":"access_denied"})"),
                 u"You didn't allow it, so nothing was linked."_s);
        QVERIFY(!needsNewLink("not json"));
    }
};

QTEST_GUILESS_MAIN(TestPoeOauth)
#include "tst_poeoauth.moc"
