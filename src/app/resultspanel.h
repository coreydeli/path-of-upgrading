// SPDX-License-Identifier: MIT
#pragma once

#include "core/settings.h"

#include <QObject>
#include <QPoint>
#include <QString>
#include <QtQmlIntegration/qqmlintegration.h>
#include <memory>

class QQmlEngine;
class QQuickWindow;

namespace pou {

class GameInput;
class TradeResults;

// The window over the game with the listings of the last trade search, at
// the side of the screen. It stays over the game like the popup and never
// takes the keyboard from it, so it's the mouse that scrolls it, loads more
// and closes it, or the Results hotkey. Its window is ResultsWindow.qml.
class ResultsPanel : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("The app has the one results window.")

    Q_PROPERTY(pou::TradeResults *results READ results CONSTANT)
    Q_PROPERTY(double scale READ scale NOTIFY settingsChanged)
    Q_PROPERTY(bool colorblind READ isColorblind NOTIFY settingsChanged)
    // Why there are no trade searches, empty when there are.
    Q_PROPERTY(QString unavailable READ unavailable NOTIFY unavailableChanged)
    // The most height the window can have on the screen it's on.
    Q_PROPERTY(int maxHeight READ maxHeight NOTIFY maxHeightChanged)

public:
    // The engine, the input and the results have to outlive it.
    ResultsPanel(QQmlEngine *engine, GameInput *input, TradeResults *results, QObject *parent = nullptr);
    ~ResultsPanel() override;

    void setSettings(const OverlaySettings &settings);
    void setUnavailable(const QString &why);

    // Shows it on that screen, or else where the mouse is, or where it last
    // was, and asks for the first listings if it hasn't got them. A search
    // that finds some while it's open shows those.
    void show(const QString &screen = {});
    // For the hotkey.
    void toggle();
    Q_INVOKABLE void close();

    bool isVisible() const;
    // Null until it's first shown, or when it couldn't be made.
    QQuickWindow *window() const { return m_window.get(); }

    TradeResults *results() const { return m_results; }
    double scale() const { return m_settings.scale; }
    bool isColorblind() const { return m_settings.colorblind; }
    QString unavailable() const { return m_unavailable; }
    int maxHeight() const { return m_maxHeight; }
    // The name of the screen it is on, or was last on.
    QString screenName() const { return m_screen; }

Q_SIGNALS:
    void settingsChanged();
    void unavailableChanged();
    void maxHeightChanged();

private:
    bool makeWindow();
    void place();

    QQmlEngine *m_engine;
    GameInput *m_input;
    TradeResults *m_results;
    std::unique_ptr<QQuickWindow> m_window;
    OverlaySettings m_settings;
    QString m_unavailable;
    QString m_screen; // the name of the one it's on, or was last
    int m_maxHeight = 0;
};

} // namespace pou
