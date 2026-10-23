// SPDX-License-Identifier: MIT
#include "platform/xcb/xgameinput.h"

#include "platform/logging.h"
#include "platform/xcb/xkeysender.h"
#include "platform/xcb/xmonitors.h"
#include "platform/xcb/xselectionwatcher.h"
#include "platform/xcb/xwindows.h"

#include <QGuiApplication>
#include <QScreen>

using namespace std::chrono_literals;

namespace pou {
namespace {

// how long the game gets to come to the front, in milliseconds
constexpr qint64 bringForwardTime = 500;
// and to copy once the keys are pressed, which takes it a while when it's busy
constexpr auto copyTime = 1500ms;

QList<NamedScreen> qtScreens()
{
    QList<NamedScreen> screens;
    for (const QScreen *screen : QGuiApplication::screens())
        screens.append({.name = screen->name(), .geometry = screen->geometry()});
    return screens;
}

// What to call a window that isn't the game, when it says.
QString windowName(const WindowInfo &window)
{
    return window.classes.isEmpty() ? window.program : window.classes.last();
}

} // namespace

XGameInput::XGameInput(QObject *parent)
    : GameInput(parent)
{
    m_reconnect.setSingleShot(true);
    m_reconnect.setInterval(5s);
    connect(&m_reconnect, &QTimer::timeout, this, &XGameInput::reconnect);
    m_poll.setInterval(20ms);
    connect(&m_poll, &QTimer::timeout, this, &XGameInput::checkInFront);
    m_timeout.setSingleShot(true);
    m_timeout.setInterval(copyTime);
    connect(&m_timeout, &QTimer::timeout, this,
            [this] { fail(m_overGame ? Failure::NothingCopied : Failure::NotOverGame); });
    connectToServer();
}

bool XGameInput::isAvailable() const
{
    return m_x->isValid() && m_watcher->isValid() && m_keys->isValid();
}

QString XGameInput::errorString() const
{
    if (!m_x->isValid())
        return m_x->errorString();
    if (!m_watcher->isValid())
        return m_watcher->errorString();
    return m_keys->errorString();
}

void XGameInput::copyItem(bool advanced)
{
    if (m_step != Step::Idle || m_keys->isBusy())
        return;
    // the server can be back since it went
    if (!isAvailable())
        connectToServer();
    if (!isAvailable()) {
        m_mouse.reset();
        fail(Failure::Unavailable, errorString());
        return;
    }
    m_advanced = advanced;

    // Over a Wayland window, the X server doesn't know where the mouse is.
    const XPointer pointer = queryPointer(*m_x);
    m_overGame = pointer.window != XCB_WINDOW_NONE && isGameWindow(describeWindow(*m_x, pointer.window));
    m_mouse = pointer.window == XCB_WINDOW_NONE ? std::nullopt
                                                : mapToScreen(pointer.position, xMonitors(*m_x), qtScreens());

    const WindowInfo inFront = describeWindow(*m_x, activeWindow(*m_x));
    qCDebug(lcInput) << "In front:" << inFront.classes << inFront.program
                     << "with the mouse over the game:" << m_overGame;
    if (isGameWindow(inFront)) {
        press();
    } else if (m_overGame) {
        // as clicking on it would bring it to the front
        m_step = Step::BringingForward;
        m_game = pointer.window;
        activate(*m_x, m_game);
        m_since.start();
        m_poll.start();
    } else {
        fail(Failure::NotInFront, windowName(inFront));
    }
}

void XGameInput::connectToServer()
{
    m_reconnect.stop();
    // the old ones go once they're done with what they're doing
    if (m_watcher) {
        m_watcher->disconnect(this);
        m_watcher->deleteLater();
    }
    if (m_keys) {
        m_keys->disconnect(this);
        m_keys->deleteLater();
    }
    m_x = std::make_unique<XConnection>();
    m_watcher = new XSelectionWatcher("CLIPBOARD", this);
    m_keys = new XKeySender(this);
    connect(m_watcher, &XSelectionWatcher::textCopied, this, &XGameInput::copied);
    connect(m_watcher, &XSelectionWatcher::disconnected, this, [this] {
        qCWarning(lcInput, "Lost the X server, and will connect to it again in a while");
        if (m_step != Step::Idle)
            fail(Failure::Unavailable, m_watcher->errorString());
        m_reconnect.start();
    });
    connect(m_keys, &XKeySender::sent, this, &XGameInput::keysSent);
    connect(m_keys, &XKeySender::failed, this, [this](const QString &why) { fail(Failure::KeyHeld, why); });
    if (!isAvailable())
        qCWarning(lcInput, "Can't copy items out of the game: %s", qPrintable(errorString()));
}

// KWin starts Xwayland again when it crashes, which takes the game with it.
void XGameInput::reconnect()
{
    connectToServer();
    if (!m_x->isValid())
        m_reconnect.start();
}

void XGameInput::checkInFront()
{
    const xcb_window_t active = activeWindow(*m_x);
    if (active == m_game) {
        m_poll.stop();
        press();
    } else if (m_since.hasExpired(bringForwardTime)) {
        fail(Failure::NotInFront, windowName(describeWindow(*m_x, active)));
    }
}

void XGameInput::press()
{
    m_step = Step::Pressing;
    m_keys->copy(m_advanced);
}

void XGameInput::keysSent()
{
    // unless the copy came while the keys were going up
    if (m_step == Step::Pressing) {
        m_step = Step::Waiting;
        m_timeout.start();
    }
}

void XGameInput::copied(const QString &text)
{
    if (m_step == Step::Pressing || m_step == Step::Waiting) {
        m_step = Step::Idle;
        m_timeout.stop();
        Q_EMIT itemCopied(text, m_mouse);
    } else {
        Q_EMIT textCopied(text);
    }
}

void XGameInput::fail(Failure why, const QString &detail)
{
    m_step = Step::Idle;
    m_poll.stop();
    m_timeout.stop();
    qCDebug(lcInput) << "Nothing was copied:" << why << detail;
    Q_EMIT copyFailed(why, detail, m_mouse);
}

} // namespace pou
