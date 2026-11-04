// SPDX-License-Identifier: MIT
#pragma once

#include "core/settings.h"

#include <QObject>
#include <QString>
#include <QtQmlIntegration/qqmlintegration.h>

class KColorSchemeManager;

namespace pou {

class SettingsStore;

// How the app looks, as the settings window shows it, saved as soon as it's
// changed: whose colors it has, and how much is drawn besides them. The
// colors are a KDE color scheme that comes with the app, which goes on the
// whole app, or the desktop's own. The theme is followed by Kirigami and the
// controls with it, in the Plasma style and without.
class Appearance : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("The app has the one.")

    // "app" or "desktop"
    Q_PROPERTY(QString colors READ colors WRITE setColors NOTIFY changed)
    // "full" or "plain"
    Q_PROPERTY(QString decorations READ decorations WRITE setDecorations NOTIFY changed)
    // Whether the app's colors are on, which they aren't when it's the
    // desktop's you chose or the scheme couldn't be found.
    Q_PROPERTY(bool appColors READ hasAppColors NOTIFY changed)
    Q_PROPERTY(bool plain READ isPlain NOTIFY changed)
    // Why the app's colors aren't on, or empty.
    Q_PROPERTY(QString colorsError READ colorsError NOTIFY changed)

public:
    // The scheme file is named PathOfUpgrading.colors and has to be in a
    // folder called color-schemes, which is where the color scheme manager
    // looks. It's looked for the first time the app's colors are wanted.
    Appearance(SettingsStore *settings, const QString &schemeFile, QObject *parent = nullptr);

    // The id of the scheme in the file's name
    static QString schemeId();

    QString colors() const;
    void setColors(const QString &colors);
    QString decorations() const;
    void setDecorations(const QString &decorations);
    bool hasAppColors() const { return m_appColors; }
    bool isPlain() const { return appearance().decorations == Decorations::Plain; }
    QString colorsError() const { return m_error; }

Q_SIGNALS:
    void changed();

private:
    const AppearanceSettings &appearance() const;
    template<typename Edit>
    void change(Edit edit);
    void follow();
    bool activateScheme();

    SettingsStore *m_settings;
    QString m_schemeFile;
    // made when it's first needed, and left when the desktop's colors are on
    KColorSchemeManager *m_manager = nullptr;
    bool m_appColors = false;
    QString m_error;
};

} // namespace pou
