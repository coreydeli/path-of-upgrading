// SPDX-License-Identifier: MIT
#pragma once

#include "app/itemreport.h"
#include "app/popuphider.h"
#include "core/settings.h"
#include "platform/screenmap.h"

#include <QObject>
#include <QPoint>
#include <QString>
#include <QtQmlIntegration/qqmlintegration.h>
#include <memory>
#include <optional>

class QQmlEngine;
class QQuickWindow;

namespace pou {

class GameInput;

// The popup over the game that says what the item you checked means for you,
// next to the mouse or where your settings put it. Its window is
// PopupWindow.qml.
class ItemPopup : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("The app has the one popup.")

    Q_PROPERTY(pou::ItemReport report READ report NOTIFY shownChanged)
    // Why the hotkey read nothing, shown in place of a report.
    Q_PROPERTY(QString message READ message NOTIFY shownChanged)
    Q_PROPERTY(bool hovered READ isHovered WRITE setHovered NOTIFY hoveredChanged)
    // Whether you opened the rest of it, when it's compact. Each item starts
    // without.
    Q_PROPERTY(bool expanded READ isExpanded WRITE setExpanded NOTIFY expandedChanged)
    Q_PROPERTY(double scale READ scale NOTIFY settingsChanged)
    Q_PROPERTY(bool compact READ isCompact NOTIFY settingsChanged)
    Q_PROPERTY(bool colorblind READ isColorblind NOTIFY settingsChanged)

public:
    // The engine and the input have to outlive the popup.
    ItemPopup(QQmlEngine *engine, GameInput *input, QObject *parent = nullptr);
    ~ItemPopup() override;

    void setSettings(const OverlaySettings &settings);

    // Shows it by the mouse, or at the top of the main screen when it isn't
    // known where the mouse is.
    void showReport(const ItemReport &report, const std::optional<ScreenPoint> &mouse);
    // A newer report on the item it has, as when Path of Building has
    // answered. It stays where it is, or hidden.
    void updateReport(const ItemReport &report);
    void showMessage(const QString &message, const std::optional<ScreenPoint> &mouse);
    Q_INVOKABLE void close();

    bool isVisible() const;
    // Null until it's first shown, or when it couldn't be made.
    QQuickWindow *window() const { return m_window.get(); }

    ItemReport report() const { return m_report; }
    QString message() const { return m_message; }
    bool isHovered() const { return m_hovered; }
    void setHovered(bool hovered);
    bool isExpanded() const { return m_expanded; }
    void setExpanded(bool expanded);
    double scale() const { return m_settings.scale; }
    bool isCompact() const { return m_settings.compact; }
    bool isColorblind() const { return m_settings.colorblind; }

Q_SIGNALS:
    void shownChanged();
    void hoveredChanged();
    void expandedChanged();
    void settingsChanged();

private:
    bool makeWindow();
    void show(const std::optional<ScreenPoint> &mouse);
    void place();
    void hide();

    QQmlEngine *m_engine;
    PopupHider m_hider;
    std::unique_ptr<QQuickWindow> m_window;
    OverlaySettings m_settings;
    ItemReport m_report;
    QString m_message;
    std::optional<ScreenPoint> m_mouse; // where it was shown for
    QPoint m_at;                        // and where it went, on the screen
    bool m_hovered = false;
    bool m_expanded = false;
};

} // namespace pou
