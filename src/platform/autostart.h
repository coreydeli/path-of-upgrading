// SPDX-License-Identifier: MIT
#pragma once

#include <QString>

namespace pou {

// Whether the program starts when you log in: an entry in the desktop's
// autostart folder, as the XDG autostart spec has it.
class Autostart
{
public:
    // What the entry says.
    struct Entry
    {
        QString name;
        QString exec; // the command, with whatever arguments it starts with
        QString icon;
    };

    // The file for a program in your autostart folder, which is
    // ~/.config/autostart/<id>.desktop unless XDG_CONFIG_HOME says otherwise.
    static QString fileFor(const QString &id);

    // With no file it's off, and can't be turned on.
    Autostart(const QString &file, const Entry &entry);

    // Looks at the file each time, as it can be changed by hand. An entry
    // that's Hidden is off, as System Settings leaves it.
    bool isEnabled() const;
    // False, with errorString(), when the file couldn't be written or removed.
    bool setEnabled(bool enabled);
    QString errorString() const { return m_error; }

private:
    QString m_file;
    Entry m_entry;
    QString m_error;
};

} // namespace pou
