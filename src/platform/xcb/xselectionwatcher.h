// SPDX-License-Identifier: MIT
#pragma once

#include "platform/xcb/xconnection.h"

#include <QObject>
#include <QSocketNotifier>

namespace pou {

// Tells us whenever text gets copied, the way clipboard managers find out:
// the X server says when the clipboard has a new owner, and we ask the owner
// for its text.
class XSelectionWatcher : public QObject
{
    Q_OBJECT

public:
    // The clipboard unless told another selection, which the tests use to
    // stay off the real one.
    explicit XSelectionWatcher(QByteArrayView selection = "CLIPBOARD", QObject *parent = nullptr);

    bool isValid() const;
    QString errorString() const;

Q_SIGNALS:
    // For each copy, even of the same text again.
    void textCopied(const QString &text);
    // When the X server has gone, as Xwayland does when it crashes. It hears
    // of nothing after that.
    void disconnected();

private:
    void readEvents();
    void handle(const xcb_generic_event_t &event);

    XConnection m_x;
    QString m_errorString;
    uint8_t m_ownerChanged = 0;
    xcb_atom_t m_selection = XCB_ATOM_NONE;
    xcb_atom_t m_property = XCB_ATOM_NONE;
    xcb_window_t m_window = XCB_WINDOW_NONE;
    // after the connection, so it goes first
    QSocketNotifier m_notifier{QSocketNotifier::Read};
};

} // namespace pou
