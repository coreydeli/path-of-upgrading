// SPDX-License-Identifier: MIT
#include "platform/xcb/xselectionwatcher.h"

#include <xcb/xfixes.h>

using namespace Qt::Literals::StringLiterals;

namespace pou {

XSelectionWatcher::XSelectionWatcher(QByteArrayView selection, QObject *parent)
    : QObject(parent)
{
    if (!m_x.isValid())
        return;
    xcb_connection_t *c = m_x.xcb();
    const xcb_query_extension_reply_t *xfixes = xcb_get_extension_data(c, &xcb_xfixes_id);
    if (!xfixes || !xfixes->present) {
        m_errorString = u"The X server has no XFixes, which it needs to say when something is copied."_s;
        return;
    }
    // XFixes won't take requests from a client that hasn't said which
    // version it speaks
    const XReply<xcb_xfixes_query_version_reply_t> version(xcb_xfixes_query_version_reply(
        c, xcb_xfixes_query_version(c, XCB_XFIXES_MAJOR_VERSION, XCB_XFIXES_MINOR_VERSION), nullptr));
    if (!version) {
        m_errorString = u"The X server's XFixes didn't answer."_s;
        return;
    }
    m_ownerChanged = xfixes->first_event + XCB_XFIXES_SELECTION_NOTIFY;
    m_selection = m_x.atom(selection);
    m_property = m_x.atom("PATH_OF_UPGRADING_SELECTION");

    // to be told on, and for owners to put the text on
    m_window = xcb_generate_id(c);
    xcb_create_window(c, XCB_COPY_FROM_PARENT, m_window, m_x.root(), 0, 0, 1, 1, 0,
                      XCB_WINDOW_CLASS_INPUT_ONLY, XCB_COPY_FROM_PARENT, 0, nullptr);
    xcb_xfixes_select_selection_input(c, m_window, m_selection,
                                      XCB_XFIXES_SELECTION_EVENT_MASK_SET_SELECTION_OWNER);
    xcb_flush(c);

    m_notifier.setSocket(xcb_get_file_descriptor(c));
    connect(&m_notifier, &QSocketNotifier::activated, this, &XSelectionWatcher::readEvents);
    m_notifier.setEnabled(true);
}

bool XSelectionWatcher::isValid() const
{
    return m_window != XCB_WINDOW_NONE && m_x.isValid();
}

QString XSelectionWatcher::errorString() const
{
    return m_errorString.isEmpty() ? m_x.errorString() : m_errorString;
}

void XSelectionWatcher::readEvents()
{
    // waiting on a reply reads the events that come before it, so those
    // are here too and not on the socket
    while (const XReply<xcb_generic_event_t> event{xcb_poll_for_event(m_x.xcb())})
        handle(*event);
    // or the socket stays readable, and we'd be here for good
    if (!m_x.isValid()) {
        m_notifier.setEnabled(false);
        Q_EMIT disconnected();
    }
}

void XSelectionWatcher::handle(const xcb_generic_event_t &event)
{
    // the top bit says another client sent it
    const uint8_t type = event.response_type & 0x7f;
    if (type == m_ownerChanged) {
        const auto &changed = reinterpret_cast<const xcb_xfixes_selection_notify_event_t &>(event);
        xcb_convert_selection(m_x.xcb(), m_window, m_selection, m_x.atom("UTF8_STRING"), m_property,
                              changed.timestamp);
        xcb_flush(m_x.xcb());
    } else if (type == XCB_SELECTION_NOTIFY) {
        // none when the owner had no text to give
        const auto &converted = reinterpret_cast<const xcb_selection_notify_event_t &>(event);
        if (converted.property == XCB_ATOM_NONE)
            return;
        const QByteArray text = m_x.bytes(m_window, m_property, m_x.atom("UTF8_STRING"));
        xcb_delete_property(m_x.xcb(), m_window, m_property);
        xcb_flush(m_x.xcb());
        if (!text.isEmpty())
            Q_EMIT textCopied(QString::fromUtf8(text));
    }
}

} // namespace pou
