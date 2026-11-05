// SPDX-License-Identifier: MIT
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QtQmlIntegration/qqmlintegration.h>

namespace pou {

class Opener;
class SecretStore;
class TradeClient;

// The session id of a browser that's signed in to the trade site, which
// makes its searches bigger and its limits yours. It's kept in the system
// keyring, never in the settings, and the trade client is signed in with it.
class TradeSession : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("The app has the one.")

    // Whether one is saved, as far as the keyring has said.
    Q_PROPERTY(bool saved READ isSaved NOTIFY changed)
    // While the keyring is asked something, which can wait on you.
    Q_PROPERTY(bool busy READ isBusy NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    // Why the last thing asked for didn't happen, or empty.
    Q_PROPERTY(QString error READ error NOTIFY changed)
    // What the page says to someone who doesn't know where to find the id,
    // see SessionGuide.
    Q_PROPERTY(QStringList findSteps READ findSteps CONSTANT)
    Q_PROPERTY(QStringList findNotes READ findNotes CONSTANT)

public:
    // They have to outlive it. The opener may be null, and then it opens
    // nothing.
    TradeSession(SecretStore *store, TradeClient *client, Opener *opener = nullptr,
                 QObject *parent = nullptr);

    // Signs in with the one in the keyring. It asks the keyring once, so it
    // can be called whenever trade searches are on.
    void load();
    // Keeps it in the keyring and signs in with it, once it's trimmed and
    // looks like a session id.
    Q_INVOKABLE void save(const QString &text);
    // Takes it out of the keyring and signs out.
    Q_INVOKABLE void forget();
    // For the button that opens the page you sign in to the website on.
    Q_INVOKABLE void openSignIn();

    bool isSaved() const { return m_state == State::SignedIn; }
    bool isBusy() const { return m_busy; }
    // "A session ID is saved."
    QString status() const;
    QString error() const { return m_error; }
    QStringList findSteps() const;
    QStringList findNotes() const;

Q_SIGNALS:
    void changed();
    // The trade client is signed in with another id, or signed out.
    void idChanged();
    // After a save, for the field it was typed in to empty.
    void stored();

private:
    enum class State {
        NotAsked, // the keyring, which is only asked while trade searches are on
        Unknown,  // as it couldn't say
        SignedOut,
        SignedIn,
    };

    void begin();
    void end();
    // The client's id, which is empty for none.
    void signIn(const QString &id);

    SecretStore *m_store;
    TradeClient *m_client;
    Opener *m_opener;
    State m_state = State::NotAsked;
    bool m_busy = false;
    QString m_error;
    QString m_id; // the client's
};

} // namespace pou
