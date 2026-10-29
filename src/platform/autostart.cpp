// SPDX-License-Identifier: MIT
#include "platform/autostart.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>

using namespace Qt::Literals::StringLiterals;

namespace pou {

// In a Flatpak that folder is the app's own, and nothing starts from it. The
// Background portal's RequestBackground, with autostart on, would be used there.

QString Autostart::fileFor(const QString &id)
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + u"/autostart/"_s + id +
           u".desktop"_s;
}

Autostart::Autostart(const QString &file, const Entry &entry)
    : m_file(file)
    , m_entry(entry)
{}

bool Autostart::isEnabled() const
{
    if (m_file.isEmpty() || !QFileInfo::exists(m_file))
        return false;
    QSettings entry(m_file, QSettings::IniFormat);
    entry.beginGroup(u"Desktop Entry"_s);
    return !entry.value(u"Hidden"_s, false).toBool();
}

bool Autostart::setEnabled(bool enabled)
{
    m_error.clear();
    if (!enabled) {
        // already gone is as good as removed
        if (QFileInfo::exists(m_file) && !QFile::remove(m_file)) {
            m_error = u"Couldn't remove %1."_s.arg(m_file);
            return false;
        }
        return true;
    }

    if (m_file.isEmpty()) {
        m_error = u"There's nowhere to keep it."_s;
        return false;
    }
    if (!QDir().mkpath(QFileInfo(m_file).absolutePath())) {
        m_error = u"Couldn't make the folder for %1."_s.arg(m_file);
        return false;
    }
    QSaveFile file(m_file);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        m_error = u"Couldn't write %1: %2"_s.arg(m_file, file.errorString());
        return false;
    }
    const QString text = u"[Desktop Entry]\n"
                         "Type=Application\n"
                         "Name=%1\n"
                         "Exec=%2\n"
                         "Icon=%3\n"
                         "Terminal=false\n"_s.arg(m_entry.name, m_entry.exec, m_entry.icon);
    file.write(text.toUtf8());
    if (!file.commit()) {
        m_error = u"Couldn't write %1: %2"_s.arg(m_file, file.errorString());
        return false;
    }
    return true;
}

} // namespace pou
