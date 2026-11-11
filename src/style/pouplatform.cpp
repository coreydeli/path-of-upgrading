// SPDX-License-Identifier: MIT
// The Kirigami half of the app's own style. The controls are a QML module;
// this plugin tells Kirigami the colors, fonts and units to use, from the
// color scheme the app has chosen rather than from the desktop's style.
#include <KColorScheme>
#include <KSharedConfig>
#include <Kirigami/Platform/PlatformPluginFactory>
#include <Kirigami/Platform/PlatformTheme>
#include <Kirigami/Platform/Units>
#include <QGuiApplication>

using namespace Kirigami::Platform;

namespace {

KColorScheme::ColorSet kdeSet(PlatformTheme::ColorSet set)
{
    switch (set) {
    case PlatformTheme::Window:
        return KColorScheme::Window;
    case PlatformTheme::Button:
        return KColorScheme::Button;
    case PlatformTheme::Selection:
        return KColorScheme::Selection;
    case PlatformTheme::Tooltip:
        return KColorScheme::Tooltip;
    case PlatformTheme::Complementary:
        return KColorScheme::Complementary;
    case PlatformTheme::Header:
        return KColorScheme::Header;
    default:
        return KColorScheme::View;
    }
}

// The scheme the app has put on itself, or the desktop's when it hasn't
KSharedConfigPtr scheme()
{
    const QString path = qApp->property("KDE_COLOR_SCHEME_PATH").toString();
    if (path.isEmpty())
        return {};
    return KSharedConfig::openConfig(path, KConfig::SimpleConfig);
}

class Theme : public PlatformTheme
{
public:
    explicit Theme(QObject *parent)
        : PlatformTheme(parent)
    {
        setSupportsIconColoring(true);
        sync();
        syncFonts();
        qGuiApp->installEventFilter(this);
    }

protected:
    bool event(QEvent *event) override
    {
        if (event->type() == PlatformThemeEvents::ColorSetChangedEvent::type ||
            event->type() == PlatformThemeEvents::ColorGroupChangedEvent::type ||
            event->type() == PlatformThemeEvents::DataChangedEvent::type) {
            sync();
            // the theme has nowhere to keep the fonts until it's told
            syncFonts();
        }
        return PlatformTheme::event(event);
    }

    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (watched == qGuiApp) {
            if (event->type() == QEvent::ApplicationFontChange)
                syncFonts();
            else if (event->type() == QEvent::ApplicationPaletteChange)
                sync();
        }
        return PlatformTheme::eventFilter(watched, event);
    }

private:
    void syncFonts()
    {
        const QFont font = QGuiApplication::font();
        setDefaultFont(font);
        QFont small = font;
        small.setPointSizeF(font.pointSizeF() * 0.86);
        setSmallFont(small);
    }

    void sync()
    {
        const auto group = QPalette::ColorGroup(colorGroup());
        const KSharedConfigPtr config = scheme();
        const KColorScheme s(group, kdeSet(colorSet()), config);
        const KColorScheme selection(group, KColorScheme::Selection, config);
        setTextColor(s.foreground(KColorScheme::NormalText).color());
        setDisabledTextColor(s.foreground(KColorScheme::InactiveText).color());
        setHighlightedTextColor(selection.foreground(KColorScheme::NormalText).color());
        setActiveTextColor(s.foreground(KColorScheme::ActiveText).color());
        setLinkColor(s.foreground(KColorScheme::LinkText).color());
        setVisitedLinkColor(s.foreground(KColorScheme::VisitedText).color());
        setNegativeTextColor(s.foreground(KColorScheme::NegativeText).color());
        setNeutralTextColor(s.foreground(KColorScheme::NeutralText).color());
        setPositiveTextColor(s.foreground(KColorScheme::PositiveText).color());
        setBackgroundColor(s.background(KColorScheme::NormalBackground).color());
        setAlternateBackgroundColor(s.background(KColorScheme::AlternateBackground).color());
        setHighlightColor(selection.background(KColorScheme::NormalBackground).color());
        setActiveBackgroundColor(s.background(KColorScheme::ActiveBackground).color());
        setLinkBackgroundColor(s.background(KColorScheme::LinkBackground).color());
        setVisitedLinkBackgroundColor(s.background(KColorScheme::VisitedBackground).color());
        setNegativeBackgroundColor(s.background(KColorScheme::NegativeBackground).color());
        setNeutralBackgroundColor(s.background(KColorScheme::NeutralBackground).color());
        setPositiveBackgroundColor(s.background(KColorScheme::PositiveBackground).color());
        setFocusColor(s.decoration(KColorScheme::FocusColor).color());
        setHoverColor(s.decoration(KColorScheme::HoverColor).color());
    }
};

// Square corners, as the plates are
class SquareUnits : public Units
{
public:
    explicit SquareUnits(QObject *parent)
        : Units(parent)
    {
        setcornerRadius(0);
    }
};

class Factory : public PlatformPluginFactory
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID PlatformPluginFactory_iid FILE "pouplatform.json")
    Q_INTERFACES(Kirigami::Platform::PlatformPluginFactory)

public:
    PlatformTheme *createPlatformTheme(QObject *parent) override { return new Theme(parent); }
    Units *createUnits(QObject *parent) override { return new SquareUnits(parent); }
};

} // namespace

#include "pouplatform.moc"
