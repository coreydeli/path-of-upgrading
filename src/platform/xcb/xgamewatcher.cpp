// SPDX-License-Identifier: MIT
#include "platform/xcb/xgamewatcher.h"

#include "platform/xcb/xwindows.h"

namespace pou {

XGameWatcher::XGameWatcher(QObject *parent)
    : GameWatcher(parent)
{
    if (!m_x.isValid())
        return;
    // to hear when windows come and go
    const uint32_t mask = XCB_EVENT_MASK_PROPERTY_CHANGE;
    xcb_change_window_attributes(m_x.xcb(), m_x.root(), XCB_CW_EVENT_MASK, &mask);
    xcb_flush(m_x.xcb());
    look();
    m_notifier.setSocket(xcb_get_file_descriptor(m_x.xcb()));
    connect(&m_notifier, &QSocketNotifier::activated, this, &XGameWatcher::readEvents);
    m_notifier.setEnabled(true);
}

void XGameWatcher::readEvents()
{
    bool listChanged = false;
    const xcb_atom_t clientList = m_x.atom("_NET_CLIENT_LIST");
    // waiting on a reply reads the events that come before it, so those
    // are here too and not on the socket
    while (const XReply<xcb_generic_event_t> event{xcb_poll_for_event(m_x.xcb())}) {
        if ((event->response_type & 0x7f) != XCB_PROPERTY_NOTIFY)
            continue;
        const auto &changed = reinterpret_cast<const xcb_property_notify_event_t &>(*event);
        if (changed.window == m_x.root() && changed.atom == clientList)
            listChanged = true;
    }
    if (!m_x.isValid()) {
        // with no way to tell, the hotkeys are better on than gone
        m_notifier.setEnabled(false);
        if (!m_running) {
            m_running = true;
            Q_EMIT runningChanged(true);
        }
        return;
    }
    if (listChanged)
        look();
}

void XGameWatcher::look()
{
    const QList<quint32> windows = m_x.values(m_x.root(), m_x.atom("_NET_CLIENT_LIST"), XCB_ATOM_WINDOW);
    const bool running = anyGameWindow(m_x, windows);
    if (running == m_running)
        return;
    m_running = running;
    Q_EMIT runningChanged(running);
}

} // namespace pou
