// SPDX-License-Identifier: MIT
#pragma once

#include "core/characterstore.h"
#include "core/pobbuild.h"
#include "core/pobverdict.h"

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <functional>
#include <optional>

namespace pou {

class PobEngine;

// The character you play, read from your characters with its build. Path of
// Building is kept in step with it, with the build and the gear you wear in
// place of its own, and what it finds out about the build is saved with the
// character.
class ActiveCharacter : public QObject
{
    Q_OBJECT

public:
    // They have to outlive it.
    ActiveCharacter(CharacterStore *store, PobEngine *pob, QObject *parent = nullptr);

    // By its id in the store, see Settings::character.
    void setId(const QString &id);
    QString id() const { return m_id; }
    // Reads it again, for when it's been changed in the store.
    void reload();

    // False when the store has no character with the id.
    bool exists() const { return m_store->find(m_id) != nullptr; }
    // With nothing on when it doesn't exist.
    const Character &character() const { return m_character; }
    // None when it has none, or one that isn't for Path of Exile 2.
    const std::optional<PobBuild> &build() const { return m_build; }
    // What Path of Building says of the build with your gear on, once it has
    // loaded them, see summary in pobbridge.lua. None until then, and again
    // when it's given another build or other gear.
    const std::optional<PobStats> &pobStats() const { return m_pobStats; }

    // Puts the item on in the slot and saves that, see pou::wear(). False when
    // it can't go there or it can't be saved, with why in errorString().
    bool wear(const QString &slot, const Item &item);
    bool takeOff(const QString &slot);
    // Saves a change to how items are judged for it, like its weights. False
    // when there's none or it can't be saved.
    bool change(const std::function<void(Character &)> &edit);
    QString errorString() const { return m_error; }

Q_SIGNALS:
    void changed();
    void pobStatsChanged();

private:
    void loaded(const QJsonObject &summary);
    void setPobStats(const std::optional<PobStats> &stats);
    bool save(const Character &character);
    bool fail(const QString &why);

    CharacterStore *m_store;
    PobEngine *m_pob;
    QString m_id;
    Character m_character;
    std::optional<PobBuild> m_build;
    // what Path of Building was given last, and what it said of it
    QString m_pobXml;
    QList<PobItem> m_pobWearing;
    std::optional<PobStats> m_pobStats;
    QString m_error;
};

} // namespace pou
