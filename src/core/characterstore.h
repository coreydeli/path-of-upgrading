// SPDX-License-Identifier: MIT
#pragma once

#include "core/gearscore.h"
#include "core/ignore.h"
#include "core/item.h"
#include "core/resists.h"
#include "core/weights.h"

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>
#include <optional>

namespace pou {

// One of your characters, with everything kept for it between runs: what it
// wears, how items are judged for it and its resistances.
struct Character
{
    QString name;
    Equipped gear;
    // Slots whose item came with the Path of Building build rather than from
    // the game.
    QSet<QString> fromPob;
    Weights weights = defaultWeights();
    QList<IgnoreRule> ignored;
    QString buildProfile; // gear score profile id, empty for the default
    ProfileTweaks tweaks;
    std::optional<ResistRecord> resists;
    // Path of Building found Chaos Inoculation the last time it loaded the
    // build. Kept for when it isn't running.
    bool chaosInoculation = false;
};

// Gear is kept as the game's text and read again with parseItem(), so items
// saved long ago gain from what the parser has learned since. An item it
// can't read any more is left out.
QJsonObject toJson(const Character &character);
// Empty for a character saved by a newer version of the app, in a way this
// one doesn't know.
std::optional<Character> characterFromJson(const QJsonObject &o);

// Your characters, each in a JSON file of its own in one folder, with the
// Path of Building build imported for it next to it.
class CharacterStore
{
public:
    explicit CharacterStore(const QString &folder);

    // Reads every character in the folder again. A file that can't be read
    // is left as it is, for you to look at, and named in problems().
    void load();
    QStringList problems() const { return m_problems; }

    // Sorted by name.
    QStringList ids() const;
    // Null when there's no character with that id.
    const Character *find(const QString &id) const;

    // Saves a new character and returns its id, or nothing when it can't.
    std::optional<QString> add(const Character &character);
    // False when the id isn't one of yours or the file can't be written.
    bool save(const QString &id, const Character &character);
    // Deletes the character along with its build.
    bool remove(const QString &id);

    // The build as Path of Building saves it, empty when none was imported.
    std::optional<QString> pobBuild(const QString &id) const;
    bool savePobBuild(const QString &id, const QString &xml);

    // Why the last change that returned false or nothing failed.
    QString errorString() const { return m_error; }

private:
    QString pathFor(const QString &id, QStringView suffix) const;
    bool fail(const QString &why);

    QString m_folder;
    QHash<QString, Character> m_characters;
    QStringList m_problems;
    QString m_error;
};

} // namespace pou
