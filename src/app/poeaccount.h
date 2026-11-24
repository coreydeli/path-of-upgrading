// SPDX-License-Identifier: MIT
#pragma once

#include "core/poeoauth.h"
#include "services/poeclient.h"

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QtQmlIntegration/qqmlintegration.h>
#include <functional>

namespace pou {

class Opener;
class PoeAuth;
class SecretStore;

// The Path of Exile account linked to the app, if there is one. The refresh
// token is kept in the system keyring, and the access token in memory only.
// A link lasts 7 days from when it was made, as the website sees to that, and
// then the player links again.
class PoeAccount : public QObject, public PoeTokenSource
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("The app has the one.")

    // Whether this build can link at all, see App::Paths::poeOffered. When not
    // nothing here does anything.
    Q_PROPERTY(bool offered READ isOffered CONSTANT)
    Q_PROPERTY(bool linked READ isLinked NOTIFY changed)
    // While the browser is open on the sign in.
    Q_PROPERTY(bool waiting READ isWaiting NOTIFY changed)
    // While the keyring or the website is asked something.
    Q_PROPERTY(bool busy READ isBusy NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    // Why the last thing asked for didn't happen, or empty.
    Q_PROPERTY(QString error READ error NOTIFY changed)
    // The words the card shows, which the website asks for or the player
    // should know.
    Q_PROPERTY(QString disclaimer READ disclaimer CONSTANT)
    Q_PROPERTY(QStringList notes READ notes CONSTANT)

public:
    // The new token, or why there isn't one.
    using TokenHandler = PoeTokenSource::Handler;
    using Clock = std::function<QDateTime()>;

    // How long the website lets a link last.
    static constexpr int linkDays = 7;
    // They have to outlive it. The opener may be null, and then it opens
    // nothing.
    PoeAccount(bool offered, SecretStore *store, PoeAuth *auth, Opener *opener = nullptr,
               QObject *parent = nullptr);

    // What the link is kept under in the keyring.
    static QString secretName();
    // For tests, which move time.
    void setClock(const Clock &clock) { m_clock = clock; }

    // Reads the keyring once, so it can be called whenever it's convenient.
    Q_INVOKABLE void load();
    // Opens the browser on the website's sign in.
    Q_INVOKABLE void link();
    // Stops waiting for the browser.
    Q_INVOKABLE void cancel();
    // Forgets the link here. The website's side is the player's to revoke, as
    // it has no way for an app to.
    Q_INVOKABLE void unlink();
    Q_INVOKABLE void openRevokePage();

    // Calls back, later, with an access token that's good for a minute at
    // least, asking the website for a new one if need be. A link that has
    // run out says so and is dropped. The handler isn't called if the
    // context is gone.
    void withToken(const QObject *context, TokenHandler handler) override;
    // The website turned the access token down, so the next one is new.
    void rejectToken() override;

    bool isOffered() const { return m_offered; }
    bool isLinked() const { return m_state == State::Linked; }
    bool isWaiting() const { return m_state == State::Waiting; }
    bool isBusy() const { return m_busy > 0; }
    QString status() const;
    QString error() const { return m_error; }
    QString disclaimer() const;
    QStringList notes() const;

Q_SIGNALS:
    void changed();
    // Linked, or no longer.
    void linkedChanged();

private:
    enum class State {
        NotAsked, // the keyring
        Unknown,  // as it couldn't say
        Unlinked,
        Waiting,
        Linked,
    };
    struct Waiter
    {
        QPointer<const QObject> context;
        TokenHandler handler;
    };

    QDateTime now() const;
    void setState(State state);
    void begin();
    void end();
    void takeLink(const PoeTokens &tokens, bool fresh);
    void save(const QString &failure);
    // Drops the link, here and in the keyring, saying why.
    void dropLink(const QString &why);
    void forgetTokens();
    void answerWaiters(const QString &token, const QString &error);
    bool accessIsGood() const;

    bool m_offered;
    SecretStore *m_store;
    PoeAuth *m_auth;
    Opener *m_opener;
    Clock m_clock;
    State m_state = State::NotAsked;
    int m_busy = 0;
    QString m_error;
    QString m_access;
    QDateTime m_accessUntil;
    QString m_refresh;
    QDateTime m_linkedUntil;
    bool m_refreshing = false;
    QList<Waiter> m_waiters;
};

} // namespace pou
