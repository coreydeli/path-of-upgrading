// SPDX-License-Identifier: MIT
#include "platform/xcb/xkeysender.h"

#include <QHash>
#include <QTimer>
#include <X11/keysym.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xtest.h>

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

QList<xcb_keycode_t> heldKeys(xcb_connection_t *c)
{
    const XReply<xcb_query_keymap_reply_t> keymap(xcb_query_keymap_reply(c, xcb_query_keymap(c), nullptr));
    QList<xcb_keycode_t> held;
    if (!keymap)
        return held;
    for (int code = 0; code < 256; ++code) {
        if (keymap->keys[code / 8] & (1 << (code % 8)))
            held << xcb_keycode_t(code);
    }
    return held;
}

QHash<xcb_keycode_t, Modifier> modifierKeys(xcb_connection_t *c)
{
    const XReply<xcb_get_modifier_mapping_reply_t> mapping(
        xcb_get_modifier_mapping_reply(c, xcb_get_modifier_mapping(c), nullptr));
    QHash<xcb_keycode_t, Modifier> modifiers;
    if (!mapping)
        return modifiers;
    const xcb_keycode_t *codes = xcb_get_modifier_mapping_keycodes(mapping.get());
    const int perModifier = mapping->keycodes_per_modifier;
    for (int index = XCB_MAP_INDEX_SHIFT; index <= XCB_MAP_INDEX_5; ++index) {
        // Mod1 is Alt
        const Modifier modifier = index == XCB_MAP_INDEX_CONTROL ? Modifier::Control
                                  : index == XCB_MAP_INDEX_1     ? Modifier::Alt
                                                                 : Modifier::Other;
        for (int i = 0; i < perModifier; ++i) {
            if (const xcb_keycode_t code = codes[index * perModifier + i])
                modifiers.insert(code, modifier);
        }
    }
    return modifiers;
}

} // namespace

XKeySender::XKeySender(QObject *parent)
    : QObject(parent)
{
    if (!m_x.isValid())
        return;
    const xcb_query_extension_reply_t *xtest = xcb_get_extension_data(m_x.xcb(), &xcb_test_id);
    if (!xtest || !xtest->present)
        m_errorString = u"The X server has no XTest, which it needs to press keys for us."_s;
}

XKeySender::~XKeySender()
{
    for (const KeyStep &step : std::as_const(m_steps)) {
        if (!step.press)
            send(step);
    }
}

bool XKeySender::isValid() const
{
    return m_errorString.isEmpty() && m_x.isValid();
}

QString XKeySender::errorString() const
{
    return m_errorString.isEmpty() ? m_x.errorString() : m_errorString;
}

void XKeySender::copy(bool advanced)
{
    if (m_busy || !isValid())
        return;
    m_busy = true;
    m_advanced = advanced;
    m_waiting.start();
    waitForKeys();
}

void XKeySender::nudge()
{
    if (m_busy || !isValid())
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

CopyKeys XKeySender::copyKeys()
{
    // where they are on a PC keyboard, for layouts that don't have them, like
    // a Russian one without a Latin C
    CopyKeys keys = {.control = 37, .alt = 64, .c = 54};
    xcb_key_symbols_t *symbols = xcb_key_symbols_alloc(m_x.xcb());
    const auto lookUp = [symbols](xcb_keysym_t keysym, quint32 &key) {
        const XReply<xcb_keycode_t> codes(xcb_key_symbols_get_keycode(symbols, keysym));
        if (codes && *codes != XCB_NO_SYMBOL)
            key = *codes;
    };
    lookUp(XK_Control_L, keys.control);
    lookUp(XK_Alt_L, keys.alt);
    lookUp(XK_c, keys.c);
    xcb_key_symbols_free(symbols);
    return keys;
}

void XKeySender::waitForKeys()
{
    const QHash<xcb_keycode_t, Modifier> modifiers = modifierKeys(m_x.xcb());
    QList<HeldModifier> held;
    bool othersHeld = false;
    for (const xcb_keycode_t code : heldKeys(m_x.xcb())) {
        if (const auto modifier = modifiers.constFind(code); modifier != modifiers.cend())
            held << HeldModifier{.key = code, .modifier = *modifier};
        else
            othersHeld = true;
    }
    // On an X11 desktop the hotkey's key has the whole keyboard while it's
    // down, and what we press goes to the program that took it.
    if (othersHeld && !m_waiting.hasExpired(letGoTime)) {
        QTimer::singleShot(10ms, this, &XKeySender::waitForKeys);
        return;
    }
    if (othersHeld) {
        m_busy = false;
        Q_EMIT failed(u"A key stayed down, so the game wasn't asked to copy."_s);
        return;
    }
    m_steps = copySteps(copyKeys(), held, m_advanced);
    sendSteps();
}

void XKeySender::sendSteps()
{
    while (!m_steps.isEmpty()) {
        // a step waits once, and goes the next time round
        if (const auto wait = std::exchange(m_steps.first().after, 0ms); wait > 0ms) {
            QTimer::singleShot(wait, this, &XKeySender::sendSteps);
            return;
        }
        send(m_steps.takeFirst());
    }
    m_busy = false;
    Q_EMIT sent();
}

void XKeySender::send(const KeyStep &step)
{
    xcb_test_fake_input(m_x.xcb(), step.press ? XCB_KEY_PRESS : XCB_KEY_RELEASE, xcb_keycode_t(step.key),
                        XCB_CURRENT_TIME, XCB_WINDOW_NONE, 0, 0, 0);
    xcb_flush(m_x.xcb());
}

// A step of the pointer from where it is, in pixels to the right.
void XKeySender::moveMouse(int dx)
{
    // the 1 is for a move from where it is, not to a place
    xcb_test_fake_input(m_x.xcb(), XCB_MOTION_NOTIFY, 1, XCB_CURRENT_TIME, XCB_WINDOW_NONE, int16_t(dx), 0,
                        0);
    xcb_flush(m_x.xcb());
}

} // namespace pou
