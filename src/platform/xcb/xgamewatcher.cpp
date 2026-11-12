// SPDX-License-Identifier: MIT
#include "platform/xcb/xgamewatcher.h"

#include "platform/xcb/xwindows.h"

using namespace std::chrono_literals;

namespace pou {

XGameWatcher::XGameWatcher(QObject *parent)
    : GameWatcher(parent)
    , m_x(std::make_unique<XConnection>())
{
    m_reconnect.setSingleShot(true);
    m_reconnect.setInterval(5s);
    connect(&m_reconnect, &QTimer::timeout, this, &XGameWatcher::reconnect);
    connect(&m_notifier, &QSocketNotifier::activated, this, &XGameWatcher::readEvents);
    watch();
}

void XGameWatcher::watch()
{
    if (!m_x->isValid())
        return;
    // to hear when windows come and go
    const uint32_t mask = XCB_EVENT_MASK_PROPERTY_CHANGE;
    xcb_change_window_attributes(m_x->xcb(), m_x->root(), XCB_CW_EVENT_MASK, &mask);
    xcb_flush(m_x->xcb());
    look();
    m_notifier.setSocket(xcb_get_file_descriptor(m_x->xcb()));
    m_notifier.setEnabled(true);
}

void XGameWatcher::readEvents()
{
    bool listChanged = false;
    const xcb_atom_t clientList = m_x->atom("_NET_CLIENT_LIST");
    // waiting on a reply reads the events that come before it, so those
    // are here too and not on the socket
    while (const XReply<xcb_generic_event_t> event{xcb_poll_for_event(m_x->xcb())}) {
        if ((event->response_type & 0x7f) != XCB_PROPERTY_NOTIFY)
            continue;
        const auto &changed = reinterpret_cast<const xcb_property_notify_event_t &>(*event);
        if (changed.window == m_x->root() && changed.atom == clientList)
            listChanged = true;
    }
    if (!m_x->isValid()) {
        m_notifier.setEnabled(false);
        // with no way to tell, the hotkeys are better on than gone
        setRunning(true);
        m_reconnect.start();
        return;
    }
    if (listChanged)
        look();
}

// KWin starts Xwayland again when it crashes, which takes the game with it.
void XGameWatcher::reconnect()
{
    m_x = std::make_unique<XConnection>();
    if (!m_x->isValid()) {
        m_reconnect.start();
        return;
    }
    watch();
}

void XGameWatcher::look()
{
    const QList<quint32> windows = m_x->values(m_x->root(), m_x->atom("_NET_CLIENT_LIST"), XCB_ATOM_WINDOW);
    setRunning(anyGameWindow(*m_x, windows));
}

void XGameWatcher::setRunning(bool running)
{
    if (running == m_running)
        return;
    m_running = running;
    Q_EMIT runningChanged(running);
}

} // namespace pou
