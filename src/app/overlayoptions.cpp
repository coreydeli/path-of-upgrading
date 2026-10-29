// SPDX-License-Identifier: MIT
#include "app/overlayoptions.h"

#include "app/hotkeys.h"
#include "app/settingsstore.h"
#include "platform/autostart.h"

#include <algorithm>
#include <cmath>

namespace pou {
namespace {

QKeySequence sequenceOf(const QString &key)
{
    return QKeySequence::fromString(key, QKeySequence::PortableText);
}

QString textOf(const QKeySequence &key)
{
    return key.toString(QKeySequence::PortableText);
}

} // namespace

OverlayOptions::OverlayOptions(SettingsStore *settings, Hotkeys *hotkeys, Autostart *autostart,
                               QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_hotkeys(hotkeys)
    , m_autostart(autostart)
{
    connect(settings, &SettingsStore::changed, this, &OverlayOptions::changed);
    connect(hotkeys, &Hotkeys::keyChanged, this, &OverlayOptions::changed);
}

const OverlaySettings &OverlayOptions::overlay() const
{
    return m_settings->settings().overlay;
}

template<typename Edit>
void OverlayOptions::change(Edit edit)
{
    Settings settings = m_settings->settings();
    edit(settings.overlay);
    // which does nothing when it's the same
    m_settings->set(settings);
}

QKeySequence OverlayOptions::checkKey() const
{
    return sequenceOf(m_hotkeys->key(Hotkeys::Hotkey::Check));
}

QKeySequence OverlayOptions::resultsKey() const
{
    return sequenceOf(m_hotkeys->key(Hotkeys::Hotkey::Results));
}

bool OverlayOptions::setCheckKey(const QKeySequence &key)
{
    return m_hotkeys->setKey(Hotkeys::Hotkey::Check, textOf(key));
}

bool OverlayOptions::setResultsKey(const QKeySequence &key)
{
    return m_hotkeys->setKey(Hotkeys::Hotkey::Results, textOf(key));
}

QString OverlayOptions::hotkeysError() const
{
    return m_hotkeys->errorString();
}

void OverlayOptions::setAdvancedCopy(bool advanced)
{
    change([&](OverlaySettings &overlay) { overlay.advancedCopy = advanced; });
}

void OverlayOptions::setWatchClipboard(bool watch)
{
    change([&](OverlaySettings &overlay) { overlay.watchClipboard = watch; });
}

QString OverlayOptions::position() const
{
    return popupPositionId(overlay().position);
}

void OverlayOptions::setPosition(const QString &position)
{
    change([&](OverlaySettings &overlay) { overlay.position = popupPositionFromId(position); });
}

void OverlayOptions::setScale(double scale)
{
    if (std::isfinite(scale))
        change([&](OverlaySettings &overlay) { overlay.scale = std::clamp(scale, 0.5, 2.0); });
}

void OverlayOptions::setCompact(bool compact)
{
    change([&](OverlaySettings &overlay) { overlay.compact = compact; });
}

void OverlayOptions::setColorblind(bool colorblind)
{
    change([&](OverlaySettings &overlay) { overlay.colorblind = colorblind; });
}

void OverlayOptions::setAutoHideSeconds(int seconds)
{
    change([&](OverlaySettings &overlay) { overlay.autoHideSeconds = std::clamp(seconds, 2, 120); });
}

void OverlayOptions::setPaused(bool paused)
{
    change([&](OverlaySettings &overlay) { overlay.paused = paused; });
}

bool OverlayOptions::startsWithDesktop() const
{
    return m_autostart->isEnabled();
}

void OverlayOptions::setStartWithDesktop(bool start)
{
    m_autostart->setEnabled(start);
    // even when it didn't work, for the switch to go back and say why
    Q_EMIT changed();
}

QString OverlayOptions::startError() const
{
    return m_autostart->errorString();
}

void OverlayOptions::lookAtStart()
{
    Q_EMIT changed();
}

} // namespace pou
