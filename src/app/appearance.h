// SPDX-License-Identifier: MIT
#pragma once

#include "core/settings.h"

#include <QFont>
#include <QObject>
#include <QString>
#include <QtQmlIntegration/qqmlintegration.h>

class KColorSchemeManager;

namespace pou {

class SettingsStore;

// How the app looks, as the settings window shows it, saved as soon as it's
// changed: whose colors it has, how much is drawn besides them, and the
// fonts. The colors are a KDE color scheme that comes with the app, which goes
// on the whole app, or the desktop's own. The theme is followed by Kirigami
// and the controls with it, in the Plasma style and without. The text font and
// size are the app's font, so the controls and the spacing follow them.
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
    // Empty is the app's own heading font, "text" is the same as the text, or
    // a font family.
    Q_PROPERTY(QString headingFont READ headingFont WRITE setHeadingFont NOTIFY changed)
    // A font family, or empty for the desktop's.
    Q_PROPERTY(QString textFont READ textFont WRITE setTextFont NOTIFY changed)
    // In points, or 0 for the desktop's.
    Q_PROPERTY(double textSize READ textSize WRITE setTextSize NOTIFY changed)
    // The families the headings and the text are in now, after what isn't
    // installed has fallen back, and the desktop's size.
    Q_PROPERTY(QString headingFamily READ headingFamily NOTIFY changed)
    Q_PROPERTY(QString textFamily READ textFamily NOTIFY changed)
    Q_PROPERTY(double desktopTextSize READ desktopTextSize CONSTANT)
    // Which of the fonts chosen isn't installed, or empty.
    Q_PROPERTY(QString fontsError READ fontsError NOTIFY changed)

public:
    // The scheme file is a KDE color scheme, read when the app's colors are
    // wanted.
    Appearance(SettingsStore *settings, const QString &schemeFile, QObject *parent = nullptr);
    // Puts the desktop's font back.
    ~Appearance() override;

    QString colors() const;
    void setColors(const QString &colors);
    QString decorations() const;
    void setDecorations(const QString &decorations);
    bool hasAppColors() const { return m_appColors; }
    bool isPlain() const { return appearance().decorations == Decorations::Plain; }
    QString colorsError() const { return m_error; }
    QString headingFont() const { return appearance().headingFont; }
    void setHeadingFont(const QString &family);
    QString textFont() const { return appearance().textFont; }
    void setTextFont(const QString &family);
    double textSize() const { return appearance().textSize; }
    void setTextSize(double points);
    QString headingFamily() const { return m_headingFamily; }
    QString textFamily() const { return m_font.family(); }
    double desktopTextSize() const { return m_desktopFont.pointSizeF(); }
    QString fontsError() const { return m_fontsError; }
    // The fonts to pick from.
    Q_INVOKABLE QStringList families() const;

Q_SIGNALS:
    void changed();

private:
    const AppearanceSettings &appearance() const;
    template<typename Edit>
    void change(Edit edit);
    void follow();
    bool activateScheme();
    void followFonts();

    SettingsStore *m_settings;
    QString m_schemeFile;
    // made when the desktop's colors come back
    KColorSchemeManager *m_manager = nullptr;
    bool m_appColors = false;
    QString m_error;
    // the font the program started with, and the one it has now
    QFont m_desktopFont;
    QFont m_font;
    QString m_headingFamily;
    QString m_fontsError;
    // for telling when something that isn't the colors changed
    AppearanceSettings m_seen;
};

} // namespace pou
