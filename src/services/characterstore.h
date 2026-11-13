// SPDX-License-Identifier: MIT
#pragma once

#include "core/characterdata.h"

#include <QHash>
#include <QString>
#include <QStringList>
#include <optional>

namespace pou {

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
