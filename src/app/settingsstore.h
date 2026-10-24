// SPDX-License-Identifier: MIT
#pragma once

#include "core/settings.h"

#include <QObject>
#include <QSettings>
#include <QString>

namespace pou {

// Your settings, read when the app starts and saved each time you change
// them, for the parts of the app that follow them.
class SettingsStore : public QObject
{
    Q_OBJECT

public:
    // Reads them from the file, in QSettings' INI format.
    explicit SettingsStore(const QString &path, QObject *parent = nullptr);

    const Settings &settings() const { return m_settings; }
    // Saves them when they're different. False when they couldn't be, but
    // they're used anyway.
    bool set(const Settings &settings);

Q_SIGNALS:
    void changed();

private:
    QSettings m_store;
    Settings m_settings;
};

} // namespace pou
