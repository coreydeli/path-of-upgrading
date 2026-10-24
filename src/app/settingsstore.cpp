// SPDX-License-Identifier: MIT
#include "app/settingsstore.h"

namespace pou {

SettingsStore::SettingsStore(const QString &path, QObject *parent)
    : QObject(parent)
    , m_store(path, QSettings::IniFormat)
    , m_settings(loadSettings(m_store))
{}

bool SettingsStore::set(const Settings &settings)
{
    if (settings == m_settings)
        return true;
    m_settings = settings;
    saveSettings(m_store, m_settings);
    // rather than when it's destroyed, to know whether it worked
    m_store.sync();
    Q_EMIT changed();
    return m_store.status() == QSettings::NoError;
}

} // namespace pou
