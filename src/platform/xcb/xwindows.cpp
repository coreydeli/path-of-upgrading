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

} // namespace pou
