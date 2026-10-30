// SPDX-License-Identifier: MIT
#include "app/popuphider.h"

#include "platform/gameinput.h"

namespace pou {
namespace {

// how far from the popup the mouse can go and still be near it
constexpr int nearby = 260;
// how far off it the mouse can be and still have the game see the popup
constexpr int margin = 4;

} // namespace

PopupHider::PopupHider(GameInput *input, QObject *parent)
    : QObject(parent)
    , m_input(input)
{
    m_hide.setSingleShot(true);
    connect(&m_hide, &QTimer::timeout, this, &PopupHider::hide);
    connect(&m_look, &QTimer::timeout, this, &PopupHider::look);
    setTimeouts({});
}

void PopupHider::setTimeouts(const Timeouts &timeouts)
{
    m_timeouts = timeouts;
    m_look.setInterval(timeouts.look);
}

void PopupHider::shown(const QString &screen, const QRect &geometry, bool byMouse)
{
    m_shown = true;
    m_screen = screen;
    m_geometry = geometry;
    m_away.invalidate();
    if (byMouse)
        m_look.start();
    else
        m_look.stop();
    if (!m_hovered)
        m_hide.start(m_timeouts.shown);
}

void PopupHider::setHovered(bool hovered)
{
    if (hovered == m_hovered)
        return;
    m_hovered = hovered;
    if (hovered)
        m_hide.stop();
    else if (m_shown)
        m_hide.start(m_timeouts.left);
}

bool PopupHider::isInTheWay() const
{
    if (!m_shown)
        return false;
    // the mouse isn't known over the popup itself
    if (m_hovered)
        return true;
    const std::optional<ScreenPoint> mouse = m_input->mouse();
    return mouse && mouse->screen == m_screen &&
           m_geometry.adjusted(-margin, -margin, margin, margin).contains(mouse->position);
}

void PopupHider::hidden()
{
    m_shown = false;
    m_hovered = false;
    m_hide.stop();
    m_look.stop();
}

void PopupHider::look()
{
    const std::optional<ScreenPoint> mouse = m_input->mouse();
    // nothing to go on over a window the game's X server doesn't know, like
    // the popup itself
    const bool away = !m_hovered && mouse &&
                      (mouse->screen != m_screen ||
                       !m_geometry.adjusted(-nearby, -nearby, nearby, nearby).contains(mouse->position));
    if (!away)
        m_away.invalidate();
    else if (!m_away.isValid())
        m_away.start();
    else if (m_away.hasExpired(m_timeouts.away.count()))
        hide();
}

void PopupHider::hide()
{
    hidden();
    Q_EMIT timeToHide();
}

} // namespace pou
