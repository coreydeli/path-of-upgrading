// SPDX-License-Identifier: MIT
#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QRect>
#include <QString>
#include <QTimer>
#include <chrono>

namespace pou {

class GameInput;

// Says when the popup should go: a while after it's shown, sooner once the
// mouse has been on it and left, and when you've taken the mouse well away
// from it, as you do to look at something else.
class PopupHider : public QObject
{
    Q_OBJECT

public:
    struct Timeouts
    {
        std::chrono::milliseconds shown = std::chrono::seconds(12);
        std::chrono::milliseconds left = std::chrono::milliseconds(2500);
        std::chrono::milliseconds away = std::chrono::milliseconds(1200);
        // how often to look where the mouse is
        std::chrono::milliseconds look = std::chrono::milliseconds(100);
    };

    // The input has to outlive the hider.
    explicit PopupHider(GameInput *input, QObject *parent = nullptr);

    const Timeouts &timeouts() const { return m_timeouts; }
    void setTimeouts(const Timeouts &timeouts);

    // The popup is shown, or shown again with something else, there on that
    // screen. Taking the mouse away only counts when it's next to the mouse.
    void shown(const QString &screen, const QRect &geometry, bool byMouse);
    void setHovered(bool hovered);
    // Stops it all, as the popup has gone some other way.
    void hidden();

Q_SIGNALS:
    void timeToHide();

private:
    void look();
    void hide();

    GameInput *m_input;
    Timeouts m_timeouts;
    QTimer m_hide;
    QTimer m_look;
    QElapsedTimer m_away; // since the mouse went away, invalid while it's near
    QString m_screen;
    QRect m_geometry;
    bool m_shown = false;
    bool m_hovered = false;
};

} // namespace pou
