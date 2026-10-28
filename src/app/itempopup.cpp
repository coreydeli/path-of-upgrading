// SPDX-License-Identifier: MIT
#include "app/itempopup.h"

#include "core/placement.h"
#include "platform/overlay.h"

#include <QGuiApplication>
#include <QQmlComponent>
#include <QQuickWindow>
#include <QScreen>
#include <chrono>

using namespace Qt::Literals::StringLiterals;

namespace pou {
namespace {

// The screen with that name, or else the main one.
QScreen *screenNamed(const QString &name)
{
    const QList<QScreen *> screens = QGuiApplication::screens();
    for (QScreen *screen : screens) {
        if (screen->name() == name)
            return screen;
    }
    return QGuiApplication::primaryScreen();
}

} // namespace

ItemPopup::ItemPopup(QQmlEngine *engine, GameInput *input, QObject *parent)
    : QObject(parent)
    , m_engine(engine)
    , m_hider(input)
{
    connect(&m_hider, &PopupHider::timeToHide, this, &ItemPopup::hide);
    setSettings({});
}

ItemPopup::~ItemPopup() = default;

void ItemPopup::setSettings(const OverlaySettings &settings)
{
    m_settings = settings;
    PopupHider::Timeouts timeouts = m_hider.timeouts();
    timeouts.shown = std::chrono::seconds(settings.autoHideSeconds);
    m_hider.setTimeouts(timeouts);
    Q_EMIT settingsChanged();
}

void ItemPopup::showReport(const ItemReport &report, const std::optional<ScreenPoint> &mouse)
{
    m_report = report;
    m_message.clear();
    setExpanded(false);
    setTrade({});
    setWearError({});
    Q_EMIT shownChanged();
    show(mouse);
}

void ItemPopup::updateReport(const ItemReport &report)
{
    if (!m_message.isEmpty())
        return;
    m_report = report;
    Q_EMIT shownChanged();
}

void ItemPopup::showMessage(const QString &message, const std::optional<ScreenPoint> &mouse)
{
    m_report = {};
    m_message = message;
    setExpanded(false);
    setTrade({});
    setWearError({});
    Q_EMIT shownChanged();
    show(mouse);
}

void ItemPopup::close()
{
    hide();
}

void ItemPopup::setTradeAvailable(bool available)
{
    if (available == m_tradeAvailable)
        return;
    m_tradeAvailable = available;
    Q_EMIT tradeChanged();
}

void ItemPopup::setTrade(const TradeReport &trade)
{
    if (trade == m_trade)
        return;
    m_trade = trade;
    Q_EMIT tradeChanged();
}

void ItemPopup::findOnTrade(TradeFinder::Kind kind)
{
    Q_EMIT tradeAsked(kind);
}

void ItemPopup::wear(const QString &slot)
{
    Q_EMIT wearAsked(slot);
}

void ItemPopup::setWearError(const QString &why)
{
    if (why == m_wearError)
        return;
    m_wearError = why;
    Q_EMIT wearErrorChanged();
}

bool ItemPopup::isVisible() const
{
    return m_window && m_window->isVisible();
}

void ItemPopup::setHovered(bool hovered)
{
    if (hovered == m_hovered)
        return;
    m_hovered = hovered;
    m_hider.setHovered(hovered);
    Q_EMIT hoveredChanged();
}

void ItemPopup::setExpanded(bool expanded)
{
    if (expanded == m_expanded)
        return;
    m_expanded = expanded;
    Q_EMIT expandedChanged();
}

bool ItemPopup::makeWindow()
{
    QQmlComponent component(m_engine, "PathOfUpgrading", "PopupWindow");
    std::unique_ptr<QObject> made(
        component.createWithInitialProperties({{u"popup"_s, QVariant::fromValue(this)}}));
    auto *window = qobject_cast<QQuickWindow *>(made.get());
    if (!window) {
        qWarning("Couldn't make the popup: %s", qPrintable(component.errorString()));
        return false;
    }
    made.release();
    m_window.reset(window);
    makeOverlay(window);
    // it grows as Path of Building answers, and when you open the rest of it
    connect(window, &QWindow::heightChanged, this, [this] {
        if (isVisible())
            place();
    });
    return true;
}

void ItemPopup::show(const std::optional<ScreenPoint> &mouse)
{
    if (!m_window && !makeWindow())
        return;
    m_mouse = mouse;
    place();
    m_window->show();
    const bool byMouse = m_settings.position == PopupPosition::Cursor && mouse;
    m_hider.shown(m_window->screen()->name(), QRect(m_at, m_window->size()), byMouse);
}

void ItemPopup::place()
{
    QScreen *screen = screenNamed(m_mouse ? m_mouse->screen : QString());
    if (!screen)
        return;
    const std::optional<QPoint> mouse = m_mouse ? std::optional(m_mouse->position) : std::nullopt;
    m_at = placePopup(m_settings.position, m_window->size(), screen->size(), mouse);
    moveOverlay(m_window.get(), screen, m_at);
}

void ItemPopup::hide()
{
    m_hider.hidden();
    if (m_window)
        m_window->hide();
    setHovered(false);
}

} // namespace pou
