// SPDX-License-Identifier: MIT
#pragma once

#include <QObject>
#include <QString>
#include <QtQmlIntegration/qqmlintegration.h>

namespace pou {

class CharacterList;
class OverlayOptions;
class SettingsStore;

// Whether the setup guide is waiting to be shown. It's there on the first
// run, or when the program is started with --setup, and it stays until it's
// finished or skipped.
class Onboarding : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("The app has the one.")

    Q_PROPERTY(bool pending READ isPending NOTIFY pendingChanged)
    // The key that checks an item, the way it's written on the keyboard.
    // Empty when it has none.
    Q_PROPERTY(QString hotkey READ hotkey NOTIFY hotkeyChanged)

public:
    // They have to outlive it. The guide isn't wanted until decide().
    Onboarding(SettingsStore *settings, CharacterList *characters, OverlayOptions *options,
               QObject *parent = nullptr);

    // Looks at what was saved, so it has to come once the characters are
    // loaded and before the program saves anything of its own.
    void decide();
    // Brings the guide back, as --setup does.
    void ask();
    // Records that it's done, for both Finish and Skip.
    Q_INVOKABLE void finish();

    bool isPending() const { return m_pending; }
    QString hotkey() const;

Q_SIGNALS:
    void pendingChanged();
    void hotkeyChanged();

private:
    void setPending(bool pending);

    SettingsStore *m_settings;
    CharacterList *m_characters;
    OverlayOptions *m_options;
    bool m_pending = false;
};

} // namespace pou
