// SPDX-License-Identifier: MIT
#pragma once

#include <QByteArray>
#include <QByteArrayView>
#include <QHash>
#include <QList>
#include <QString>
#include <cstdlib>
#include <memory>
#include <xcb/xcb.h>

namespace pou {

// Replies from xcb are ours to free.
struct FreeReply
{
    void operator()(void *reply) const { std::free(reply); }
};

template<typename T>
using XReply = std::unique_ptr<T, FreeReply>;

// A connection of our own to the X server. On a Wayland desktop that's
// Xwayland, where the game runs as an X11 window, and this is how we find its
// window, the mouse over it and what it copies. Qt talks to the Wayland
// compositor instead, which keeps other windows and the mouse to itself and
// only shares the clipboard with the window that has focus.
class XConnection
{
public:
    // To the display in $DISPLAY.
    XConnection();
    ~XConnection();

    XConnection(const XConnection &) = delete;
    XConnection &operator=(const XConnection &) = delete;

    bool isValid() const { return m_root != XCB_WINDOW_NONE; }
    // Why it isn't, in words for people.
    QString errorString() const { return m_errorString; }

    xcb_connection_t *xcb() const { return m_xcb; }
    xcb_window_t root() const { return m_root; }

    // Interned the first time it's asked for.
    xcb_atom_t atom(QByteArrayView name);

    // A window property of 32-bit values, like _NET_CLIENT_LIST. Empty when
    // the window doesn't have it in that type.
    QList<quint32> values(xcb_window_t window, xcb_atom_t property, xcb_atom_t type);
    // A property of bytes, like WM_CLASS.
    QByteArray bytes(xcb_window_t window, xcb_atom_t property, xcb_atom_t type);

private:
    XReply<xcb_get_property_reply_t> getProperty(xcb_window_t window, xcb_atom_t property, xcb_atom_t type,
                                                 int format);

    xcb_connection_t *m_xcb = nullptr;
    xcb_window_t m_root = XCB_WINDOW_NONE;
    QString m_errorString;
    QHash<QByteArray, xcb_atom_t> m_atoms;
};

} // namespace pou
