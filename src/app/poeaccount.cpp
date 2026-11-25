// SPDX-License-Identifier: MIT
#include "app/poeaccount.h"

#include "platform/opener.h"
#include "platform/secretstore.h"
#include "services/poeauth.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QTimer>
#include <utility>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

// Is the access token worth using, with a minute of its life still to go.
constexpr int spareSeconds = 60;

const QString notLinked = u"Your account isn't linked."_s;

QString ranOutText()
{
    return u"Your link ran out after 7 days, which is how long the website lets it last. "
           "Link your account again."_s;
}

QByteArray stored(const QString &refresh, const QDateTime &until)
{
    return QJsonDocument(QJsonObject{{u"refresh"_s, refresh}, {u"until"_s, until.toString(Qt::ISODate)}})
        .toJson(QJsonDocument::Compact);
}

} // namespace

QString PoeAccount::secretName()
{
    return u"poe-oauth"_s;
}

PoeAccount::PoeAccount(bool offered, SecretStore *store, PoeAuth *auth, Opener *opener, QObject *parent)
    : QObject(parent)
    , m_offered(offered)
    , m_store(store)
    , m_auth(auth)
    , m_opener(opener)
    , m_clock([] { return QDateTime::currentDateTimeUtc(); })
{
    connect(m_auth, &PoeAuth::browserWanted, this, [this](const QUrl &url) {
        if (m_opener)
            m_opener->open(url);
    });
    connect(m_auth, &PoeAuth::linked, this, [this](const PoeTokens &tokens) {
        if (m_state == State::Waiting)
            takeLink(tokens, true);
    });
    connect(m_auth, &PoeAuth::refreshed, this, [this](const PoeTokens &tokens) {
        if (!m_refreshing)
            return;
        m_refreshing = false;
        takeLink(tokens, false);
        answerWaiters(tokens.access, {});
    });
    connect(m_auth, &PoeAuth::failed, this, [this](const QString &text, bool newLinkNeeded) {
        if (m_refreshing) {
            m_refreshing = false;
            if (newLinkNeeded) {
                dropLink(text);
            } else {
                m_error = text;
                answerWaiters({}, text);
                Q_EMIT changed();
            }
        } else if (m_state == State::Waiting) {
            m_error = text;
            setState(m_refresh.isEmpty() ? State::Unlinked : State::Linked);
        }
    });
}

QDateTime PoeAccount::now() const
{
    return m_clock();
}

QString PoeAccount::status() const
{
    if (m_busy > 0)
        return u"Waiting for the keyring."_s;
    switch (m_state) {
    case State::NotAsked:
    case State::Unlinked:
        break;
    case State::Unknown:
        return u"Couldn't tell whether your account is linked."_s;
    case State::Waiting:
        return u"Waiting for you in the browser. Sign in to Path of Exile and allow Path of Upgrading there."_s;
    case State::Linked:
        return u"Your account is linked until %1."_s.arg(
            QLocale().toString(m_linkedUntil.toLocalTime().date(), QLocale::LongFormat));
    }
    return notLinked;
}

QString PoeAccount::disclaimer() const
{
    return u"This product isn't affiliated with or endorsed by Grinding Gear Games in any way."_s;
}

QStringList PoeAccount::notes() const
{
    return {
        u"Path of Upgrading never sees your password. You sign in on the Path of Exile website and allow it there."_s,
        u"Only a token is kept, in your system keyring. It lets the app read your list of characters and their gear."_s,
        u"A link lasts 7 days at most, which is how long the website lets it last. After that you link again."_s,
        u"Unlinking here only forgets the link on this computer. To take its access away on the website, "
        "open your account's applications page."_s,
    };
}

void PoeAccount::setState(State state)
{
    const bool wasLinked = isLinked();
    m_state = state;
    Q_EMIT changed();
    if (wasLinked != isLinked())
        Q_EMIT linkedChanged();
}

void PoeAccount::begin()
{
    ++m_busy;
    m_error.clear();
    Q_EMIT changed();
}

void PoeAccount::end()
{
    --m_busy;
    Q_EMIT changed();
}

void PoeAccount::load()
{
    // once, so a keyring that said no isn't asked again and again
    if (!m_offered || m_state != State::NotAsked || m_busy > 0)
        return;
    begin();
    m_store->read(secretName(), this, [this](const SecretStore::Result &result) {
        end();
        if (result.status == SecretStore::Status::Failed) {
            m_error = u"Couldn't read your Path of Exile link from the keyring: %1"_s.arg(result.error);
            setState(State::Unknown);
            return;
        }
        const QJsonObject object = QJsonDocument::fromJson(result.secret.toUtf8()).object();
        const QString refresh = object.value(u"refresh"_s).toString();
        const QDateTime until = QDateTime::fromString(object.value(u"until"_s).toString(), Qt::ISODate);
        if (result.status == SecretStore::Status::NotFound) {
            setState(State::Unlinked);
        } else if (refresh.isEmpty() || !until.isValid()) {
            // not one of ours, so it's no use to anyone
            dropLink({});
        } else if (until <= now()) {
            dropLink(ranOutText());
        } else {
            m_refresh = refresh;
            m_linkedUntil = until;
            setState(State::Linked);
        }
    });
}

void PoeAccount::link()
{
    if (!m_offered || m_state == State::Waiting || m_busy > 0 || m_refreshing)
        return;
    m_error.clear();
    setState(State::Waiting);
    m_auth->start();
}

void PoeAccount::cancel()
{
    if (m_state != State::Waiting)
        return;
    m_auth->cancel();
    setState(m_refresh.isEmpty() ? State::Unlinked : State::Linked);
}

void PoeAccount::unlink()
{
    if (!m_offered || m_busy > 0)
        return;
    m_auth->cancel();
    m_refreshing = false;
    forgetTokens();
    answerWaiters({}, notLinked);
    begin();
    setState(State::Unlinked);
    m_store->remove(secretName(), this, [this](const SecretStore::Result &result) {
        end();
        if (result.status == SecretStore::Status::Failed) {
            m_error = u"Couldn't remove your Path of Exile link from the keyring: %1"_s.arg(result.error);
            Q_EMIT changed();
        }
    });
}

void PoeAccount::openRevokePage()
{
    if (m_opener)
        m_opener->open(QUrl(u"https://www.pathofexile.com/my-account/applications"_s));
}

void PoeAccount::forgetTokens()
{
    m_access.clear();
    m_accessUntil = {};
    m_refresh.clear();
    m_linkedUntil = {};
}

bool PoeAccount::accessIsGood() const
{
    return !m_access.isEmpty() && m_accessUntil > now().addSecs(spareSeconds);
}

void PoeAccount::takeLink(const PoeTokens &tokens, bool fresh)
{
    m_access = tokens.access;
    m_accessUntil = now().addSecs(tokens.expiresIn);
    m_refresh = tokens.refresh;
    // a refresh gives a new token with the old one's end
    if (fresh)
        m_linkedUntil = now().addDays(linkDays);
    begin();
    m_store->write(secretName(), QString::fromUtf8(stored(m_refresh, m_linkedUntil)), this,
                   [this, fresh](const SecretStore::Result &result) {
                       end();
                       if (result.status == SecretStore::Status::Done) {
                           if (fresh)
                               setState(State::Linked);
                           return;
                       }
                       if (fresh) {
                           // not kept, so it would be lost at the next start
                           forgetTokens();
                           m_error = u"Couldn't keep your link in the keyring: %1"_s.arg(result.error);
                           setState(State::Unlinked);
                       } else {
                           m_error =
                               u"Couldn't save the refreshed link in the keyring: %1"_s.arg(result.error);
                           Q_EMIT changed();
                       }
                   });
}

void PoeAccount::dropLink(const QString &why)
{
    forgetTokens();
    begin();
    m_error = why;
    setState(State::Unlinked);
    answerWaiters({}, why.isEmpty() ? notLinked : why);
    m_store->remove(secretName(), this, [this, why](const SecretStore::Result &result) {
        end();
        if (result.status == SecretStore::Status::Failed) {
            m_error = u"Couldn't remove your Path of Exile link from the keyring: %1"_s.arg(result.error);
            Q_EMIT changed();
        } else if (m_error.isEmpty()) {
            m_error = why;
            Q_EMIT changed();
        }
    });
}

void PoeAccount::answerWaiters(const QString &token, const QString &error)
{
    const QList<Waiter> waiters = std::exchange(m_waiters, {});
    for (const Waiter &waiter : waiters) {
        if (waiter.context)
            waiter.handler(token, error);
    }
}

void PoeAccount::rejectToken()
{
    m_access.clear();
    m_accessUntil = {};
}

void PoeAccount::withToken(const QObject *context, TokenHandler handler)
{
    const auto later = [context](TokenHandler handler, QString token, QString error) {
        QTimer::singleShot(0, context,
                           [handler = std::move(handler), token = std::move(token),
                            error = std::move(error)] { handler(token, error); });
    };
    if (!m_offered || m_refresh.isEmpty()) {
        later(std::move(handler), {}, notLinked);
        return;
    }
    if (m_linkedUntil <= now()) {
        m_waiters << Waiter{.context = context, .handler = std::move(handler)};
        dropLink(ranOutText());
        return;
    }
    if (accessIsGood()) {
        later(std::move(handler), m_access, {});
        return;
    }
    m_waiters << Waiter{.context = context, .handler = std::move(handler)};
    if (!m_refreshing) {
        m_refreshing = true;
        m_auth->refresh(m_refresh);
    }
}

} // namespace pou
