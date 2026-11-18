// SPDX-License-Identifier: MIT
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QtQmlIntegration/qqmlintegration.h>
#include <functional>

namespace pou {

class Autostart;
class GlobalShortcuts;
class SecretStore;

// Wipes everything the app keeps and starts it again as if it had never run,
// for testers who want to go through it as a new user. Only betas have it, see
// Paths::startOverOffered. It touches the app's own files and nothing else:
// not Path of Building, its builds or the game.
class StartOver : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("The app has the one.")

    // Whether this build has it at all.
    Q_PROPERTY(bool offered READ isOffered CONSTANT)
    // While the keyring is asked to let the session ID go, which can wait on you.
    Q_PROPERTY(bool busy READ isBusy NOTIFY changed)
    // What couldn't be removed, one sentence each. The rest is done.
    Q_PROPERTY(QStringList problems READ problems NOTIFY changed)

public:
    // Where the app keeps its files.
    struct Places
    {
        QString settings; // the file
        QString data;     // the folder
        QString cache;    // the folder
    };
    // Starts the app again once this one is gone, and quits it. False when it
    // couldn't.
    using Restart = std::function<bool()>;

    // They have to outlive it. Without offered it does nothing.
    StartOver(bool offered, const Places &places, Autostart *autostart, SecretStore *secrets,
              GlobalShortcuts *shortcuts, QObject *parent = nullptr);

    void setRestart(const Restart &restart) { m_restart = restart; }

    // Forgets the hotkeys, takes out the autostart entry and the session ID,
    // then removes the files and starts the app again, which opens on the
    // setup guide. When something can't be removed the rest still goes, and
    // it stays open to say what.
    Q_INVOKABLE void run();

    bool isOffered() const { return m_offered; }
    bool isBusy() const { return m_busy; }
    QStringList problems() const { return m_problems; }

Q_SIGNALS:
    void changed();

private:
    void removeFiles();

    bool m_offered;
    Places m_places;
    Autostart *m_autostart;
    SecretStore *m_secrets;
    GlobalShortcuts *m_shortcuts;
    Restart m_restart;
    bool m_busy = false;
    QStringList m_problems;
};

} // namespace pou
