// SPDX-License-Identifier: MIT
#include "platform/win/wingameinput.h"

#include "platform/gamewindow.h"
#include "platform/logging.h"
#include "platform/win/winkeysender.h"

#include <QClipboard>
#include <QCursor>
#include <QFileInfo>
#include <QGuiApplication>
#include <QScreen>
#include <iterator>
#include <windows.h>

using namespace std::chrono_literals;

namespace pou {
namespace {

// how long the game gets to come to the front, in milliseconds
constexpr qint64 bringForwardTime = 500;
// how long the popup gets to go from under the mouse, in milliseconds
constexpr qint64 popupGoneTime = 250;
// and to copy once the keys are pressed, which takes it a while when it's busy
constexpr auto copyTime = 1500ms;

HWND windowUnderMouse()
{
    POINT point;
    if (!GetCursorPos(&point))
        return nullptr;
    // the window itself, not the part of it under the mouse
    const HWND window = WindowFromPoint(point);
    return window ? GetAncestor(window, GA_ROOT) : nullptr;
}

DWORD processOf(HWND window)
{
    DWORD process = 0;
    GetWindowThreadProcessId(window, &process);
    return process;
}

WindowInfo describeWindow(HWND window)
{
    WindowInfo info;
    if (!window)
        return info;
    wchar_t className[256];
    if (const int length = GetClassNameW(window, className, std::size(className)); length > 0)
        info.classes << QString::fromWCharArray(className, length);

    const HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processOf(window));
    if (!process)
        return info;
    wchar_t path[MAX_PATH];
    DWORD length = std::size(path);
    if (QueryFullProcessImageNameW(process, 0, path, &length))
        info.program = QFileInfo(QString::fromWCharArray(path, int(length))).fileName();
    CloseHandle(process);
    return info;
}

// What to call a window that isn't the game: its program, without the .exe.
QString windowName(const WindowInfo &window)
{
    return QFileInfo(window.program).completeBaseName();
}

} // namespace

WinGameInput::WinGameInput(QObject *parent)
    : GameInput(parent)
    , m_keys(new WinKeySender(this))
{
    m_poll.setInterval(20ms);
    connect(&m_poll, &QTimer::timeout, this, &WinGameInput::poll);
    m_timeout.setSingleShot(true);
    m_timeout.setInterval(copyTime);
    connect(&m_timeout, &QTimer::timeout, this,
            [this] { fail(m_overGame ? Failure::NothingCopied : Failure::NotOverGame); });
    connect(m_keys, &WinKeySender::sent, this, &WinGameInput::keysSent);
    connect(m_keys, &WinKeySender::nudged, this, &WinGameInput::nudged);
    connect(m_keys, &WinKeySender::failed, this, [this](const QString &why) { fail(Failure::KeyHeld, why); });
    connect(QGuiApplication::clipboard(), &QClipboard::dataChanged, this, &WinGameInput::clipboardChanged);
}

void WinGameInput::copyItem(const CopyOptions &options)
{
    if (m_step != Step::Idle || m_keys->isBusy())
        return;
    m_advanced = options.advanced;
    m_nudge = options.nudge;
    if (m_nudge) {
        m_mouse.reset();
        m_step = Step::PopupGoing;
        m_since.start();
        m_poll.start();
        return;
    }
    findGame();
}

std::optional<ScreenPoint> WinGameInput::mouse() const
{
    // Qt's own, so it's scaled as its screens are
    const QPoint position = QCursor::pos();
    const QScreen *screen = QGuiApplication::screenAt(position);
    if (!screen)
        return std::nullopt;
    return ScreenPoint{.screen = screen->name(), .position = position - screen->geometry().topLeft()};
}

void WinGameInput::findGame()
{
    const HWND under = windowUnderMouse();
    m_overGame = under && isGameWindow(describeWindow(under));
    m_mouse = mouse();

    const HWND inFront = GetForegroundWindow();
    const WindowInfo front = describeWindow(inFront);
    qCDebug(lcInput) << "In front:" << front.classes << front.program
                     << "with the mouse over the game:" << m_overGame;
    if (isGameWindow(front)) {
        press();
    } else if (m_overGame) {
        // as clicking on it would bring it to the front, which Windows lets
        // us do as the hotkey was pressed for us
        m_step = Step::BringingForward;
        m_game = quintptr(under);
        SetForegroundWindow(under);
        m_since.start();
        m_poll.start();
    } else {
        fail(Failure::NotInFront, windowName(front));
    }
}

void WinGameInput::poll()
{
    if (m_step == Step::PopupGoing)
        checkPopupGone();
    else
        checkInFront();
}

void WinGameInput::checkPopupGone()
{
    const HWND under = windowUnderMouse();
    if (under && processOf(under) == GetCurrentProcessId() && !m_since.hasExpired(popupGoneTime))
        return;
    m_poll.stop();
    findGame();
}

void WinGameInput::checkInFront()
{
    const HWND active = GetForegroundWindow();
    if (quintptr(active) == m_game) {
        m_poll.stop();
        press();
    } else if (m_since.hasExpired(bringForwardTime)) {
        fail(Failure::NotInFront, windowName(describeWindow(active)));
    }
}

void WinGameInput::press()
{
    if (m_nudge) {
        m_step = Step::Nudging;
        m_keys->nudge();
        return;
    }
    m_step = Step::Pressing;
    m_keys->copy(m_advanced);
}

void WinGameInput::nudged()
{
    if (m_step != Step::Nudging)
        return;
    m_step = Step::Pressing;
    m_keys->copy(m_advanced);
}

void WinGameInput::keysSent()
{
    // unless the copy came while the keys were going up
    if (m_step == Step::Pressing) {
        m_step = Step::Waiting;
        m_timeout.start();
    }
}

void WinGameInput::clipboardChanged()
{
    const QString text = QGuiApplication::clipboard()->text();
    if (text.isEmpty())
        return;
    if (m_step == Step::Pressing || m_step == Step::Waiting) {
        m_step = Step::Idle;
        m_timeout.stop();
        Q_EMIT itemCopied(text, m_mouse);
    } else {
        Q_EMIT textCopied(text);
    }
}

void WinGameInput::fail(Failure why, const QString &detail)
{
    m_step = Step::Idle;
    m_poll.stop();
    m_timeout.stop();
    qCDebug(lcInput) << "Nothing was copied:" << why << detail;
    Q_EMIT copyFailed(why, detail, m_mouse);
}

} // namespace pou
