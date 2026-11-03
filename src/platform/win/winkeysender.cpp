// SPDX-License-Identifier: MIT
#include "platform/win/winkeysender.h"

#include "platform/win/winkeys.h"

#include <QTimer>
#include <windows.h>

using namespace Qt::Literals::StringLiterals;
using namespace std::chrono_literals;

namespace pou {
namespace {

// how long the hotkey's keys get to go up, in milliseconds
constexpr qint64 letGoTime = 1500;
// how long the mouse is a pixel away from where it was
constexpr auto nudgeTime = 20ms;
// and the game gets to show what's under it once it's back
constexpr auto redrawTime = 70ms;

bool isDown(quint32 key)
{
    return GetAsyncKeyState(int(key)) & 0x8000;
}

void send(const KeyStep &step)
{
    INPUT input = {};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = WORD(step.key);
    // games often go by where the key is rather than what it is
    input.ki.wScan = WORD(MapVirtualKeyW(step.key, MAPVK_VK_TO_VSC));
    input.ki.dwFlags = step.press ? 0 : KEYEVENTF_KEYUP;
    SendInput(1, &input, sizeof(INPUT));
}

// A step of the pointer from where it is, in pixels to the right.
void moveMouse(int dx)
{
    POINT point;
    if (GetCursorPos(&point))
        SetCursorPos(point.x + dx, point.y);
}

} // namespace

WinKeySender::WinKeySender(QObject *parent)
    : QObject(parent)
{}

WinKeySender::~WinKeySender()
{
    for (const KeyStep &step : std::as_const(m_steps)) {
        if (!step.press)
            send(step);
    }
}

void WinKeySender::copy(bool advanced)
{
    if (m_busy)
        return;
    m_busy = true;
    m_advanced = advanced;
    m_waiting.start();
    waitForKeys();
}

void WinKeySender::nudge()
{
    if (m_busy)
        return;
    m_busy = true;
    moveMouse(1);
    QTimer::singleShot(nudgeTime, this, [this] {
        moveMouse(-1);
        QTimer::singleShot(redrawTime, this, [this] {
            m_busy = false;
            Q_EMIT nudged();
        });
    });
}

void WinKeySender::waitForKeys()
{
    QList<HeldModifier> held;
    bool othersHeld = false;
    for (quint32 key = 1; key < 0xff; ++key) {
        if (!winCountsAsHeld(key) || !isDown(key))
            continue;
        if (const std::optional<Modifier> modifier = winModifier(key))
            held << HeldModifier{.key = key, .modifier = *modifier};
        else
            othersHeld = true;
    }
    // or the game would see them with ours
    if (othersHeld && !m_waiting.hasExpired(letGoTime)) {
        QTimer::singleShot(10ms, this, &WinKeySender::waitForKeys);
        return;
    }
    if (othersHeld) {
        m_busy = false;
        Q_EMIT failed(u"A key stayed down, so the game wasn't asked to copy."_s);
        return;
    }
    m_steps = copySteps(winCopyKeys(), held, m_advanced);
    sendSteps();
}

void WinKeySender::sendSteps()
{
    while (!m_steps.isEmpty()) {
        // a step waits once, and goes the next time round
        if (const auto wait = std::exchange(m_steps.first().after, 0ms); wait > 0ms) {
            QTimer::singleShot(wait, this, &WinKeySender::sendSteps);
            return;
        }
        send(m_steps.takeFirst());
    }
    m_busy = false;
    Q_EMIT sent();
}

} // namespace pou
