// SPDX-License-Identifier: MIT
#pragma once

#include "app/activecharacter.h"
#include "core/settings.h"
#include "core/weighedprofile.h"

#include <QList>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <functional>

namespace pou {

class PobEngine;
class PobReply;

// Has Path of Building weigh the character's build, one slot group at a time,
// and keeps what it says with the character as its weighed profile (see
// WeighedProfile). It weighs when Path of Building is on and the character
// has a build with no weighing, or one made for another build, another
// Path of Building or another damage balance. Items being checked go first.
class BuildWeigher : public QObject
{
    Q_OBJECT

public:
    // They have to outlive it.
    BuildWeigher(ActiveCharacter *character, PobEngine *pob, QObject *parent = nullptr);

    // Whether Path of Building is on, and the balance it weighs by.
    void setSettings(const PobSettings &settings);
    // Says when an item is being checked, which it lets finish before it asks
    // for the next slot.
    void setBusy(const std::function<bool()> &busy) { m_busy = busy; }

    // Weighs again even when the last weighing is right for the build, for
    // when you ask. False when it can't, say with Path of Building off.
    bool weighAgain();
    bool isWeighing() const { return m_running; }

Q_SIGNALS:
    void weighingChanged();

public Q_SLOTS:
    // Starts weighing if it should.
    void consider();

private Q_SLOTS:
    void next();

private:
    bool needed() const;
    // What the weighed profile would have to say to be right for now.
    bool upToDate(const WeighedProfile &weighed) const;
    QString attempt() const;
    void begin();
    void answered(const QString &group);
    void finish();
    void cancel();
    void setRunning(bool running);

    ActiveCharacter *m_character;
    PobEngine *m_pob;
    PobSettings m_settings;
    std::function<bool()> m_busy;
    QTimer m_retry;

    bool m_running = false;
    int m_run = 0;
    QString m_for;      // the character's id
    QString m_build;    // and the build's fingerprint, when it began
    QStringList m_left; // the groups still to weigh
    QList<SlotWeighing> m_done;
    QPointer<PobReply> m_reply;
    // what failed to come out of a weighing, so it isn't tried over and over
    QString m_gaveUp;
};

} // namespace pou
