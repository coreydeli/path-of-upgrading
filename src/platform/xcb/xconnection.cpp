// SPDX-License-Identifier: MIT
#include "platform/xcb/xconnection.h"

#include <limits>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

QString connectionError(int error)
{
    const QString display = qEnvironmentVariable("DISPLAY");
    if (display.isEmpty())
        return u"DISPLAY isn't set, so there's no X server or Xwayland to connect to."_s;
    if (error == XCB_CONN_CLOSED_PARSE_ERR)
        return u"DISPLAY is set to %1, which doesn't name a display."_s.arg(display);
    return u"Can't connect to the X server on display %1."_s.arg(display);
}

} // namespace

XConnection::XConnection()
{
    int screen = 0;
    m_xcb = xcb_connect(nullptr, &screen);
    if (const int error = xcb_connection_has_error(m_xcb)) {
        m_errorString = connectionError(error);
        return;
    }
    xcb_screen_iterator_t it = xcb_setup_roots_iterator(xcb_get_setup(m_xcb));
    for (; screen > 0 && it.rem > 0; --screen)
        xcb_screen_next(&it);
    if (it.rem > 0)
        m_root = it.data->root;
    else
        m_errorString = connectionError(XCB_CONN_CLOSED_INVALID_SCREEN);
}

XConnection::~XConnection()
{
    // a connection that failed needs it too
    xcb_disconnect(m_xcb);
}

QString XConnection::errorString() const
{
    if (m_errorString.isEmpty() && xcb_connection_has_error(m_xcb))
        return u"Lost the connection to the X server."_s;
    return m_errorString;
}

xcb_atom_t XConnection::atom(QByteArrayView name)
{
    const QByteArray key = name.toByteArray();
    if (const auto it = m_atoms.constFind(key); it != m_atoms.cend())
        return *it;
    xcb_generic_error_t *error = nullptr;
    const XReply<xcb_intern_atom_reply_t> reply(
        xcb_intern_atom_reply(m_xcb, xcb_intern_atom(m_xcb, false, key.size(), key.constData()), &error));
    std::free(error);
    if (!reply)
        return XCB_ATOM_NONE;
    m_atoms.insert(key, reply->atom);
    return reply->atom;
}

QList<quint32> XConnection::values(xcb_window_t window, xcb_atom_t property, xcb_atom_t type)
{
    const auto reply = getProperty(window, property, type, 32);
    if (!reply)
        return {};
    const auto *values = static_cast<const quint32 *>(xcb_get_property_value(reply.get()));
    return QList<quint32>(values, values + xcb_get_property_value_length(reply.get()) / 4);
}

QByteArray XConnection::bytes(xcb_window_t window, xcb_atom_t property, xcb_atom_t type)
{
    const auto reply = getProperty(window, property, type, 8);
    if (!reply)
        return {};
    return QByteArray(static_cast<const char *>(xcb_get_property_value(reply.get())),
                      xcb_get_property_value_length(reply.get()));
}

XReply<xcb_get_property_reply_t> XConnection::getProperty(xcb_window_t window, xcb_atom_t property,
                                                          xcb_atom_t type, int format)
{
    // the whole value, however long it is
    const xcb_get_property_cookie_t cookie =
        xcb_get_property(m_xcb, false, window, property, type, 0, std::numeric_limits<uint32_t>::max() / 4);
    // an error, like the window being gone, comes back here rather than as an event
    xcb_generic_error_t *error = nullptr;
    XReply<xcb_get_property_reply_t> reply(xcb_get_property_reply(m_xcb, cookie, &error));
    std::free(error);
    if (!reply || reply->format != format || (type != XCB_ATOM_ANY && reply->type != type))
        return {};
    return reply;
}

} // namespace pou
