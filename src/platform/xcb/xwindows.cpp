// SPDX-License-Identifier: MIT
#include "platform/xcb/xwindows.h"

#include <QFile>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

// The kernel's name for it, cut to 15 characters
QString processName(quint32 pid)
{
    QFile comm(u"/proc/%1/comm"_s.arg(pid));
    if (!comm.open(QIODevice::ReadOnly))
        return {};
    return QString::fromUtf8(comm.readAll()).trimmed();
}

} // namespace

xcb_window_t activeWindow(XConnection &x)
{
    return x.values(x.root(), x.atom("_NET_ACTIVE_WINDOW"), XCB_ATOM_WINDOW).value(0, XCB_WINDOW_NONE);
}

WindowInfo describeWindow(XConnection &x, xcb_window_t window)
{
    WindowInfo info;
    // the instance and then the class, each ending in a nul
    const QByteArray wmClass = x.bytes(window, XCB_ATOM_WM_CLASS, XCB_ATOM_STRING);
    for (const QByteArray &part : wmClass.split('\0')) {
        if (!part.isEmpty())
            info.classes << QString::fromLatin1(part);
    }
    const QList<quint32> pid = x.values(window, x.atom("_NET_WM_PID"), XCB_ATOM_CARDINAL);
    if (!pid.isEmpty())
        info.program = processName(pid.first());
    return info;
}

XPointer queryPointer(XConnection &x)
{
    XPointer pointer;
    const xcb_atom_t wmState = x.atom("WM_STATE");
    // Down from the root, through the window manager's frame, to the window
    // with WM_STATE, which is the program's. KWin keeps windows of its own
    // under the programs', which have none.
    xcb_window_t window = x.root();
    for (int depth = 0; depth < 8; ++depth) {
        const XReply<xcb_query_pointer_reply_t> reply(
            xcb_query_pointer_reply(x.xcb(), xcb_query_pointer(x.xcb(), window), nullptr));
        if (!reply)
            break;
        if (depth == 0)
            pointer.position = QPoint(reply->root_x, reply->root_y);
        if (reply->child == XCB_WINDOW_NONE)
            break;
        window = reply->child;
        if (!x.values(window, wmState, wmState).isEmpty()) {
            pointer.window = window;
            break;
        }
    }
    return pointer;
}

void activate(XConnection &x, xcb_window_t window)
{
    xcb_client_message_event_t message = {
        .response_type = XCB_CLIENT_MESSAGE,
        .format = 32,
        .window = window,
        .type = x.atom("_NET_ACTIVE_WINDOW"),
    };
    // asked for by a pager or taskbar, which window managers go along with
    message.data.data32[0] = 2;
    message.data.data32[1] = XCB_CURRENT_TIME;
    xcb_send_event(x.xcb(), false, x.root(),
                   XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT,
                   reinterpret_cast<const char *>(&message));
    xcb_flush(x.xcb());
}

} // namespace pou
